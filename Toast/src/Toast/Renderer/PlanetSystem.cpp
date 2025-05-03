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

	std::vector<Vector3> PlanetSystem::sBaseVertices;
	std::vector<uint32_t> PlanetSystem::sBaseIndices;
	std::vector<Vertex> PlanetSystem::sBuildVertices;
	std::vector<uint32_t> PlanetSystem::sBuildIndices;
	std::unordered_map<Vertex, size_t, Vertex::Hasher, Vertex::Equal> PlanetSystem::sVertexMap;
	static std::vector<Ref<PlanetNode>> gAllNodes;
	static std::vector<PlanetNode*> gActiveNodes; 
	static std::vector<PlanetNode*> gVisibleNodes;
	static std::vector<PlanetNode*> gWorkQueue;
	static FixedThreadPool gJobPool(4);

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

		u = std::fmod(u + width, width);
		vTex = std::clamp(vTex, 0.0, height - 1.000001);

		return { u, vTex };
	}

	//------------------------------------------------------------------
	//  Bilinear height for 4 texel coords  (uint16 height map)
	//------------------------------------------------------------------
	static __m256d GetHeightBilinearSIMD(const __m256d& u, const __m256d& v, const TerrainData& td)
	{
		alignas(32) double U[4], V[4];   _mm256_store_pd(U, u); _mm256_store_pd(V, v);
		const double* base = td.HeightData.data();
		int pitch = td.RowPitch >> 1;    // row length in uint16
		double H[4];

		for (int i = 0; i < 4; ++i)
		{
			int x1 = int(U[i]);  int y1 = int(V[i]);
			int x2 = (x1 + 1) % td.Width;
			int y2 = std::min<int>(y1 + 1, td.Height - 1);

			double fx = U[i] - x1, fy = V[i] - y1;

			double Q11 = base[y1 * pitch + x1];
			double Q21 = base[y1 * pitch + x2];
			double Q12 = base[y2 * pitch + x1];
			double Q22 = base[y2 * pitch + x2];

			double R1 = Q11 * (1.0 - fx) + Q21 * fx;
			double R2 = Q12 * (1.0 - fx) + Q22 * fx;
			H[i] = R1 * (1.0 - fy) + R2 * fy;
		}

		return _mm256_load_pd(H);        // packed doubles
	}

	static CPUVertex BuildCPUVertex(const Vector3& srcPos, const PlanetComponent& planet)
	{
		// 1) normalize
		Vector3AVX2 p(srcPos);
		p = p.Normalised();

		// 2) UV
		Vector3 unit = p.ToVector3();
		Vector2 uv = GetUVFromPosition(unit, double(planet.TerrainData.Width), double(planet.TerrainData.Height));

		// 3) height lookup + radial displacement
		double h = PlanetSystem::GetHeight(uv, planet.TerrainData);
		p = p * (planet.PlanetData.radius + h);

		// 4) assemble
		CPUVertex v;
		v.Position = p.ToVector3();
		v.UV = uv;
		return v;
	}

	static void BuildCPUVertex4(const Vec3x4d& in, CPUVertex* out, const PlanetComponent& planet)
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

		/* 4. scatter results ---------------------------------------------- */
		alignas(32) double hu[4], hv[4], hh[4];
		_mm256_store_pd(hu, U);  _mm256_store_pd(hv, V);  _mm256_store_pd(hh, H);

		for (int i = 0; i < 4; ++i)
		{
			Vector3 dir{ nx[i], ny[i], nz[i] };
			out[i].Position = dir * (planet.PlanetData.radius + hh[i]);
			out[i].UV = { hu[i], hv[i] };
		}
	}

	static void MakeMidVertices(const CPUVertex& A, const CPUVertex& B, const CPUVertex& C, CPUVertex& mAB, CPUVertex& mBC, CPUVertex& mCA, const PlanetComponent& planet)
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
		BuildCPUVertex4(ins, out, planet);

		// 4) scatter back to the three mids
		mAB = out[0];
		mBC = out[1];
		mCA = out[2];
		// out[3] is a duplicate of out[2], ignore it
	}

	void SplitNode(PlanetNode* n, const PlanetComponent& planet) 
	{
		if (!n->ChildNodes.empty()) return;  // already split

		// get the three mid vertices
		CPUVertex mAB, mBC, mCA;
		MakeMidVertices(n->A, n->B, n->C, mAB, mBC, mCA, planet);

		int nextL = n->SubdivisionLevel + 1;
		n->ChildNodes.resize(4);
		n->ChildNodes[0] = CreateRef<PlanetNode>(mAB, mBC, mCA, nextL);
		n->ChildNodes[1] = CreateRef<PlanetNode>(mBC, mCA, n->A, nextL);
		n->ChildNodes[2] = CreateRef<PlanetNode>(n->B, mAB, mBC, nextL);
		n->ChildNodes[3] = CreateRef<PlanetNode>(mCA, mAB, n->C, nextL);

		for (auto& c : n->ChildNodes)
			c->parent = n;
	}

	void CollapseNode(PlanetNode* n)
	{
		if (n->ChildNodes.empty()) return;
		// drop only the immediate children:
		n->ChildNodes.clear();
	}

	uint32_t PlanetSystem::HashFace(uint32_t index0, uint32_t index1, uint32_t index2)
	{
		// Simple hash combining indices; you can make this more complex as needed
		return static_cast<uint32_t>(index0 * 73856093 ^ index1 * 19349663 ^ index2 * 83492791);
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

	void PlanetSystem::UpdatePlanetLOD(PlanetComponent& planet, const Vector3& camPlanetSpace, const Vector3& planetCenter, Matrix& planetNoScaleTransform)
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
					double dist2 = (n->center - camPlanetSpace).LengthSquared();

					if (NeedSplit(n->SubdivisionLevel, dist2, planet))
						n->state = PlanetNode::State::WantSplit;
					else if (NeedCollapse(n->SubdivisionLevel, dist2, planet))
						n->state = PlanetNode::State::WantCollapse;
					else
						n->state = PlanetNode::State::ActiveLeaf;
				}
			});


		// Apply the split, collapse or keep to build the updated active leaves list
		std::vector<PlanetNode*> newLeaves;
		newLeaves.reserve(gActiveNodes.size() * 1.2);

		for (PlanetNode* n : gActiveNodes)
		{
			switch (n->state)
			{
			case PlanetNode::State::WantSplit:
				SplitNode(n, planet);
				for (auto& c : n->ChildNodes)
					newLeaves.push_back(c.get());
				break;

			case PlanetNode::State::WantCollapse:
				CollapseNode(n);
				newLeaves.push_back(n);
				break;

			case PlanetNode::State::ActiveLeaf:
				newLeaves.push_back(n);
				break;

			default:
				// no culling here — we ignore Culled state
				break;
			}
		}

		// Swap the new leaves into the active leaves list
		{
			std::scoped_lock lock(gActiveMutex);
			gActiveNodes.swap(newLeaves);
		}
	}

	void PlanetSystem::ComputeVisibleNodes(const PlanetComponent& planet, const Vector3& camPlanetSpace, const Vector3& planetCenter, bool backfaceCull)
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

		const size_t CHUNK = 512;
		size_t numTasks = (N + CHUNK - 1) / CHUNK;

		std::vector<std::vector<PlanetNode*>> tls(numTasks);

		gJobPool.parallelFor(numTasks, [&](size_t task){
			size_t begin = task * CHUNK;
			size_t end = std::min(begin + CHUNK, N);
			auto& local = tls[task];
			local.reserve(end - begin);

			const double* dotThresh = planet.FaceLevelDotLUT.data();
			for (size_t i = begin; i < end; ++i)
			{
				PlanetNode* n = gActiveNodes[i];

				// back-face test
				if (!backfaceCull)
					local.push_back(n);
				else
				{
					double dp = Vector3::Dot(Vector3::Normalize(n->center), Vector3::Normalize(n->center - camPlanetSpace));

					if (dp < dotThresh[n->SubdivisionLevel])
						local.push_back(n);
				}
			}
			});

		// merge visible nodes from the different threads under lock
		{
			std::scoped_lock lk(gActiveMutex);
			size_t total = 0;
			for (auto& v : tls) 
				total += v.size();

			gVisibleNodes.clear();
			gVisibleNodes.reserve(total);

			for (auto& v : tls)
				for (auto* n : v)
					gVisibleNodes.emplace_back(n);
		}
	}

	void PlanetSystem::RebuildPlanetMesh(PlanetComponent& planet, Matrix& planetNoScaleTransform)
	{
		TOAST_PROFILE_FUNCTION();

		// Clear the old build data.
		sBuildVertices.clear();
		sBuildIndices.clear();
		sVertexMap.clear();

		// 3) Helper to add‐or‐reuse a vertex when doing smooth shading
		auto addVertex = [&](const CPUVertex& cpuV, const Vector3 worldPos) -> size_t {
			Vertex v;
			v.Position = { (float)worldPos.x, (float)worldPos.y, (float)worldPos.z };
			v.Texcoord = { (float)cpuV.UV.x,  (float)cpuV.UV.y };
			v.Normal = { 0, 0, 0 };
			v.Tangent = { 0, 0, 0, 0 };
			v.Color = { 0, 0, 0 };

			auto result = sVertexMap.emplace(v, sBuildVertices.size());
			if (result.second) {
				sBuildVertices.emplace_back(v);
			}
			return result.first->second;
			};

		// Loop through all visible nodes to but them back into the build vertices and indices.
		for (PlanetNode* n : gVisibleNodes)
		{
			CPUVertex& A = n->A, & B = n->B, & C = n->C;

			Vector3 worldPosA = planetNoScaleTransform * A.Position;
			Vector3 worldPosB = planetNoScaleTransform * B.Position;
			Vector3 worldPosC = planetNoScaleTransform * C.Position;

			// load positions into SIMD vectors (w lane = 1.0 by default)
			Vector3AVX2 sA(worldPosA.x, worldPosA.y, worldPosA.z);
			Vector3AVX2 sB(worldPosB.x, worldPosB.y, worldPosB.z);
			Vector3AVX2 sC(worldPosC.x, worldPosC.y, worldPosC.z);

			// cross + normalize
			Vector3AVX2 sn = Vector3AVX2::Cross(sB - sA, sC - sA).Normalised();

			// extract back to scalar Vector3
			Vector3 faceN = sn.ToVector3();

			if (planet.PlanetData.smoothShading)
			{
				size_t iA = addVertex(A, worldPosA);
				size_t iB = addVertex(B, worldPosB);
				size_t iC = addVertex(C, worldPosC);

				// accumulate into each shared vertex
				sBuildVertices[iA].Normal.x += (float)faceN.x;
				sBuildVertices[iA].Normal.y += (float)faceN.y;
				sBuildVertices[iA].Normal.z += (float)faceN.z;

				sBuildVertices[iB].Normal.x += (float)faceN.x;
				sBuildVertices[iB].Normal.y += (float)faceN.y;
				sBuildVertices[iB].Normal.z += (float)faceN.z;

				sBuildVertices[iC].Normal.x += (float)faceN.x;
				sBuildVertices[iC].Normal.y += (float)faceN.y;
				sBuildVertices[iC].Normal.z += (float)faceN.z;

				// emit one triangle
				sBuildIndices.emplace_back(iA);
				sBuildIndices.emplace_back(iB);
				sBuildIndices.emplace_back(iC);
			}
			else
			{
				// flat shading: unique vertex per corner
				Vertex vA(A.Position, A.UV, faceN);
				sBuildVertices.emplace_back(vA);
				sBuildIndices.emplace_back(sBuildVertices.size() - 1);

				Vertex vB(B.Position, B.UV, faceN);
				sBuildVertices.emplace_back(vB);
				sBuildIndices.emplace_back(sBuildVertices.size() - 1);

				Vertex vC(C.Position, C.UV, faceN);
				sBuildVertices.emplace_back(vC);
				sBuildIndices.emplace_back(sBuildVertices.size() - 1);
			}

			if (planet.PlanetData.smoothShading)
			{
#ifdef __AVX2__
				size_t N = sBuildVertices.size();
				size_t i = 0;

				// SIMD‐accelerate in blocks of 4
				for (; i + 3 < N; i += 4)
				{
					// Gather 4 normals (float→double)
					__m256d nx = _mm256_set_pd(
						(double)sBuildVertices[i + 3].Normal.x,
						(double)sBuildVertices[i + 2].Normal.x,
						(double)sBuildVertices[i + 1].Normal.x,
						(double)sBuildVertices[i + 0].Normal.x
					);
					__m256d ny = _mm256_set_pd(
						(double)sBuildVertices[i + 3].Normal.y,
						(double)sBuildVertices[i + 2].Normal.y,
						(double)sBuildVertices[i + 1].Normal.y,
						(double)sBuildVertices[i + 0].Normal.y
					);
					__m256d nz = _mm256_set_pd(
						(double)sBuildVertices[i + 3].Normal.z,
						(double)sBuildVertices[i + 2].Normal.z,
						(double)sBuildVertices[i + 1].Normal.z,
						(double)sBuildVertices[i + 0].Normal.z
					);

					Vec3x4d batch{ nx, ny, nz };
					Vec3x4d normed = batch.Normalize();

					// Scatter back (double→float)
					alignas(32) double ox[4], oy[4], oz[4];
					_mm256_store_pd(ox, normed.x);
					_mm256_store_pd(oy, normed.y);
					_mm256_store_pd(oz, normed.z);

					for (int k = 0; k < 4; ++k)
					{
						sBuildVertices[i + k].Normal.x = (float)ox[3 - k];
						sBuildVertices[i + k].Normal.y = (float)oy[3 - k];
						sBuildVertices[i + k].Normal.z = (float)oz[3 - k];
					}
				}

				// Scalar tail for any leftover 1–3 verts
				for (; i < N; ++i)
				{
					Vector3 n{
						sBuildVertices[i].Normal.x,
						sBuildVertices[i].Normal.y,
						sBuildVertices[i].Normal.z
					};
					n = Vector3::Normalize(n);
					sBuildVertices[i].Normal = { (float)n.x, (float)n.y, (float)n.z };
				}
#else
				// No AVX2: do it the old way
				for (auto& v : sBuildVertices)
				{
					Vector3 n{ v.Normal.x, v.Normal.y, v.Normal.z };
					n = Vector3::Normalize(n);
					v.Normal = { (float)n.x, (float)n.y, (float)n.z };
				}
#endif
			}
		}
	}

	void PlanetSystem::SubdivideFace(Ref<PlanetNode>& node, CPUVertex& A, CPUVertex& B, CPUVertex& C, Vector3& cameraPosPlanetSpace, PlanetComponent& planet, const Vector3& planetCenter, Matrix& planetTransform, uint16_t subdivision, const siv::PerlinNoise& perlin, TerrainDetailComponent* terrainDetail)
	{
		//double height;
		//NextPlanetFace nextFace;
		//Vector2 uvCoords;
		//Bounds bounds;

		////double aDistance = A.Position.LengthSqrt();
		////double bDistance = B.Position.LengthSqrt();
		////double cDistance = C.Position.LengthSqrt();

		//double aDistance = (A.Position - cameraPosPlanetSpace).LengthSquared();
		//double bDistance = (B.Position - cameraPosPlanetSpace).LengthSquared();
		//double cDistance = (C.Position - cameraPosPlanetSpace).LengthSquared();

		////TOAST_CORE_CRITICAL("SubdivideFace: Subdivision=%d, aDistance=%.2f, bDistance=%.2f, cDistance=%.2f",
		////	subdivision, aDistance, bDistance, cDistance);

		//if (subdivision >= BASE_PLANET_SUBDIVISIONS + planet.Subdivisions)	
		//	nextFace = NextPlanetFace::LEAF;
		//else
		//{
		//	double threshold = planet.DistanceLUT[(uint32_t)subdivision - BASE_PLANET_SUBDIVISIONS];
		//	//TOAST_CORE_CRITICAL("SubdivideFace: Threshold for subdivision %d is %.2f", subdivision, threshold);
		//	if (aDistance < planet.DistanceLUT[(uint32_t)subdivision - BASE_PLANET_SUBDIVISIONS] && bDistance < planet.DistanceLUT[(uint32_t)subdivision - BASE_PLANET_SUBDIVISIONS] && cDistance < planet.DistanceLUT[(uint32_t)subdivision - BASE_PLANET_SUBDIVISIONS])
		//		nextFace = NextPlanetFace::SPLIT;
		//	else 
		//		nextFace = NextPlanetFace::LEAF; // Add triangle due to distance
		//}

		////TOAST_CORE_CRITICAL("SubdivideFace: nextFace = %s", (nextFace == NextPlanetFace::SPLIT ? "SPLIT" : "LEAF"));

		//if (nextFace == NextPlanetFace::SPLIT)
		//{
		//	CPUVertex aMid, bMid, cMid;
		//	double mediumTerrainDetailNoise = 0.0;

		//	aMid.Position = B.Position + ((C.Position - B.Position) * 0.5);
		//	bMid.Position = C.Position + ((A.Position - C.Position) * 0.5);
		//	cMid.Position = A.Position + ((B.Position - A.Position) * 0.5);

		//	auto ComputeVertex = [&](CPUVertex& v) {
		//		Vector3 n = Vector3::Normalize(v.Position);
		//		v.UV = GetUVFromPosition(n, (double)planet.TerrainData.Width, (double)planet.TerrainData.Height);
		//		double mediumTerrainDetailNoise = 0.0;
		//		if (terrainDetail && subdivision > terrainDetail->SubdivisionActivation) {
		//			mediumTerrainDetailNoise = perlin.octave2D_01(v.UV.x * terrainDetail->Frequency, v.UV.y * terrainDetail->Frequency, terrainDetail->Octaves) * terrainDetail->Amplitude;
		//		}
		//		double h = GetHeight(v.UV, planet.TerrainData);
		//		v.Position = n * (planet.PlanetData.radius + h + mediumTerrainDetailNoise);
		//		};

		//	ComputeVertex(aMid);
		//	ComputeVertex(bMid);
		//	ComputeVertex(cMid);

		//	// Create child nodes for the four new faces
		//	node->ChildNodes.clear();
		//	node->ChildNodes.reserve(4);

		//	// For each of the four subdivided triangles, create a new node
		//	// Triangle 1: aMid, bMid, cMid
		//	{
		//		Ref<PlanetNode> child = CreateRef<PlanetNode>(aMid, bMid, cMid, (uint16_t)(subdivision + 1), planetTransform);
		//		SubdivideFace(child, aMid, bMid, cMid, cameraPosPlanetSpace, planet, planetCenter, planetTransform, subdivision + 1, perlin, terrainDetail);
		//		node->ChildNodes.emplace_back(child);
		//	}

		//	// Triangle 2: cMid, bMid, A
		//	{
		//		Ref<PlanetNode> child = CreateRef<PlanetNode>(cMid, bMid, A, (uint16_t)(subdivision + 1), planetTransform);
		//		SubdivideFace(child, cMid, bMid, A, cameraPosPlanetSpace, planet, planetCenter, planetTransform, subdivision + 1, perlin, terrainDetail);
		//		node->ChildNodes.emplace_back(child);
		//	}

		//	// Triangle 3: B, aMid, cMid
		//	{
		//		Ref<PlanetNode> child = CreateRef<PlanetNode>(B, aMid, cMid, (uint16_t)(subdivision + 1), planetTransform);
		//		SubdivideFace(child, B, aMid, cMid, cameraPosPlanetSpace, planet, planetCenter, planetTransform, subdivision + 1, perlin, terrainDetail);
		//		node->ChildNodes.emplace_back(child);
		//	}

		//	// Triangle 4: bMid, aMid, C
		//	{
		//		Ref<PlanetNode> child = CreateRef<PlanetNode>(bMid, aMid, C, (uint16_t)(subdivision + 1), planetTransform);
		//		SubdivideFace(child, bMid, aMid, C, cameraPosPlanetSpace, planet, planetCenter, planetTransform, subdivision + 1, perlin, terrainDetail);
		//		node->ChildNodes.emplace_back(child);
		//	}

		//	// After all children are subdivided
		//	node->UpdateBoundsFromChildren();
		//}
		//else
		//{
		//	//TOAST_CORE_CRITICAL("SubdivideFace: LEAF branch - adding vertices for subdivision %d", subdivision);

		//	bool crackTriangle = false;
		//	CPUVertex closestVertex, furthestVertex, middleVertex;

		//	double closestDistance = (std::min)(aDistance, (std::min)(bDistance, cDistance));
		//	double furthestDistance = (std::max)(aDistance, (std::max)(bDistance, cDistance));
		//	double secondClosestDistance;

		//	if (closestDistance == aDistance)
		//		closestVertex = A;
		//	else if (closestDistance == bDistance)
		//		closestVertex = B;
		//	else
		//		closestVertex = C;

		//	if (furthestDistance == aDistance)
		//		furthestVertex = A;
		//	else if (furthestDistance == bDistance)
		//		furthestVertex = B;
		//	else
		//		furthestVertex = C;

		//	if (closestDistance == aDistance)
		//		secondClosestDistance = (furthestDistance == bDistance) ? cDistance : bDistance;
		//	else if (closestDistance == bDistance)
		//		secondClosestDistance = (furthestDistance == aDistance) ? cDistance : aDistance;
		//	else
		//		secondClosestDistance = (furthestDistance == aDistance) ? bDistance : aDistance;

		//	// Identify middle vertex based on distances
		//	if ((closestDistance != aDistance) && (furthestDistance != aDistance))
		//		middleVertex = A;
		//	else if ((closestDistance != bDistance) && (furthestDistance != bDistance))
		//		middleVertex = B;
		//	else
		//		middleVertex = C;

		//	if(subdivision < (planet.Subdivisions + BASE_PLANET_SUBDIVISIONS))
		//	{
		//		if (closestDistance < planet.DistanceLUT[(uint32_t)subdivision - BASE_PLANET_SUBDIVISIONS] && secondClosestDistance < planet.DistanceLUT[(uint32_t)subdivision - BASE_PLANET_SUBDIVISIONS])
		//			crackTriangle = true;
		//	}

		//	// Function to add or retrieve a vertex
		//	auto addVertex = [&](const CPUVertex& cpuVertex, const Vector3& transformedPos) -> size_t {
		//		// Create a Vertex instance
		//		Vertex v;
		//		v.Position = { (float)transformedPos.x, (float)transformedPos.y, (float)transformedPos.z };
		//		v.Texcoord = { (float)cpuVertex.UV.x, (float)cpuVertex.UV.y };
		//		// Initialize normal to zero; we'll accumulate face normals
		//		v.Normal = { 0.0f, 0.0f, 0.0f };
		//		v.Tangent = { 0.0f, 0.0f, 0.0f, 0.0f };
		//		v.Color = { 0.0f, 0.0f, 0.0f };

		//		// Try to insert the vertex into the map
		//		auto result = planet.VertexMap.emplace(v, planet.BuildVertices.size());
		//		if (result.second) {
		//			// Vertex was not in the map; add it to the vertex list
		//			planet.BuildVertices.emplace_back(v);
		//		}
		//		// Return the index of the vertex
		//		return result.first->second;
		//		};

		//	if (!crackTriangle)
		//	{
		//		Vector3 vecA = planetTransform * A.Position;
		//		Vector3 vecB = planetTransform * B.Position;
		//		Vector3 vecC = planetTransform * C.Position;

		//		Vector3 normal = Vector3::Normalize(Vector3::Cross(vecB - vecA, vecC - vecA));

		//		if (planet.PlanetData.smoothShading)
		//		{
		//			// Add or retrieve vertices
		//			size_t indexA = addVertex(A, vecA);
		//			size_t indexB = addVertex(B, vecB);
		//			size_t indexC = addVertex(C, vecC);

		//			// Accumulate normals
		//			planet.BuildVertices[indexA].Normal.x += (float)normal.x;
		//			planet.BuildVertices[indexA].Normal.y += (float)normal.y;
		//			planet.BuildVertices[indexA].Normal.z += (float)normal.z;

		//			planet.BuildVertices[indexB].Normal.x += (float)normal.x;
		//			planet.BuildVertices[indexB].Normal.y += (float)normal.y;
		//			planet.BuildVertices[indexB].Normal.z += (float)normal.z;

		//			planet.BuildVertices[indexC].Normal.x += (float)normal.x;
		//			planet.BuildVertices[indexC].Normal.y += (float)normal.y;
		//			planet.BuildVertices[indexC].Normal.z += (float)normal.z;

		//			// Add indices
		//			planet.BuildIndices.emplace_back(indexA);
		//			planet.BuildIndices.emplace_back(indexB);
		//			planet.BuildIndices.emplace_back(indexC);
		//		}
		//		else 
		//		{
		//			Vertex vertexA = Vertex(vecA, A.UV, normal);
		//			planet.BuildVertices.emplace_back(vertexA);
		//			planet.BuildIndices.emplace_back(planet.BuildVertices.size() - 1);

		//			Vertex vertexB = Vertex(vecB, B.UV, normal);
		//			planet.BuildVertices.emplace_back(vertexB);
		//			planet.BuildIndices.emplace_back(planet.BuildVertices.size() - 1);

		//			Vertex vertexC = Vertex(vecC, C.UV, normal);
		//			planet.BuildVertices.emplace_back(vertexC);
		//			planet.BuildIndices.emplace_back(planet.BuildVertices.size() - 1);
		//		}

		//		node->ComputeBoundsFromTriangle();

		//		// Chunks are used by the physics engine
		//		//AssignFaceToChunk(vecA, vecB, vecC, planet.TerrainChunks, planetCenter);
		//	}
		//	else
		//	{
		//		double mediumTerrainDetailNoise = 0.0;
		//		// Calculate new vertex	
		//		CPUVertex additionalVertex;
		//		additionalVertex.Position = (closestVertex.Position + middleVertex.Position) * 0.5;
		//		Vector3 additionalVertexNormalized = Vector3::Normalize(additionalVertex.Position);
		//		additionalVertex.UV = GetUVFromPosition(additionalVertexNormalized, (double)planet.TerrainData.Width, (double)planet.TerrainData.Height);
		//		if(terrainDetail && subdivision > terrainDetail->SubdivisionActivation)
		//			mediumTerrainDetailNoise = perlin.octave2D_01(additionalVertex.UV.x * terrainDetail->Frequency, additionalVertex.UV.y * terrainDetail->Frequency, terrainDetail->Octaves) * terrainDetail->Amplitude;
		//		double height = GetHeight(additionalVertex.UV, planet.TerrainData);
		//		additionalVertex.Position = additionalVertexNormalized * (planet.PlanetData.radius + height + mediumTerrainDetailNoise);

		//		Vector3 additionalVertexPos = planetTransform * additionalVertex.Position;
		//		Vector3 closestVertexPos = planetTransform * closestVertex.Position;
		//		Vector3 middleVertexPos = planetTransform * middleVertex.Position;
		//		Vector3 furthestVertexPos = planetTransform * furthestVertex.Position;

		//		// First triangle
		//		Vector3 normal = Vector3::Normalize(Vector3::Cross(additionalVertexPos - closestVertexPos, additionalVertexPos - furthestVertexPos));

		//		if (normal.y < 0.0)
		//			normal = normal * -1.0;

		//		if (planet.PlanetData.smoothShading)
		//		{
		//			// Add or retrieve vertices
		//			size_t indexA = addVertex(A, additionalVertexPos);
		//			size_t indexB = addVertex(B, closestVertexPos);
		//			size_t indexC = addVertex(C, furthestVertexPos);

		//			// Accumulate normals
		//			planet.BuildVertices[indexA].Normal.x += (float)normal.x;
		//			planet.BuildVertices[indexA].Normal.y += (float)normal.y;
		//			planet.BuildVertices[indexA].Normal.z += (float)normal.z;

		//			planet.BuildVertices[indexB].Normal.x += (float)normal.x;
		//			planet.BuildVertices[indexB].Normal.y += (float)normal.y;
		//			planet.BuildVertices[indexB].Normal.z += (float)normal.z;

		//			planet.BuildVertices[indexC].Normal.x += (float)normal.x;
		//			planet.BuildVertices[indexC].Normal.y += (float)normal.y;
		//			planet.BuildVertices[indexC].Normal.z += (float)normal.z;

		//			// Add indices
		//			planet.BuildIndices.emplace_back(indexA);
		//			planet.BuildIndices.emplace_back(indexB);
		//			planet.BuildIndices.emplace_back(indexC);
		//		}
		//		else
		//		{
		//			Vertex vertexA = Vertex(additionalVertexPos, additionalVertex.UV, normal);
		//			planet.BuildVertices.emplace_back(vertexA);
		//			planet.BuildIndices.emplace_back(planet.BuildVertices.size() - 1);

		//			Vertex vertexB = Vertex(closestVertexPos, closestVertex.UV, normal);
		//			planet.BuildVertices.emplace_back(vertexB);
		//			planet.BuildIndices.emplace_back(planet.BuildVertices.size() - 1);

		//			Vertex vertexC = Vertex(furthestVertexPos, furthestVertex.UV, normal);
		//			planet.BuildVertices.emplace_back(vertexC);
		//			planet.BuildIndices.emplace_back(planet.BuildVertices.size() - 1);
		//		}

		//		Ref<PlanetNode> child1 = CreateRef<PlanetNode>(A, B, C, subdivision + 1);
		//		node->ChildNodes.push_back(child1);

		//		//AssignFaceToChunk(additionalVertexPos, closestVertexPos, furthestVertexPos, planet.TerrainChunks, planetCenter);

		//		// Second triangle
		//		normal = Vector3::Normalize(Vector3::Cross(additionalVertexPos - furthestVertexPos, additionalVertexPos - middleVertexPos));
		//		if (normal.y < 0.0)
		//			normal = normal * -1.0;

		//		if (planet.PlanetData.smoothShading)
		//		{
		//			// Add or retrieve vertices
		//			size_t indexA = addVertex(A, additionalVertexPos);
		//			size_t indexB = addVertex(B, furthestVertexPos);
		//			size_t indexC = addVertex(C, middleVertexPos);

		//			// Accumulate normals
		//			planet.BuildVertices[indexA].Normal.x += (float)normal.x;
		//			planet.BuildVertices[indexA].Normal.y += (float)normal.y;
		//			planet.BuildVertices[indexA].Normal.z += (float)normal.z;

		//			planet.BuildVertices[indexB].Normal.x += (float)normal.x;
		//			planet.BuildVertices[indexB].Normal.y += (float)normal.y;
		//			planet.BuildVertices[indexB].Normal.z += (float)normal.z;

		//			planet.BuildVertices[indexC].Normal.x += (float)normal.x;
		//			planet.BuildVertices[indexC].Normal.y += (float)normal.y;
		//			planet.BuildVertices[indexC].Normal.z += (float)normal.z;

		//			// Add indices
		//			planet.BuildIndices.emplace_back(indexA);
		//			planet.BuildIndices.emplace_back(indexB);
		//			planet.BuildIndices.emplace_back(indexC);
		//		}
		//		else
		//		{
		//			Vertex vertexD = Vertex(additionalVertexPos, additionalVertex.UV, normal);
		//			vertexD.Color = { 1.0f, 0.0f, 0.0f };
		//			planet.BuildVertices.emplace_back(vertexD);
		//			planet.BuildIndices.emplace_back(planet.BuildVertices.size() - 1);

		//			Vertex vertexF = Vertex(furthestVertexPos, furthestVertex.UV, normal);
		//			planet.BuildVertices.emplace_back(vertexF);
		//			planet.BuildIndices.emplace_back(planet.BuildVertices.size() - 1);

		//			Vertex vertexE = Vertex(middleVertexPos, middleVertex.UV, normal);
		//			planet.BuildVertices.emplace_back(vertexE);
		//			planet.BuildIndices.emplace_back(planet.BuildVertices.size() - 1);
		//		}

		//		Ref<PlanetNode> child2 = CreateRef<PlanetNode>(A, B, C, subdivision + 1);
		//		node->ChildNodes.push_back(child2);

		//		//TOAST_CORE_CRITICAL("Planet vertices count after adding face: %zu", planet.BuildVertices.size());

		//		//AssignFaceToChunk(additionalVertexPos, furthestVertexPos, middleVertexPos, planet.TerrainChunks, planetCenter);
		//	}
		//
		//	return;
		//}
	}

	void PlanetSystem::CalculateBasePlanet(PlanetComponent& planet, double scale)
	{
		TOAST_PROFILE_FUNCTION();

		double ratio = ((1.0 + sqrt(5.0)) / 2.0);

		sBaseVertices = std::vector<Vector3>{
			Vector3::Normalize({ ratio, 0.0, -1.0 })* scale,
			Vector3::Normalize({ -ratio, 0.0, -1.0 })* scale,
			Vector3::Normalize({ ratio, 0.0, 1.0 })* scale,
			Vector3::Normalize({ -ratio, 0.0, 1.0 })* scale,
			Vector3::Normalize({ 0.0, -1.0, ratio })* scale,
			Vector3::Normalize({ 0.0, -1.0, -ratio })* scale,
			Vector3::Normalize({ 0.0, 1.0, ratio })* scale,
			Vector3::Normalize({ 0.0, 1.0, -ratio })* scale,
			Vector3::Normalize({ -1.0, ratio, 0.0 })* scale,
			Vector3::Normalize({ -1.0, -ratio, 0.0 })* scale,
			Vector3::Normalize({ 1.0, ratio, 0.0 })* scale,
			Vector3::Normalize({ 1.0, -ratio, 0.0 })* scale
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
			CPUVertex A = BuildCPUVertex(v0, planet);
			CPUVertex B = BuildCPUVertex(v1, planet);
			CPUVertex C = BuildCPUVertex(v2, planet);

			// create the node at level 0
			Ref<PlanetNode> root = CreateRef<PlanetNode>(A, B, C, 0);

			// Here all the nodes ever creates is keept alive
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

	void PlanetSystem::TraverseNode(Ref<PlanetNode>& node, PlanetComponent& planet, Vector3& cameraPosPlanetSpace, const Vector3& planetCenter, bool backfaceCull, bool frustumCullActivated, Ref<Frustum>& frustum, Matrix& planetTransform, const siv::PerlinNoise& perlin, TerrainDetailComponent* terrainDetail)
	{
		//Vector3 center = (node->A.Position + node->B.Position + node->C.Position) / 3.0;
		//Vector3 viewVector = center - cameraPosPlanetSpace;
		//double cameraDistance = viewVector.Length();

		//double dotProduct = Vector3::Dot(Vector3::Normalize(center), Vector3::Normalize(viewVector));

		////TOAST_CORE_CRITICAL("TraverseNode: Node subdivision=%d, cameraDistance=%.2f, dotProduct=%.2f",
		////	node->SubdivisionLevel, cameraDistance, dotProduct);

		//Ref<PlanetNode> nodeWorldSpace = CreateRef<PlanetNode>(*node);

		//nodeWorldSpace->A = planetTransform * node->A.Position;
		//nodeWorldSpace->B = planetTransform * node->B.Position;
		//nodeWorldSpace->C = planetTransform * node->C.Position;
		//planet.PlanetNodesWorldSpace.emplace_back(nodeWorldSpace);

		//double backFaceCullingIgnoreDistance = 50000.0;
		//if (cameraDistance > backFaceCullingIgnoreDistance)
		//{
		//	TOAST_PROFILE_SCOPE("Backface culling test");
		//	std::lock_guard<std::mutex> lock(planetDataMutex);
		//	if (backfaceCull && dotProduct >= planet.FaceLevelDotLUT[(uint32_t)node->SubdivisionLevel])
		//	{
		//		//TOAST_CORE_CRITICAL("TraverseNode: Node culled by backface (subdivision %d, dotProduct=%.2f, threshold=%.2f)",
		//			//node->SubdivisionLevel, dotProduct, planet.FaceLevelDotLUT[(uint32_t)node->SubdivisionLevel]);
		//		return;
		//	}
		//}
		// 
		//if (frustumCullActivated)
		//{
		//	TOAST_PROFILE_SCOPE("Frustum culling test");
		//	auto intersect = frustum->ContainsTriangleVolume(Vector3::Normalize(node->A.Position) * planet.PlanetData.radius, Vector3::Normalize(node->B.Position) * planet.PlanetData.radius, Vector3::Normalize(node->C.Position) * planet.PlanetData.radius, planet.HeightMultLUT[node->SubdivisionLevel]);

		//	if (intersect == VolumeTri::OUTSIDE)
		//	{
		//		//TOAST_CORE_CRITICAL("TraverseNode: Node culled by frustum (subdivision %d)", node->SubdivisionLevel);

		//		return;
		//	}
		//}

		////TOAST_CORE_CRITICAL("node->SubdivisionLevel going to subdivision: %d", node->SubdivisionLevel);

		//if (node->SubdivisionLevel >= BASE_PLANET_SUBDIVISIONS)
		//{
		//	//TOAST_CORE_CRITICAL("TraverseNode: Processing face at subdivision %d", node->SubdivisionLevel);

		//	SubdivideFace(nodeWorldSpace, node->A, node->B, node->C, cameraPosPlanetSpace, planet, planetCenter, planetTransform, BASE_PLANET_SUBDIVISIONS, perlin, terrainDetail);
		//}
		//else 
		//{
		//	for (auto& child : node->ChildNodes)
		//		TraverseNode(child, planet, cameraPosPlanetSpace, planetCenter, backfaceCull, frustumCullActivated, frustum, planetTransform, perlin, terrainDetail);
		//}
	}

	void PlanetSystem::GeneratePlanet(Ref<Frustum>& frustum, DirectX::XMFLOAT3& scale, const Vector3& planetCenter, DirectX::XMMATRIX noScaleTransform, DirectX::XMVECTOR camPos, bool backfaceCull, bool frustumCullActivated,  PlanetComponent& planet, std::unordered_map<std::pair<int, int>, Ref<ShapeBox>, PairHash>& terrainColliders, std::unordered_map<std::pair<int, int>, std::vector<Vector3>, PairHash>& terrainColliderPositions, TerrainDetailComponent* terrainDetail)
	{
		TOAST_PROFILE_FUNCTION();

		auto start = std::chrono::high_resolution_clock::now();

		//TOAST_CORE_INFO("Planet build started on planet thread");

		planetGenerationOngoing.store(true);

		siv::PerlinNoise perlin;

		if (terrainDetail)
			perlin = siv::PerlinNoise(static_cast<uint32_t>(terrainDetail->Seed));

		int triangleAdded = 0;

		Matrix planetTransform = { noScaleTransform };
		Vector3 cameraPos = { camPos };

		Vector3 cameraPosPlanetSpace = Matrix::Inverse(planetTransform) * cameraPos;

		//cameraPosPlanetSpace.ToString("Camera pos in planet space: ");
		
		{
			std::lock_guard<std::mutex> lock(planetDataMutex);

			sVertexMap.clear();
			sBuildVertices.clear();
			sBuildIndices.clear();

			planet.PlanetNodesWorldSpace.clear();

			planet.TerrainChunks.clear();
		}

		{
			std::lock_guard<std::mutex> lock(terrainCollidersMutex);
			terrainColliders.clear();
			terrainColliderPositions.clear();
		}

		{
			TOAST_PROFILE_SCOPE("Looping through the tree structure!");

			for (auto& node : gBaseNodes)
				TraverseNode(node, planet, cameraPosPlanetSpace, planetCenter, backfaceCull, frustumCullActivated, frustum, planetTransform, perlin, terrainDetail);

			for (auto& vertex : sBuildVertices) {
				Vector3 normal(vertex.Normal.x, vertex.Normal.y, vertex.Normal.z);
				normal = Vector3::Normalize(normal);
				vertex.Normal = { (float)normal.x, (float)normal.y, (float)normal.z };
			}
		}

		//for (const auto& chunkEntry : planet.TerrainChunks)
		//{
		//	const auto& chunkKey = chunkEntry.first;
		//	const auto& verticesInChunk = chunkEntry.second;

		//	if (verticesInChunk.empty()) 
		//		continue;

		//	{
		//		std::lock_guard<std::mutex> lock(terrainCollidersMutex);
		//		terrainColliderPositions[chunkKey].insert(terrainColliderPositions[chunkKey].end(), verticesInChunk.begin(), verticesInChunk.end());
		//	}

		//	Bounds chunkBounds;
		//	GetVerticesBounds(verticesInChunk, chunkBounds);

		//	// Create a collider for the chunk
		//	Ref<ShapeBox> collider = CreateRef<ShapeBox>();
		//	collider->SetBounds(chunkBounds);

		//	// Add the collider to the list
		//	terrainColliders[chunkKey] = collider;
		//}

		newPlanetReady.store(true);
		planetGenerationOngoing.store(false);

		if (sBuildVertices.size() == 0)
			TOAST_CORE_CRITICAL("Empty planet!!");

		// Stop timing
		auto end = std::chrono::high_resolution_clock::now();

		// Calculate the duration
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

		//TOAST_CORE_INFO("Planet created with %d number of vertices and %d number indices, time: %dms", planet.BuildVertices.size(), planet.BuildIndices.size(), duration.count());

		return;
	}

	void PlanetSystem::RegeneratePlanet(Ref<Frustum>& frustum, DirectX::XMFLOAT3& scale, const Vector3& planetCenter, DirectX::XMMATRIX noScaleTransform, DirectX::XMVECTOR camPos, bool backfaceCull, bool frustumCullActivated, PlanetComponent& planet, std::unordered_map<std::pair<int, int>, Ref<ShapeBox>, PairHash>& terrainColliders, std::unordered_map<std::pair<int, int>, std::vector<Vector3>, PairHash>& terrainColliderPositions, TerrainDetailComponent* terrainDetail)
	{
		if (generationFuture.valid() && generationFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
			return;

		if (!planetGenerationOngoing.exchange(true))
		{
			// **POD copies** of only the data we actually need on the worker thread:
			PlanetComponent* pPtr = &planet;
			Vector3 planetCenterCopy = planetCenter;

			// pack XMMATRIX/XMVECTOR into unaligned floats
			DirectX::XMFLOAT4X4 matCopy;
			DirectX::XMStoreFloat4x4(&matCopy, noScaleTransform);
			DirectX::XMFLOAT4   vecCopy;
			DirectX::XMStoreFloat4(&vecCopy, camPos);

			bool backfaceCullCopy = backfaceCull;

			generationFuture = std::async(std::launch::async,
				[pPtr,
				matCopy,
				vecCopy,
				planetCenterCopy,
				backfaceCullCopy]()
				{
					auto planetTF = DirectX::XMLoadFloat4x4(&matCopy);
					Matrix planetNoScaleTransform = planetTF;
					auto camWV = DirectX::XMLoadFloat4(&vecCopy);
					Vector3 camPS = Matrix::Inverse(planetTF) * Vector3 { camWV };

					UpdatePlanetLOD(*pPtr, camPS, planetCenterCopy, planetNoScaleTransform);

					ComputeVisibleNodes(*pPtr, camPS, planetCenterCopy, backfaceCullCopy);

					RebuildPlanetMesh(*pPtr, planetNoScaleTransform);

					newPlanetReady.store(true);
					planetGenerationOngoing.store(false);
				});

			TOAST_CORE_CRITICAL("Planet ready: active nodes=%d, visible nodes=%d, vertices=%d, indices=%d", gActiveNodes.size(), gVisibleNodes.size(), sBuildVertices.size(), sBuildIndices.size());
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

	double PlanetSystem::GetHeight(Vector2 uvCoords, const TerrainData& terrainData)
	{
		uint32_t x1 = (uint32_t)(uvCoords.x);
		uint32_t y1 = (uint32_t)(uvCoords.y);

		uint32_t x2 = x1 == (terrainData.Width - 1) ? 0 : x1 + 1;
		uint32_t y2 = y1 == (terrainData.Height - 1) ? 0 : y1 + 1;

		double Q11 = static_cast<double>(terrainData.HeightData[y1 * (terrainData.RowPitch / 2) + x1]);
		double Q12 = static_cast<double>(terrainData.HeightData[y2 * (terrainData.RowPitch / 2) + x1]);
		double Q21 = static_cast<double>(terrainData.HeightData[y1 * (terrainData.RowPitch / 2) + x2]);
		double Q22 = static_cast<double>(terrainData.HeightData[y2 * (terrainData.RowPitch / 2) + x2]);

		return Math::BilinearInterpolation(uvCoords, Q11, Q12, Q21, Q22);
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