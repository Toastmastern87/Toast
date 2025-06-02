#include "tpch.h"

#include "PlanetSystem.h"

#include "Toast/Scene/Components.h"

#include "Toast/Core/Math/Math.h"

#include "Toast/Utils/FixedThreadPool.h"

#include <chrono>

namespace Toast {

	std::mutex PlanetSystem::planetDataMutex;
	std::mutex PlanetSystem::terrainCollidersMutex;
	std::future<void> PlanetSystem::generationFuture;
	std::atomic<bool> PlanetSystem::newPlanetReady{ false };
	std::atomic<bool> PlanetSystem::planetGenerationOngoing{ false };

	static std::vector<Ref<PlanetNode>> gBaseNodes;

	static std::mutex gActiveMutex;
	static std::mutex sBuildMutex;
	static std::mutex gNodeLookupMutex;
	static std::mutex sCPUVertMutex;

	std::vector<Vector3> PlanetSystem::sBaseVertices;
	std::vector<uint32_t> PlanetSystem::sBaseIndices;
	std::vector<Vertex> PlanetSystem::sBuildVertices;
	std::vector<uint32_t> PlanetSystem::sBuildIndices;
	std::unordered_map<Vertex, size_t, Vertex::Hasher, Vertex::Equal> PlanetSystem::sVertexMap;
	std::unordered_map<CPUVertex, size_t, CPUVertexHasher, CPUVertexEqual> PlanetSystem::sCPUVertexMap;
	std::vector<CPUVertex> PlanetSystem::sCPUVertices;
	std::unordered_map<PlanetNode, Ref<PlanetNode>,	PlanetNode::Hasher> gNodeLookup;
	static std::vector<Ref<PlanetNode>> gAllNodes;
	static std::vector<PlanetNode*> gActiveNodes; 
	static std::vector<PlanetNode*> gVisibleNodes;
	static std::vector<PlanetNode*> gWorkQueue;
	static FixedThreadPool gJobPool(4);

	static thread_local siv::PerlinNoise gPerlin{ 19871102u };

	inline double smoothstep(double edge0, double edge1, double x)
	{
		double t = std::clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
		return t * t * (3.0 - 2.0 * t);
	}

	static Vector2 GetUVFromPosition(const Vector3 pos, double width, double height)
	{
		TOAST_PROFILE_FUNCTION();

		__m128 v = _mm_setr_ps((float)pos.x, (float)pos.y, (float)pos.z, 0.f);
		__m128 len2 = _mm_dp_ps(v, v, 0x7F);
		__m128 rsqrt = _mm_rsqrt_ps(len2);
		rsqrt = _mm_mul_ps(rsqrt, _mm_sub_ps(_mm_set1_ps(1.5f), _mm_mul_ps(_mm_mul_ps(rsqrt, rsqrt), _mm_mul_ps(len2, _mm_set1_ps(0.5f)))));
		v = _mm_mul_ps(v, rsqrt);

		float x = _mm_cvtss_f32(v);
		float y = _mm_cvtss_f32(_mm_shuffle_ps(v, v, _MM_SHUFFLE(1, 1, 1, 1)));
		float z = _mm_cvtss_f32(_mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 2, 2, 2)));

		// 2. *scalar* trig (exactly as before)
		double theta = std::atan2((double)z, (double)x);   // –π .. π
		double  phi = std::asin((double)y);              // –π/2 .. π/2

		// 3. map to texel space, wrap U
		double u = theta / M_PI * 0.5 + 0.5;
		double vTex = phi / M_PIDIV2 * 0.5 + 0.5;

		u *= (width - 1.0);
		vTex *= (height - 1.0);

		//u = std::fmod(u + width, width);
		//vTex = std::clamp(vTex, 0.0, height - 1.000001);

		return { u, vTex };
	}

	inline size_t RowStride(const TerrainData& td)
	{
		return td.Stride;  
	}

	static size_t AddVertexThreadSafe(const CPUVertex& cpuV,
		const Vector3& worldPos)
	{
		Vertex v;
		v.Position = { (float)worldPos.x, (float)worldPos.y, (float)worldPos.z };
		v.Texcoord = { (float)cpuV.UV.x,  (float)cpuV.UV.y };
		v.Normal = { 0,0,0 };
		v.Tangent = { 0,0,0,0 };
		v.Color = { 0,0,0 };

		std::scoped_lock lk(sBuildMutex);          // tiny critical section
		auto [it, inserted] = PlanetSystem::sVertexMap.emplace(v, PlanetSystem::sBuildVertices.size());

		if (inserted)
			PlanetSystem::sBuildVertices.emplace_back(v);
		return it->second;
	}

	static const CPUVertex& CacheCPUVertex(const CPUVertex& in)
	{
		std::scoped_lock lk(sCPUVertMutex);

		auto [it, inserted] =
			PlanetSystem::sCPUVertexMap.emplace(in, PlanetSystem::sCPUVertices.size());

		if (inserted)                 // first time we see this position
			PlanetSystem::sCPUVertices.emplace_back(in);

		return PlanetSystem::sCPUVertices[it->second];   // cached (or newly added) copy
	}

	//------------------------------------------------------------------
	//  Bilinear height for 4 texel coords  (uint16 height map)
	//------------------------------------------------------------------
	static __m256d GetHeightBilinearSIMD(const __m256d& u, const __m256d& v, const TerrainData& td)
	{
		alignas(32) double U[4], V[4];   _mm256_store_pd(U, u); _mm256_store_pd(V, v);
		const double* base = td.HeightData.data();
		double H[4];

		for (int i = 0; i < 4; ++i)
		{
			int x1 = int(U[i]);  int y1 = int(V[i]);
			int x2 = (x1 + 1) % td.Width;
			int y2 = (y1 + 1) % td.Height;

			double fx = U[i] - x1, fy = V[i] - y1;

			double Q11 = base[y1 * RowStride(td) + x1];
			double Q21 = base[y1 * RowStride(td) + x2];
			double Q12 = base[y2 * RowStride(td) + x1];
			double Q22 = base[y2 * RowStride(td) + x2];

			double R1 = Q11 * (1.0 - fx) + Q21 * fx;
			double R2 = Q12 * (1.0 - fx) + Q22 * fx;
			H[i] = R1 * (1.0 - fy) + R2 * fy;
		}

		return _mm256_load_pd(H);        // packed doubles
	}

	static double GetHeightSIMD(double u, double v,
		const TerrainData& td)
	{
		__m256d U = _mm256_set_pd(0, 0, 0, u);
		__m256d V = _mm256_set_pd(0, 0, 0, v);
		__m256d H = GetHeightBilinearSIMD(U, V, td);
		alignas(32) double hh[4];
		_mm256_store_pd(hh, H);
		return hh[0];       // our value in the lowest lane
	}

	inline double Hill3DLOD(const Vector3& unitDir, int maxOct, double baseFreq, double amp, double planetRadius, int subdivision, double persistence = 0.55)
	{
		int allowedOct = std::min(maxOct, subdivision + 2);

		double f = baseFreq;   // *long* wavelength octave
		double a = amp;

		double h = gPerlin.octave3D(unitDir.x * baseFreq, unitDir.y * baseFreq, unitDir.z * baseFreq, allowedOct, persistence);

		return std::abs(h) * amp;
	}

	static CPUVertex BuildCPUVertex(const Vector3& srcPos, int subdivision, const PlanetComponent& planet, const TerrainDetailComponent* td)
	{
		// 1) normalize
		Vector3AVX2 p(srcPos);
		p = p.Normalised();

		// 2) UV
		Vector3 unit = p.ToVector3();
		Vector2 uv = GetUVFromPosition(unit, double(planet.TerrainData.Width), double(planet.TerrainData.Height));

		// 3) height lookup + radial displacement
		double baseHeight = GetHeightSIMD(uv.x, uv.y, planet.TerrainData);
		double hRolling = 0.0;
		double mask = 0.0;
		double hGravel = 0.0;

		if (td) 
		{
			double hRolling = Hill3DLOD(unit, td->Octaves, td->Frequency, td->Amplitude, planet.PlanetData.radius, subdivision);

			double mask = 1.0 - smoothstep(td->GravelLowThreshold, td->GravelHighThreshold, hRolling);

			double hGravel = 0.7 * std::fabs(gPerlin.octave3D(unit.x * td->GravelFrequency, unit.y * td->GravelFrequency, unit.z * td->GravelFrequency, td->GravelOctaves, td->GravelAmplitude));
		}

		double h = baseHeight + hRolling + mask * hGravel;

		p = p * (planet.PlanetData.radius + h);

		// 4) assemble
		CPUVertex v;
		v.Position = p.ToVector3();
		v.UV = uv;
		return CacheCPUVertex(v);
	}

	static void BuildCPUVertex4(const Vec3x4d& in, CPUVertex* out, int subdivision, const PlanetComponent& planet, const TerrainDetailComponent* terrainDetail)
	{
		// Unpacking the doubles
		alignas(32) double xd[4], yd[4], zd[4];
		_mm256_store_pd(xd, in.x);
		_mm256_store_pd(yd, in.y);
		_mm256_store_pd(zd, in.z);

		/* 1. per-lane normalize (double, scalar ‒ cost is tiny) ------------ */
		double nx[4], ny[4], nz[4], theta[4], phi[4];
		for (int i = 0; i < 4; ++i)
		{
			double lenInv = 1.0 / std::sqrt(xd[i] * xd[i] + yd[i] * yd[i] + zd[i] * zd[i]);
			nx[i] = xd[i] * lenInv;  ny[i] = yd[i] * lenInv;  nz[i] = zd[i] * lenInv;
			theta[i] = std::atan2(nz[i], nx[i]);           // –π..π
			phi[i] = std::asin(ny[i]);                  // –π/2..π/2
		}

		/* 2. map to texel space (doubles) ---------------------------------- */
		__m256d U = _mm256_set_pd((theta[3] / M_PI) * 0.5 + 0.5, (theta[2] / M_PI) * 0.5 + 0.5,	(theta[1] / M_PI) * 0.5 + 0.5, (theta[0] / M_PI) * 0.5 + 0.5);

		__m256d V = _mm256_set_pd((phi[3] / (M_PI / 2)) * 0.5 + 0.5, (phi[2] / (M_PI / 2)) * 0.5 + 0.5, (phi[1] / (M_PI / 2)) * 0.5 + 0.5, (phi[0] / (M_PI / 2)) * 0.5 + 0.5);

		__m256d fW = _mm256_set1_pd(double(planet.TerrainData.Width - 1));
		__m256d fH = _mm256_set1_pd(double(planet.TerrainData.Height - 1));
		U = _mm256_mul_pd(U, fW);
		V = _mm256_mul_pd(V, fH);

		// Getting the height by using Bilinear Interpolation and SIMD
		__m256d H = GetHeightBilinearSIMD(U, V, planet.TerrainData);

		alignas(32) double hu[4], hv[4], hh[4];

		_mm256_store_pd(hu, U);
		_mm256_store_pd(hv, V);
		_mm256_store_pd(hh, H);

		for (int i = 0; i < 4; ++i)
		{
			Vector3 dir{ nx[i], ny[i], nz[i] };
			double extra = 0.0;
			if (terrainDetail)
			{
				const auto& td = *terrainDetail;
				double hRolling = Hill3DLOD(dir, td.Octaves, td.Frequency, td.Amplitude, planet.PlanetData.radius, subdivision);

				double mask = 1.0 - smoothstep(td.GravelLowThreshold, td.GravelHighThreshold, hRolling);

				double hGravel = 0.7 * std::fabs(gPerlin.octave3D(dir.x * td.GravelFrequency, dir.y * td.GravelFrequency, dir.z * td.GravelFrequency, td.GravelOctaves, td.GravelAmplitude));

				extra = hRolling + mask * hGravel;
			}
			hh[i] += extra;
		}

		/* 4. scatter results ---------------------------------------------- */
		for (int i = 0; i < 4; ++i)
		{
			CPUVertex tmp;
			Vector3 dir{ nx[i], ny[i], nz[i] };
			tmp.Position = dir * (planet.PlanetData.radius + hh[i]);
			tmp.UV = { hu[i], hv[i] };
			out[i] = CacheCPUVertex(tmp);
		}
	}

	inline bool NeedsCrackPatch(const PlanetNode* n, const Vector3& camPS, const PlanetComponent& planet)
	{
		/* compute camera‑space distances of the three vertices (sqrt → linear) */
		double dA = (n->A.Position - camPS).LengthSquared();
		double dB = (n->B.Position - camPS).LengthSquared();
		double dC = (n->C.Position - camPS).LengthSquared();

		const int L = n->SubdivisionLevel;
		if (L >= planet.Subdivisions)
			return false;                                    // at max LOD already

		double thresh = planet.DistanceLUT[L]; // Check if 2 of the vertices are in the lower level, that means that this is a edge node and crack is needed.

		/* how many of the three are inside the threshold? */
		int inside = int(dA < thresh) + int(dB < thresh) + int(dC < thresh);

		return inside == 2;                                  // your old rule
	}

	static std::array<PlanetNode*, 2> MakeCrackPatches(const PlanetNode* n,	const PlanetComponent& planet, const TerrainDetailComponent* terrainDetail, const Vector3& camPS, std::vector<Ref<PlanetNode>>& patchKeepAlive)
	{
		/* ---- 1. classify the three vertices by camera distance ------------ */
		struct Vtx { const CPUVertex* v; double d2; };
		Vtx v[3] = {
			{ &n->A, 0 }, { &n->B, 0 }, { &n->C, 0 }
		};

		v[0].d2 = (v[0].v->Position - camPS).LengthSquared();
		v[1].d2 = (v[1].v->Position - camPS).LengthSquared();
		v[2].d2 = (v[2].v->Position - camPS).LengthSquared();

		std::sort(std::begin(v), std::end(v),
			[](const Vtx& a, const Vtx& b) { return a.d2 < b.d2; });
		// v[0] = closest, v[1] = middle, v[2] = furthest

		/* ---- 2. build the mid‑point between closest & middle -------------- */
		Vector3 mp = (v[0].v->Position + v[1].v->Position) * 0.5f;
		Vec3x4d vPack = Vec3x4d::Load(mp, mp, mp, mp);
		CPUVertex tmp[4];
		BuildCPUVertex4(vPack, tmp, n->SubdivisionLevel, planet, terrainDetail);       // tmp[0] has the result
		CPUVertex M = tmp[0];

		/* ---- 3. make the two little faces --------------------------------- */
		auto newNode = [&](const CPUVertex& A, const CPUVertex& B, const CPUVertex& C)
			{
				auto ref = CreateRef<PlanetNode>(A, B, C, n->SubdivisionLevel);
				patchKeepAlive.emplace_back(ref);
				return ref;
			};

		Ref<PlanetNode> p0 = newNode(M, *v[0].v, *v[2].v);   // M‑closest‑furthest
		Ref<PlanetNode> p1 = newNode(M, *v[2].v, *v[1].v);   // M‑furthest‑middle

		return { p0.get(), p1.get() };
	}

	static Ref<PlanetNode> AddOrGetNode(const CPUVertex& A,	const CPUVertex& B,	const CPUVertex& C,	int level)
	{
		PlanetNode proto(A, B, C, level);     // temporary value just for comparison

		std::scoped_lock lock(gNodeLookupMutex);

		auto it = gNodeLookup.find(proto);
		if (it != gNodeLookup.end())
			return it->second;             // already exists → reuse

		// create, register, and hand back a fresh node
		Ref<PlanetNode> fresh = CreateRef<PlanetNode>(A, B, C, level);
		gNodeLookup.emplace(*fresh, fresh); // key = *fresh (value semantics)
		gAllNodes.emplace_back(fresh);      // optional: keep linear list
		return fresh;
	}

	static void MakeMidVertices(const CPUVertex& A, const CPUVertex& B, const CPUVertex& C, CPUVertex& mAB, CPUVertex& mBC, CPUVertex& mCA, int subdivision, const PlanetComponent& planet, const TerrainDetailComponent* terrainDetail)
	{
		// 1) compute the raw mid-positions
		Vector3 pAB = (A.Position + B.Position) * 0.5f;
		Vector3 pBC = (B.Position + C.Position) * 0.5f;
		Vector3 pCA = (C.Position + A.Position) * 0.5f;

		// 2) pack them into a Vec3x4d (four lanes)
		//    we repeat pCA in the last lane, since BuildCPUVertex4 works on 4 lanes
		Vec3x4d ins = Vec3x4d::Load(pAB, pBC, pCA, pCA);

		// 3) call your existing SIMD builder
		CPUVertex out[4];
		BuildCPUVertex4(ins, out, subdivision, planet, terrainDetail);

		// 4) scatter back to the three mids
		mAB = out[0];
		mBC = out[1];
		mCA = out[2];
		// out[3] is a duplicate of out[2], ignore it
	}

	void SplitNode(PlanetNode* n, const PlanetComponent& planet, const TerrainDetailComponent* terrainDetail)
	{
		if (!n->ChildNodes.empty()) return;  // already split

		// get the three mid vertices
		CPUVertex mAB, mBC, mCA;
		MakeMidVertices(n->A, n->B, n->C, mAB, mBC, mCA, n->SubdivisionLevel, planet, terrainDetail);

		int nextL = n->SubdivisionLevel + 1;
		n->ChildNodes.resize(4);
		n->ChildNodes[0] = AddOrGetNode(n->A, mAB, mCA, nextL);
		n->ChildNodes[1] = AddOrGetNode(n->B, mBC, mAB, nextL);
		n->ChildNodes[2] = AddOrGetNode(n->C, mCA, mBC, nextL);
		n->ChildNodes[3] = AddOrGetNode(mAB, mBC, mCA, nextL);

		for (auto& c : n->ChildNodes)
			c->Parent = n;
	}

	std::array<PlanetNode*, 4> CollapseNode(PlanetNode* n)
	{
		std::array<PlanetNode*, 4> out{};
		if (!n || n->ChildNodes.empty()) 
			return out;

		for (size_t i = 0; i < n->ChildNodes.size(); ++i)
			out[i] = n->ChildNodes[i].get();

		n->ChildNodes.clear();
		return out;
	}

	void PlanetSystem::GetFaceBounds(const std::initializer_list<Vector3>& vertices, Bounds& bounds)
	{
		// Check if the list is empty
		if (vertices.size() == 0)
		{
			// Handle the error as needed, e.g., throw an exception or set min/max to zero
			bounds.mins = bounds.maxs = Vector3(0.0f, 0.0f, 0.0f);
			return;
		}

		// Initialize min and max with the first vertex
		auto it = vertices.begin();
		bounds.mins = bounds.maxs = *it;

		// Iterate over the rest of the vertices
		for (++it; it != vertices.end(); ++it)
		{
			const Vector3& vertex = *it;

			// Update min bounds
			if (vertex.x < bounds.mins.x) bounds.mins.x = vertex.x;
			if (vertex.y < bounds.mins.y) bounds.mins.y = vertex.y;
			if (vertex.z < bounds.mins.z) bounds.mins.z = vertex.z;

			// Update max bounds
			if (vertex.x > bounds.maxs.x) bounds.maxs.x = vertex.x;
			if (vertex.y > bounds.maxs.y) bounds.maxs.y = vertex.y;
			if (vertex.z > bounds.maxs.z) bounds.maxs.z = vertex.z;
		}
	}

	void PlanetSystem::UpdateActiveNodes(PlanetComponent& planet, const TerrainDetailComponent* terrainDetails, const Vector3& camPlanetSpace, const Vector3& planetCenter, Matrix& planetNoScaleTransform)
	{
		TOAST_PROFILE_FUNCTION();

		// Copying the current leaves into the work queue to be processed by the planet LOD system.
		gWorkQueue = gActiveNodes;

		// Work through all the leaves and flag them for split, collapse or keep.
		const size_t CHUNK = 512;
		gJobPool.parallelFor((gWorkQueue.size() + CHUNK - 1) / CHUNK,
			[&](size_t task)
			{
				size_t begin = task * CHUNK;
				size_t end = std::min(begin + CHUNK, gWorkQueue.size());

				for (size_t i = begin; i < end; ++i)
				{
					PlanetNode* n = gWorkQueue[i];

					if (NeedSplit(n, planet, camPlanetSpace))
						n->NodeState = PlanetNode::State::WantSplit;
					else if (NeedCollapse(n, planet, camPlanetSpace))
						n->NodeState = PlanetNode::State::WantCollapse;
					else
						n->NodeState = PlanetNode::State::ActiveLeaf;
				}
			});

		// Apply the split, collapse or keep to build the updated active leaves list
		std::vector<PlanetNode*> updatedLeaves;
		updatedLeaves.reserve(gActiveNodes.size() * 1.2);

		std::unordered_set<PlanetNode*> skipSet;

		for (PlanetNode* n : gActiveNodes)
		{
			if (skipSet.count(n)) continue;

			if (n->SubdivisionLevel > planet.Subdivisions)
			{
				while (n->SubdivisionLevel > planet.Subdivisions && n->Parent)
					n = n->Parent;

				auto victims = CollapseNode(n);
				for (auto* v : victims) if (v) 
					skipSet.insert(v);

				n->NodeState = PlanetNode::State::ActiveLeaf;

				updatedLeaves.emplace_back(n);

				continue;
			}

			switch (n->NodeState)
			{
			case PlanetNode::State::WantSplit:
				SplitNode(n, planet, terrainDetails);
				for (auto& c : n->ChildNodes)
					updatedLeaves.push_back(c.get());
				break;
			case PlanetNode::State::WantCollapse:
			{
				if (n->Parent)
				{
					PlanetNode* parent = n->Parent;

					bool allWantCollapse = true;

					for (auto& child : parent->ChildNodes)
					{
						if (child->NodeState != PlanetNode::State::WantCollapse)
						{
							allWantCollapse = false;
							break;
						}
					}

					if (allWantCollapse)
					{
						auto victims = CollapseNode(parent);
						for (auto* v : victims) if (v)
							skipSet.insert(v);

						n->NodeState = PlanetNode::State::ActiveLeaf;
						updatedLeaves.emplace_back(parent);
					}
					else
						updatedLeaves.push_back(n);
				}
				else
					updatedLeaves.push_back(n);

				break;
			}
			case PlanetNode::State::ActiveLeaf:
				updatedLeaves.push_back(n);
				break;
			default:
				// no culling here — we ignore Culled state
				break;
			}
		}

		// Swap the new leaves into the active leaves list
		{
			std::scoped_lock lock(gActiveMutex);
			gActiveNodes.swap(updatedLeaves);
		}
	}

	void PlanetSystem::ComputeVisibleNodes(const PlanetComponent& planet, const TerrainDetailComponent* terrainDetails, const Vector3& camPlanetSpace, const Vector3& planetCenter, bool backfaceCull, bool frustumCull, const Frustum* frustum)
	{
		TOAST_PROFILE_FUNCTION();

		size_t N = gActiveNodes.size();
		if (N == 0)
		{
			// nothing to do when number of active leaves are 0, in theory that should never happen
			TOAST_CORE_WARN("Number of active nodes on the planet is 0!");
			std::scoped_lock lk(gActiveMutex);
			gVisibleNodes.clear();
			return;
		}

		alignas(32) double pNX[8], pNY[8], pNZ[8], pD[8];   // 8 = next power of two
		int planeCount = int(frustumCull ? frustum->mPlanetCheckPlanes.size() : 0);

		for (int i = 0; i < planeCount; ++i) {
			const auto& pl = frustum->mPlanetCheckPlanes[i];
			pNX[i] = pl.Normal.x;
			pNY[i] = pl.Normal.y;
			pNZ[i] = pl.Normal.z;
			pD[i] = pl.D;
		}
		/* pad the remaining lanes so we can always load 4 doubles */
		for (int i = planeCount; i < 8; ++i)
			pNX[i] = pNY[i] = pNZ[i] = pD[i] = 0.0;

		struct ThreadScratch
		{
			std::vector<PlanetNode*>       visible;     // raw ptrs → renderer
			std::vector<Ref<PlanetNode>>   patches;     // keep Ref<>‑ownership
		};

		const size_t CHUNK = 512;
		size_t numTasks = (N + CHUNK - 1) / CHUNK;

		std::vector<ThreadScratch> tls(numTasks);

		gJobPool.parallelFor(numTasks, [&](size_t task)
		{
			size_t begin = task * CHUNK;
			size_t end = std::min(begin + CHUNK, N);
			auto& td = tls[task];

			td.visible.reserve(end - begin);

			const double* dotThresh = planet.FaceLevelDotLUT.data();
			for (size_t i = begin; i < end; ++i)
			{
				PlanetNode* n = gActiveNodes[i];

				// back-face test
				if (backfaceCull)
				{
					double dp = Vector3::Dot(Vector3::Normalize(n->Center), Vector3::Normalize(n->Center - camPlanetSpace));

					if (dp >= dotThresh[n->SubdivisionLevel])
						continue;
				}

				if (frustumCull)
				{
					bool outside = false;

					const double maxHeight = planet.PlanetData.maxAltitude;
					const double radius = n->SphereRadius + maxHeight;

					__m256d cx = _mm256_set1_pd(n->Center.x);
					__m256d cy = _mm256_set1_pd(n->Center.y);
					__m256d cz = _mm256_set1_pd(n->Center.z);
					__m256d R = _mm256_set1_pd(-radius);          // we compare ‘dist < -R’

					/* two batches: planes 0‑3 and planes 4‑7 (only first 6 are valid) */
					for (int batch = 0; batch < 2; ++batch)
					{
						int idx = batch * 4;

						__m256d nx = _mm256_load_pd(pNX + idx);
						__m256d ny = _mm256_load_pd(pNY + idx);
						__m256d nz = _mm256_load_pd(pNZ + idx);
						__m256d  d = _mm256_load_pd(pD + idx);

						/* dist = cx*nx + cy*ny + cz*nz - d  (FMA) */
						__m256d dist = _mm256_mul_pd(cx, nx);              // cx*nx
						dist = _mm256_fmadd_pd(cy, ny, dist);              // + cy*ny
						dist = _mm256_fmadd_pd(cz, nz, dist);              // + cz*nz
						dist = _mm256_sub_pd(dist, d);                     // - d

						/* compare: dist < -radius  ? */
						__m256d cmp = _mm256_cmp_pd(dist, R, _CMP_LT_OQ);
						if (_mm256_movemask_pd(cmp))                       // any plane says ‘outside’
						{
							outside = true;
							break;
						}
					}

					if (outside) 
						continue;     // reject this leaf immediately

					double planetRadius = planet.PlanetData.radius;
					double heightRange = planet.HeightMultLUT[n->SubdivisionLevel];

					auto intersect = frustum->ContainsTriangle(n->A.Position, n->B.Position, n->C.Position);

					if (intersect == VolumeTri::OUTSIDE)
						continue;
				}
				
				if (NeedsCrackPatch(n, camPlanetSpace, planet) && n->SubdivisionLevel > 0)
				{
					auto pp = MakeCrackPatches(n, planet, terrainDetails, camPlanetSpace, td.patches);
					td.visible.emplace_back(pp[0]);
					td.visible.emplace_back(pp[1]);
				}
				else
					td.visible.emplace_back(n);
			}
		});

		// merge visible nodes from the different threads under lock
		static std::vector<Ref<PlanetNode>> sPatchKeepAlive;     // ▼ lifetime bucket

		{
			std::scoped_lock lk(gActiveMutex);

			/* 1. visible list */
			gVisibleNodes.clear();
			size_t totalVis = 0;
			for (auto& t : tls) totalVis += t.visible.size();
			gVisibleNodes.reserve(totalVis);

			for (auto& t : tls)
				gVisibleNodes.insert(gVisibleNodes.end(),
					t.visible.begin(), t.visible.end());

			/* 2. keep patch nodes alive for the whole frame */
			sPatchKeepAlive.clear();
			size_t totalPatches = 0;
			for (auto& t : tls) totalPatches += t.patches.size();
			sPatchKeepAlive.reserve(totalPatches);

			for (auto& t : tls)
				sPatchKeepAlive.insert(sPatchKeepAlive.end(),
					t.patches.begin(), t.patches.end());
		}
	}

	void PlanetSystem::RebuildPlanetMesh(PlanetComponent& planet,
		Matrix& planetNoScaleTf)
	{
		TOAST_PROFILE_FUNCTION();

		/* --------------- clear global build buffers -------------------- */
		sBuildVertices.clear();
		sBuildIndices.clear();
		sVertexMap.clear();

		constexpr size_t CHUNK = 512;
		size_t numTasks = (gVisibleNodes.size() + CHUNK - 1) / CHUNK;

		/* --------------- per‑thread scratch space ---------------------- */
		struct ThreadScratch
		{
			std::vector<uint32_t> localIndices;
		};
		std::vector<ThreadScratch> tls(numTasks);

		/* --------------- first pass: generate verts / indices ---------- */
		gJobPool.parallelFor(numTasks, [&](size_t task)
			{
				size_t begin = task * CHUNK;
				size_t end = std::min(begin + CHUNK, gVisibleNodes.size());
				auto& scratch = tls[task];
				scratch.localIndices.reserve((end - begin) * 3);

				for (size_t i = begin; i < end; ++i)
				{
					PlanetNode* n = gVisibleNodes[i];
					CPUVertex& A = n->A, & B = n->B, & C = n->C;

					Vector3 wpA = planetNoScaleTf * A.Position;
					Vector3 wpB = planetNoScaleTf * B.Position;
					Vector3 wpC = planetNoScaleTf * C.Position;

					/* face normal (SIMD) */
					Vector3AVX2 sA(wpA.x, wpA.y, wpA.z),
						sB(wpB.x, wpB.y, wpB.z),
						sC(wpC.x, wpC.y, wpC.z);
					Vector3AVX2 sn = Vector3AVX2::Cross(sB - sA, sC - sA).Normalised();
					Vector3 faceN = sn.ToVector3();

					if (faceN.y < 0.0)
						faceN = faceN * -1.0;

					if (planet.PlanetData.smoothShading)
					{
						size_t iA = AddVertexThreadSafe(A, wpA);
						size_t iB = AddVertexThreadSafe(B, wpB);
						size_t iC = AddVertexThreadSafe(C, wpC);

						{   /* accumulate normals under mutex */
							std::scoped_lock lk(sBuildMutex);
							sBuildVertices[iA].Normal = { sBuildVertices[iA].Normal.x + (float)faceN.x, sBuildVertices[iA].Normal.y + (float)faceN.y, sBuildVertices[iA].Normal.z + (float)faceN.z };
							sBuildVertices[iB].Normal = { sBuildVertices[iB].Normal.x + (float)faceN.x, sBuildVertices[iB].Normal.y + (float)faceN.y, sBuildVertices[iB].Normal.z + (float)faceN.z };
							sBuildVertices[iC].Normal = { sBuildVertices[iC].Normal.x + (float)faceN.x, sBuildVertices[iC].Normal.y + (float)faceN.y, sBuildVertices[iC].Normal.z + (float)faceN.z };
						}

						scratch.localIndices.emplace_back((uint32_t)iA);
						scratch.localIndices.emplace_back((uint32_t)iB);
						scratch.localIndices.emplace_back((uint32_t)iC);
					}
					else
					{
						Vertex vA(wpA, A.UV, faceN);
						Vertex vB(wpB, B.UV, faceN);
						Vertex vC(wpC, C.UV, faceN);

						std::scoped_lock lk(sBuildMutex);
						sBuildVertices.emplace_back(vA);
						sBuildVertices.emplace_back(vB);
						sBuildVertices.emplace_back(vC);

						uint32_t base = (uint32_t)sBuildVertices.size();
						scratch.localIndices.push_back(base - 3);
						scratch.localIndices.push_back(base - 2);
						scratch.localIndices.push_back(base - 1);
					}
				}
			});

		/* --------------- merge indices from all threads --------------- */
		{
			std::scoped_lock lk(sBuildMutex);
			for (auto& t : tls)
				sBuildIndices.insert(sBuildIndices.end(),
					t.localIndices.begin(), t.localIndices.end());
		}

		/* --------------- second pass: normalise (SIMD, parallel) ------ */
		if (planet.PlanetData.smoothShading)
		{
			size_t N = sBuildVertices.size();
			size_t nTasks = (N + CHUNK - 1) / CHUNK;

			gJobPool.parallelFor(nTasks, [&](size_t task)
				{
					size_t begin = task * CHUNK;
					size_t end = std::min(begin + CHUNK, N);

					for (size_t i = begin; i < end; ++i)
					{
						Vector3 n(sBuildVertices[i].Normal);
						n = Vector3::Normalize(n);
						{
							sBuildVertices[i].Normal = { (float)n.x, (float)n.y, (float)n.z };
						}
					}
				});
		}
	}

	void PlanetSystem::CalculateBasePlanet(PlanetComponent& planet, TerrainDetailComponent* terrainDetail, double scale)
	{
		TOAST_PROFILE_FUNCTION();

		double ratio = ((1.0 + sqrt(5.0)) / 2.0);

		sBaseVertices = std::vector<Vector3>{
			Vector3::Normalize({ ratio, 0.0, -1.0 }) * scale,
			Vector3::Normalize({ -ratio, 0.0, -1.0 }) * scale,
			Vector3::Normalize({ ratio, 0.0, 1.0 }) * scale,
			Vector3::Normalize({ -ratio, 0.0, 1.0 }) * scale,
			Vector3::Normalize({ 0.0, -1.0, ratio }) * scale,
			Vector3::Normalize({ 0.0, -1.0, -ratio }) * scale,
			Vector3::Normalize({ 0.0, 1.0, ratio }) * scale,
			Vector3::Normalize({ 0.0, 1.0, -ratio }) * scale,
			Vector3::Normalize({ -1.0, ratio, 0.0 }) * scale,
			Vector3::Normalize({ -1.0, -ratio, 0.0 }) * scale,
			Vector3::Normalize({ 1.0, ratio, 0.0 }) * scale,
			Vector3::Normalize({ 1.0, -ratio, 0.0 }) * scale
		};

		sBaseIndices = std::vector<uint32_t>{
				1, 3, 8,
				3, 1, 9,
				2, 0, 10,
				0, 2, 11,

				5, 7, 0,
				7, 5, 1,
				6, 4, 2,
				4, 6, 3,

				9, 11, 4,
				11, 9, 5,
				10, 8, 6,
				8, 10, 7,

				7, 1, 8,
				1, 5, 9,
				0, 7, 10,
				5, 0, 11,

				3, 6, 8,
				4, 3, 9,
				6, 2, 10,
				2, 4, 11
		};

		gAllNodes.clear();
		gActiveNodes.clear();

		size_t faceCount = sBaseIndices.size() / 3;
		gAllNodes.reserve(faceCount);
		gActiveNodes.reserve(faceCount);

		for (size_t f = 0; f < faceCount; ++f)
		{
			int idx = int(f) * 3;
			Vector3 v0 = sBaseVertices[sBaseIndices[idx + 0]];
			Vector3 v1 = sBaseVertices[sBaseIndices[idx + 1]];
			Vector3 v2 = sBaseVertices[sBaseIndices[idx + 2]];

			// use your scalar helper for single vertices
			CPUVertex A = BuildCPUVertex(v0, 0, planet, terrainDetail);
			CPUVertex B = BuildCPUVertex(v1, 0, planet, terrainDetail);
			CPUVertex C = BuildCPUVertex(v2, 0, planet, terrainDetail);

			// create the node at level 0
			Ref<PlanetNode> root = CreateRef<PlanetNode>(A, B, C, 0);

			// Here all the nodes ever creates is kept alive
			gAllNodes.push_back(root);

			// Here we keep a raw pointer to the node so that it can be cheaply be split, collapsed and shuffled around
			gActiveNodes.push_back(root.get());
		}
	}

	void PlanetSystem::DetailObjectPlacement(const PlanetComponent& planet, TerrainObjectComponent& objects, DirectX::XMMATRIX noScaleTransform, DirectX::XMVECTOR& camPos)
	{
		std::vector<DirectX::XMFLOAT3> objectPositions;

		//Matrix planetTransform = { noScaleTransform };
		//Vector3 cameraPos = { camPos };

		//const siv::PerlinNoise& perlin = siv::PerlinNoise(static_cast<uint32_t>(19871102));

		//std::vector<Vertex> vertices = planet.RenderMesh->GetVertices();
		//std::vector<uint32_t> indices = planet.RenderMesh->GetIndices();
		//
		//if (indices.size() > 0 && vertices.size() > 0)
		//{
		//	for (int i = 0; i < indices.size() - 2; i += 3)
		//	{
		//		Vector3 A = vertices[indices[i]].Position;
		//		Vector3 B = vertices[indices[i + 1]].Position;
		//		Vector3 C = vertices[indices[i + 2]].Position;

		//		double aDistance = (A - cameraPos).LengthSquared();
		//		double bDistance = (B - cameraPos).LengthSquared();
		//		double cDistance = (C - cameraPos).LengthSquared();

		//		if (aDistance < planet.DistanceLUT[(uint32_t)objects.SubdivisionActivation] && bDistance < planet.DistanceLUT[(uint32_t)objects.SubdivisionActivation] && cDistance < planet.DistanceLUT[(uint32_t)objects.SubdivisionActivation])
		//		{
		//			Vector2 aUV = vertices[indices[i]].Texcoord;
		//			Vector2 bUV = vertices[indices[i + 1]].Texcoord;
		//			Vector2 cUV = vertices[indices[i + 2]].Texcoord;

		//			Vector2 centerUV = (aUV + bUV + cUV) / 3.0;

		//			double noiseValue = perlin.octave2D_01(centerUV.x, centerUV.y, 4);
		//			int stonesInThisTriangle = static_cast<int>(std::round(static_cast<double>(objects.MaxNrOfObjectPerFace) * noiseValue));

		//			if (stonesInThisTriangle > 0)
		//			{
		//				uint32_t seed = HashFace(indices[i], indices[i+1], indices[i+2]);
		//				std::mt19937 rng(seed);
		//				std::uniform_real_distribution<double> dist(0.0f, 1.0f);

		//				for (int j = 0; j < stonesInThisTriangle; ++j) {
		//					// Generate barycentric coordinates deterministically
		//					double u = dist(rng);
		//					double v = dist(rng);
		//					if (u + v > 1.0f) {
		//						u = 1.0f - u;
		//						v = 1.0f - v;
		//					}
		//					float w = 1.0f - u - v;

		//					// Calculate the object's local position
		//					Vector3 objectPosition = A * u + B * v + C * w;

		//					objectPositions.emplace_back(DirectX::XMFLOAT3(objectPosition.x, objectPosition.y, objectPosition.z));
		//				}
		//			}
		//		}
		//	}
		//}

		if(objectPositions.size() > 0)
			objects.MeshObject->SetInstanceData(&objectPositions[0], objectPositions.size() * sizeof(DirectX::XMFLOAT3), objectPositions.size());
	}

	void PlanetSystem::InvalidateAllNodes()
	{
		std::scoped_lock lk(gActiveMutex, gNodeLookupMutex);
		gActiveNodes.clear();
		gVisibleNodes.clear();
		gAllNodes.clear();
		gNodeLookup.clear();
		sCPUVertexMap.clear();
		sCPUVertices.clear();
	}

	void PlanetSystem::RegeneratePlanet(Ref<Frustum>& frustum, DirectX::XMFLOAT3& scale, const Vector3& planetCenter, DirectX::XMMATRIX noScaleTransform, DirectX::XMVECTOR camPos, bool backfaceCull, bool frustumCull, PlanetComponent& planet, std::unordered_map<std::pair<int, int>, Ref<ShapeBox>, PairHash>& terrainColliders, std::unordered_map<std::pair<int, int>, std::vector<Vector3>, PairHash>& terrainColliderPositions, TerrainDetailComponent* terrainDetail)
	{
		if (generationFuture.valid() && generationFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
			return;

		if (terrainDetail && terrainDetail->Generation != planet.BuiltDetailGeneration)
		{
			InvalidateAllNodes();            // toss old LOD tree & caches
			CalculateBasePlanet(planet, terrainDetail, planet.PlanetData.radius);   // rebuild level-0 icosahedron
			planet.BuiltDetailGeneration = terrainDetail->Generation;
		}

		if (!planetGenerationOngoing.exchange(true))
		{
			// **POD copies** of only the data we actually need on the worker thread:
			PlanetComponent* pPtr = &planet;
			TerrainDetailComponent* tdPtr = terrainDetail;
			Vector3 planetCenterCopy = planetCenter;
			Frustum* frustumPtr = frustum.get();

			// pack XMMATRIX/XMVECTOR into unaligned floats
			DirectX::XMFLOAT4X4 matCopy;
			DirectX::XMStoreFloat4x4(&matCopy, noScaleTransform);
			DirectX::XMFLOAT4   vecCopy;
			DirectX::XMStoreFloat4(&vecCopy, camPos);

			sCPUVertexMap.clear();
			sCPUVertices.clear();

			bool backfaceCullCopy = backfaceCull;
			bool frustumCullCopy = frustumCull;

			generationFuture = std::async(std::launch::async,
				[pPtr,
				tdPtr,
				matCopy,
				vecCopy,
				planetCenterCopy,
				backfaceCullCopy,
				frustumCullCopy,
				frustumPtr]()
				{
					auto planetTF = DirectX::XMLoadFloat4x4(&matCopy);
					Matrix planetNoScaleTransform = planetTF;
					auto camWV = DirectX::XMLoadFloat4(&vecCopy);
					Vector3 camPS = Matrix::Inverse(planetTF) * Vector3 { camWV };

					UpdateActiveNodes(*pPtr, tdPtr, camPS, planetCenterCopy, planetNoScaleTransform);

					ComputeVisibleNodes(*pPtr, tdPtr, camPS, planetCenterCopy, backfaceCullCopy, frustumCullCopy, frustumPtr);

					RebuildPlanetMesh(*pPtr, planetNoScaleTransform);

					newPlanetReady.store(true);
					planetGenerationOngoing.store(false);
				});

			//TOAST_CORE_CRITICAL("Planet ready: active nodes=%d, visible nodes=%d, vertices=%d, indices=%d", gActiveNodes.size(), gVisibleNodes.size(), sBuildVertices.size(), sBuildIndices.size());
		}

		return;
	}

	void PlanetSystem::UpdatePlanet(Ref<Mesh>& renderPlanet, TerrainColliderComponent& terrainCollider)
	{
		std::lock_guard<std::mutex> lock(planetDataMutex);
		if (newPlanetReady.load())
		{
			{
				std::lock_guard<std::mutex> lock(terrainCollidersMutex);
				terrainCollider.Colliders = terrainCollider.BuildColliders;
				terrainCollider.ColliderPositions = terrainCollider.BuildColliderPositions;
			}

			renderPlanet->mLODGroups[0]->Vertices = sBuildVertices;
			renderPlanet->mLODGroups[0]->Indices = sBuildIndices;
			renderPlanet->InvalidatePlanet();

			newPlanetReady.store(false);
		}
	}

	void PlanetSystem::Shutdown()
	{
		if (generationFuture.valid()) {
			generationFuture.wait();
		}
	}

	void PlanetSystem::GenerateDistanceLUT(std::vector<double>& distanceLUT, float radius, float FoV, float screenWdth, float screenHeight, double maxPixelError)
	{
		const int MAX_LEVEL = 25;
		// our two anchors:
		const int L_anchor0 = 7;      // we want level 7 at exactly 100 000 m
		const int L_anchor1 = 20;      // we want level 20 at its existing “good” value

		// 1) Compute raw chord‐based LOD distances (before squaring)
		std::vector<double> raw_sq(MAX_LEVEL + 1);
		double f = screenHeight * 0.5 / std::tan(FoV * 0.5);
		for (int ℓ = 0; ℓ <= MAX_LEVEL; ++ℓ) {
			double φ = M_PI / (4.0 * std::pow(2.0, ℓ));
			double edge = 2.0 * radius * std::sin(φ);
			double d = (edge * f) / maxPixelError;
			raw_sq[ℓ] = d * d;
		}

		// 2) Uniformly scale so that raw_sq[MAX_LEVEL] → 25.0 (i.e. √25 = 5 m)
		double scale = 25.0 / raw_sq[MAX_LEVEL];
		std::vector<double> scaled_sq(MAX_LEVEL + 1);
		for (int ℓ = 0; ℓ <= MAX_LEVEL; ++ℓ)
			scaled_sq[ℓ] = raw_sq[ℓ] * scale;

		// 3) Convert to linear distances
		std::vector<double> d_lin(MAX_LEVEL + 1);
		for (int ℓ = 0; ℓ <= MAX_LEVEL; ++ℓ)
			d_lin[ℓ] = std::sqrt(scaled_sq[ℓ]);

		// 4) Define our anchor distances in linear space
		double D0 = 100000.0;    // level 7 → exactly 100 000 m
		double D1 = d_lin[L_anchor1];  // level 20 → keep whatever scaled gave us

		// 5) Solve for A,B in d(ℓ) = A·exp(B·ℓ) passing through (ℓ=D0) and (ℓ=L_anchor1,D1)
		double B = (std::log(D1) - std::log(D0)) / double(L_anchor1 - L_anchor0);
		double A = D0 * std::exp(-B * L_anchor0);

		// 6) Build final squared‐distance LUT, piecewise:
		distanceLUT.resize(MAX_LEVEL + 1);
		for (int ℓ = 0; ℓ <= MAX_LEVEL; ++ℓ) {
			double d;
			if (ℓ < L_anchor0) d = d_lin[ℓ];            // keep coarse scaled
			else if (ℓ <= L_anchor1) d = A * std::exp(B * ℓ); // exponential between
			else                      d = d_lin[ℓ];           // keep fine scaled
			distanceLUT[ℓ] = d * d;
		}

		//int i = 0;
		//for (auto level : distanceLUT)
		//{
		//	i++;
		//	TOAST_CORE_INFO("distanceLUT[%d]: %lf", i, level);
		//}
	}

	void PlanetSystem::GenerateFaceDotLevelLUT(std::vector<double>& faceLevelDotLUT, float planetRadius, float maxHeight)
	{
		const int MAX_SUBDIVISION = 25;

		std::lock_guard<std::mutex> lock(planetDataMutex);

		// 1) the extra angle due to maxHeight above the sphere
		double cullingAngle = std::acos(planetRadius / (planetRadius + maxHeight));

		// 2) the “half-angle” of the base icosahedron face (for level 0)
		//    an icosahedron triangle subtends acos(0.5) at the center
		double faceHalf0 = std::acos(0.5);

		faceLevelDotLUT.clear();
		faceLevelDotLUT.reserve(MAX_SUBDIVISION + 1);

		double angle = faceHalf0;
		for (int level = 0; level <= MAX_SUBDIVISION; ++level) {
			// dot threshold = sin(faceHalfAngle + cullingAngle)
			faceLevelDotLUT.push_back(std::sin(angle + cullingAngle));
			// next level’s half-angle is half as big
			angle *= 0.5;
		}

		//for (auto level : faceLevelDotLUT)
		//	TOAST_CORE_INFO("FacelevelDotLUT: %f", level);
	}

	void PlanetSystem::GenerateHeightMultLUT(std::vector<double>& heightMultLUT, double planetRadius, double maxHeight)
	{
		const int MAX_SUBDIVISION = 25;

		heightMultLUT.clear();

		double ratio = ((1.0 + sqrt(5.0)) / 2.0);

		std::vector<Vector3> vertices = std::vector<Vector3>{
			Vector3::Normalize({ -ratio, 0.0, -1.0 }) * planetRadius,
			Vector3::Normalize({ -ratio, 0.0, 1.0 }) * planetRadius,
			Vector3::Normalize({ -1.0, ratio, 0.0 }) * planetRadius,
		};

		Vector3 a = vertices[0];
		Vector3 b = vertices[1];
		Vector3 c = vertices[2];

		Vector3 center = (a + b + c) / 3.0;

		center *= planetRadius / (center.Length() + maxHeight);
		heightMultLUT.push_back(1.0 / Vector3::Dot(Vector3::Normalize(a), Vector3::Normalize(center)) - 1.0);
		double normMaxHeight = maxHeight / planetRadius;

		for (int i = 1; i <= MAX_SUBDIVISION; i++)
		{
			Vector3 A = b + ((c - b) * 0.5);
			Vector3 B = c + ((a - c) * 0.5);
			c = a + ((b - a) * 0.5);
			a = A * planetRadius / A.Length();
			b = B * planetRadius / B.Length();
			c *= planetRadius / c.Length();
			heightMultLUT.push_back((1.0 / Vector3::Dot(Vector3::Normalize(a), Vector3::Normalize(center)) + normMaxHeight) - 1.0);
		}

		//for (auto level : heightMultLUT)
		//	TOAST_CORE_INFO("heightMultLUT: %lf", level);
	}

	uint32_t PlanetSystem::GetOrAddVector3(std::unordered_map<Vector3, uint32_t, Vector3::Hasher, Vector3::Equal>& vertexMap, const Vector3& vertex, std::vector<Vector3>& vertices)
	{
		TOAST_PROFILE_FUNCTION();

		auto it = vertexMap.find(vertex);
		if (it != vertexMap.end()) {
			return it->second;
		}
		else {
			vertices.emplace_back(vertex);
			uint32_t newIndex = vertices.size() - 1;
			vertexMap[vertex] = newIndex;
			return newIndex;
		}
	}

	void PlanetSystem::AssignFaceToChunk(const Vector3& vecA, const Vector3& vecB, const Vector3& vecC,
		std::unordered_map<std::pair<int, int>, std::vector<Vector3>, PairHash>& chunks,
		const Vector3& planetCenter)
	{
		Vector3 centerPoint = (vecA + vecB + vecC) / 3.0;

		// Compute the direction vector from the planet's center to the vertex
		Vector3 direction = centerPoint - planetCenter;
		Vector3 normalizedDirection = Vector3::Normalize(direction);

		// Convert to spherical coordinates
		double latitude = std::asin(normalizedDirection.y) * (180.0 / M_PI); // Degrees
		double longitude = std::atan2(normalizedDirection.z, normalizedDirection.x) * (180.0 / M_PI);
		if (longitude < 0.0)
			longitude += 360.0;

		// Determine bin indices
		const int NUM_LATITUDE_BINS = 720;   // Adjust as needed
		const int NUM_LONGITUDE_BINS = 1440;  // Adjust as needed

		int latIndex = static_cast<int>((latitude + 90.0) / (180.0 / NUM_LATITUDE_BINS));
		int lonIndex = static_cast<int>(longitude / (360.0 / NUM_LONGITUDE_BINS)); 

		// Clamp indices to valid ranges
		latIndex = (std::min)(latIndex, NUM_LATITUDE_BINS - 1);
		lonIndex = (std::min)(lonIndex, NUM_LONGITUDE_BINS - 1);

		// Create the chunk key
		std::pair<int, int> chunkKey = { latIndex, lonIndex };

		// Add the vertex to the chunk
		chunks[chunkKey].emplace_back(vecA);
		chunks[chunkKey].emplace_back(vecB);
		chunks[chunkKey].emplace_back(vecC);
	}

	void PlanetSystem::GetVerticesBounds(const std::vector<Vector3>& vertices, Bounds& bounds)
	{
		if (vertices.empty())
		{
			bounds = Bounds();
			return;
		}

		// Initialize min and max with the first vertex
		Vector3 min = vertices[0];
		Vector3 max = vertices[0];

		// Iterate over all vertices
		for (const auto& vertex : vertices)
		{
			min.x = (std::min)(min.x, vertex.x);
			min.y = (std::min)(min.y, vertex.y);
			min.z = (std::min)(min.z, vertex.z);

			max.x = (std::max)(max.x, vertex.x);
			max.y = (std::max)(max.y, vertex.y);
			max.z = (std::max)(max.z, vertex.z);
		}

		bounds.mins = min;
		bounds.maxs = max;
	}

}