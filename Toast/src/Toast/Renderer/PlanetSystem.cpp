#include "tpch.h"

#include "PlanetSystem.h"

#include "Toast/Scene/Components.h"

#include "Toast/Core/Math/Math.h"

#include "Toast/Utils/FixedThreadPool.h"

#include <chrono>

namespace Toast {

	static inline uint32_t V(uint32_t x, uint32_t y, uint32_t N)
	{
		return y * N + x;       
	}

	void PlanetSystem::Initialize()
	{
		// Setting up Shader Layout
		std::vector<ShaderLayout::ShaderInputElement> planetElements;
		ShaderLayout::ShaderInputElement pos(DXGI_FORMAT_R16G16_UINT, "POSITION", 0);
		pos.mInputClassification = D3D11_INPUT_PER_VERTEX_DATA; 
		planetElements.emplace_back(pos);

		Shader* planetGPassShader = ShaderLibrary::Get("assets/shaders/Planet/PlanetGeometryPass.hlsl");

		ID3D10Blob* vsBlob = planetGPassShader->GetVSRaw();

		sShaderInputLayout = ShaderLayout(planetElements, vsBlob);

		// Setting up Constant Buffers
		sPlanetFrameCBuffer = ConstantBufferLibrary::Load("PlanetFrame", 112, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_VERTEX_SHADER, CBufferBindSlot::PlanetFrame), CBufferBindInfo(D3D11_PIXEL_SHADER, CBufferBindSlot::PlanetFrame) });
		sPlanetFrameCBuffer->Bind();
		sPlanetFrameBuffer.Allocate(sPlanetFrameCBuffer->GetSize());
		sPlanetFrameBuffer.ZeroInitialize();

		sPlanetLevelCBuffer = ConstantBufferLibrary::Load("PlanetLevel", 16, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_VERTEX_SHADER, CBufferBindSlot::PlanetLevel), CBufferBindInfo(D3D11_PIXEL_SHADER, CBufferBindSlot::PlanetLevel) });
		sPlanetLevelCBuffer->Bind();
		sPlanetLevelBuffer.Allocate(sPlanetLevelCBuffer->GetSize());
		sPlanetLevelBuffer.ZeroInitialize();

		sBaseHeightMapTexture = dynamic_cast<Texture2D*>(TextureLibrary::Get("assets/textures/Checkerboard.png"));

		sStarFieldTexture2D = dynamic_cast<Texture2D*>(TextureLibrary::Get("assets/textures/Checkerboard.png"));

		sStarFieldTextureCube = CreateRef<TextureCube>(DXGI_FORMAT_R16G16B16A16_UNORM, DXGI_FORMAT_UNKNOWN, 2048, 2048, D3D11_USAGE_DEFAULT, (D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE |
			D3D11_BIND_UNORDERED_ACCESS |
			D3D11_BIND_RENDER_TARGET), 1, 0, 0);

		sStarFieldTextureCube->CreateUAV(0);
	}

	void PlanetSystem::InitializeLevels()
	{
		TOAST_PROFILE_FUNCTION();

		TOAST_CORE_CRITICAL("Initializing levels Levels!");

		sLevels.assign(sNumLevels, {}); 
	}

	void PlanetSystem::RebuildGrid()
	{
		TOAST_PROFILE_FUNCTION();

		TOAST_CORE_CRITICAL("Rebuilding grid!");

		std::vector<uint16_t> vertices;                 // gx,gy packed as uint16
		std::vector<uint32_t> indices;

		const uint32_t N = sGridSize;     // 257, 513 …
		const uint32_t cells = N - 1;
		const uint32_t w = cells / 4;     // 64, 128 …

		vertices.reserve(N * N * 2);
		indices.reserve((N - 1) * (N - 1) * 6);

		for (uint32_t y = 0; y < N; ++y)
			for (uint32_t x = 0; x < N; ++x)
			{
				vertices.push_back((uint16_t)x);
				vertices.push_back((uint16_t)y);
			}

		auto emit = [&](uint32_t x, uint32_t y)
			{
				uint32_t i0 = y * N + x;
				uint32_t i1 = i0 + 1;
				uint32_t i2 = (y + 1) * N + x;
				uint32_t i3 = i2 + 1;
				indices.insert(indices.end(), { i0,i2,i1,  i1,i2,i3 });
			};

		const uint32_t border = 1;          // ← only one cell

		for (uint32_t y = border; y < cells - border; ++y)
			for (uint32_t x = border; x < cells - border; ++x)
				emit(x, y);

		sGridIndexCount = (uint32_t)indices.size();
		sGridVertexBuffer = CreateRef<VertexBuffer>(vertices.data(), (uint32_t)vertices.size() * sizeof(uint16_t), (uint32_t)vertices.size() / 2, 0, D3D11_USAGE_IMMUTABLE);
		sCenterGridIndexBuffer = CreateRef<IndexBuffer>(indices.data(), sGridIndexCount);

		sValidPlanet = true;
	}

	void PlanetSystem::RebuildRingGridIndices()
	{
		const uint32_t N = sGridSize;          // 257
		const uint32_t cells = N - 1;              // 256
		const uint32_t w = cells / 4;          // 64  (kept for clarity)

		std::vector<uint32_t> idx;
		idx.reserve((cells * cells - (cells - 2 * w) * (cells - 2 * w)) * 6);

		auto emit = [&](uint32_t x, uint32_t y)
			{
				uint32_t i0 = y * N + x;
				uint32_t i1 = i0 + 1;
				uint32_t i2 = (y + 1) * N + x;
				uint32_t i3 = i2 + 1;
				idx.insert(idx.end(), { i0, i2, i1,  i1, i2, i3 });
			};

		const uint32_t outer = 1;                  // strip **one** cell on the outside

		for (uint32_t y = 0; y < cells; ++y)
			for (uint32_t x = 0; x < cells; ++x)
			{
				/* Is this cell in the (old) w-wide ring? */
				bool inRing = (x < w || x >= cells - w ||
					y < w || y >= cells - w);

				/* Is it in the outer-most 1-cell band we now want to skip? */
				bool inOuterEdge = (x < outer || x >= cells - outer ||
					y < outer || y >= cells - outer);

				if (inRing && !inOuterEdge)        // keep all ring cells except the outer rim
					emit(x, y);
			}

		sRingGridIndexCount = static_cast<uint32_t>(idx.size());
		sRingGridIndexBuffer = CreateRef<IndexBuffer>(idx.data(), sRingGridIndexCount);
	}

	void PlanetSystem::RebuildLODEdgeGrid()
	{
		std::vector<uint16_t> vertices;  
		std::vector<uint32_t> indices;

		const uint32_t cells = sGridSize - 1;  
		const uint32_t lenFine = cells;           
		const uint32_t lenCoarse = lenFine / 2 + 1; 

		auto map = [&](uint32_t edge, uint16_t u, uint16_t v) -> std::pair<uint16_t, uint16_t>
			{
				switch (edge)
				{
				case 0: 
					return { u,  v };               
				case 1: 
					return { static_cast<uint16_t>(cells - v), u };
				case 2: 
					return { static_cast<uint16_t>(cells - u), static_cast<uint16_t>(cells - v) };   
				default:
					return { v, static_cast<uint16_t>(cells - u) }; 
				}
			};

		for (uint32_t edge = 0; edge < 4; ++edge)
		{
			const bool flip = (edge == 2 || edge == 3);   // bottom & left need CW→CCW

			const uint32_t vOffset = static_cast<uint32_t>(vertices.size() / 2);

			/* coarse row (outer) : local v = 0  ,  u = 0,2,4,… */
			for (uint32_t k = 0; k < lenCoarse; ++k)
			{
				auto [gx, gy] = map(edge, static_cast<uint16_t>(2 * k), 0);
				vertices.push_back(gx); vertices.push_back(gy);
			}

			for (uint32_t u = 0; u < lenFine; ++u)
			{
				auto [gx, gy] = map(edge, static_cast<uint16_t>(u), 1);
				vertices.push_back(gx); vertices.push_back(gy);
			}

			const uint32_t cBase = vOffset;               // first coarse of this edge
			const uint32_t fBase = vOffset + lenCoarse;   // first fine   of this edge

			for (uint32_t k = 0; k + 1 < lenCoarse; ++k) 
			{
				uint32_t c0 = cBase + k;
				uint32_t c1 = c0 + 1;

				uint32_t f0 = fBase + 2 * k;
				uint32_t f1 = f0 + 1;
				uint32_t f2 = f0 + 2;                     // exists except at last span

				auto pushTri = [&](uint32_t a, uint32_t b, uint32_t c)
					{
						if (flip)  
							indices.insert(indices.end(), { a, c, b }); // flip winding
						else       
							indices.insert(indices.end(), { a, b, c });
					};

				pushTri(f0, f1, c0);         
				pushTri(f1, c0, c1);          
				if (f2 < fBase + lenFine)     
					pushTri(f1, f2, c1);
			}
		}

		const uint32_t vbSize = static_cast<uint32_t>(vertices.size()) * sizeof(uint16_t);
		sLODGridVertexBuffer = CreateRef<VertexBuffer>(vertices.data(), vbSize, static_cast<uint32_t>(vertices.size() / 2), 0, D3D11_USAGE_IMMUTABLE);

		sLODGridIndexBuffer = CreateRef<IndexBuffer>(indices.data(), static_cast<uint32_t>(indices.size()));
		sLODGridIndexCount = static_cast<uint32_t>(indices.size());
	}

	LODDrawInfo PlanetSystem::DetermineActiveLODLevels(const Vector3& camPosPS)
	{
		double camHeight = std::max(0.0, camPosPS.Length() - sRadius); 
		double heightSq = camHeight * camHeight;

		uint32_t first = 0;                               
		while (first + 1 < sNumLevels && heightSq > sDistanceLUT[first])
			++first;                                         

		// How far can the player see
		const double dObserver = std::sqrt(camHeight * (2.0 * sRadius + camHeight));         // camera’s horizon
		const double dPeak = std::sqrt(sMaxHeight * (2.0 * sRadius + sMaxHeight));           // extra for peaks
		double horizon = dObserver + dPeak;

		uint32_t last = first;                              // we already keep it
		double   cell = double(1u << first);                // metres / texel
		double   half = 0.5 * (sGridSize - 1) * cell;       // half-width

		while (half < horizon && last + 1 < sNumLevels)                     // still have rings
		{
			++last;                                          // add next ring
			cell *= 2.0;
			half *= 2.0;
		}

		sActiveLevels.first = first;               // finest level to draw
		sActiveLevels.count = last - first + 1;    // how many in total
		return sActiveLevels;
	}

	void PlanetSystem::UpdateLevelOrigins(const Vector3& camPosPS)
	{
		TOAST_PROFILE_FUNCTION();

		const int halfGrid = static_cast<int>(sGridSize) / 2;

		for (uint32_t L = 0; L < sNumLevels; ++L)
		{
			const int cellSize = 1 << L;

			std::pair<uint32_t, uint32_t> newOrigin;
			newOrigin.first = static_cast<int>(std::floor(camPosPS.x / double(cellSize))) - halfGrid;
			newOrigin.second = static_cast<int>(std::floor(camPosPS.z / double(cellSize))) - halfGrid;

			if (newOrigin != sLevels[L].Origin)
			{
				sLevels[L].Origin = newOrigin;
				sLevels[L].Dirty = true;
			}
			else
				sLevels[L].Dirty = false;
		}
	}

	Buffer& PlanetSystem::BuildLevelCB(uint32_t L)
	{
		TOAST_PROFILE_FUNCTION();

		static PlanetLevelCB cb;                // lives between calls
		const ClipLevel& lvl = sLevels[L];

		cb.OriginX = lvl.Origin.first;
		cb.OriginY = lvl.Origin.second;
		cb.CellSize = 1u << L;                  // 2^L metres
		cb.GridSize = sGridSize;                // e.g. 257

		/* copy to the generic scratch buffer you created
		   when you built  sPlanetLevelCBuffer  */
		sPlanetLevelBuffer.Write(reinterpret_cast<uint8_t*>(&cb), sizeof(cb), 0);

		return sPlanetLevelBuffer;
	}

	void PlanetSystem::OnUpdate(const Vector3& camPosWS, const Vector3& worldTranslation, DirectX::XMMATRIX viewMatrix)
	{
		TOAST_PROFILE_FUNCTION();

		mRotationQuat = Quaternion::FromRollPitchYaw(Math::DegreesToRadians(sRotationEulerAngles.x), Math::DegreesToRadians(sRotationEulerAngles.y), Math::DegreesToRadians(sRotationEulerAngles.z));
		mRotationQuat = Quaternion::Normalize(mRotationQuat);
		mInvRotationQuat = mRotationQuat.Conjugate();

		Vector3 camRel = camPosWS - Vector3(sTranslation) - worldTranslation;
		Vector3 camPosPS = Vector3::Rotate(camRel, mInvRotationQuat);

		PlanetFrameCB cb{};
		Vector3 centreCVd = Vector3(sTranslation);
		cb.Center = DirectX::XMFLOAT3((float)centreCVd.x, (float)centreCVd.y, (float)centreCVd.z);
		cb.Radius = (float)sRadius;
		cb.MaxHeight = (float)sMaxHeight;
		cb.MinHeight = (float)sMinHeight;

		// planet-fixed triad – ONLY the quaternion is involved
		Vector3 lonEastWS = Vector3::Normalize(Vector3::Rotate({ 1,0,0 }, mRotationQuat)); // +longitude
		Vector3 spinUpWS = Vector3::Normalize(Vector3::Rotate({ 0,1,0 }, mRotationQuat)); // spin axis
		Vector3 lonNorthWS = Vector3::Normalize(Vector3::Rotate({ 0,0,1 }, mRotationQuat));

		// camera-dependent radial, kept for lifting the grid
		Vector3 radUpWS = Vector3::Normalize(camPosWS - Vector3(sTranslation));

		// 1.3   project planet-east into the tangent plane → tangent east
		Vector3 tanEastWS = lonEastWS - radUpWS * Vector3::Dot(lonEastWS, radUpWS);
		if (tanEastWS.LengthSquared() < 1e-6f)                      // at planet pole
			tanEastWS = Vector3::Normalize(Vector3::Cross(spinUpWS, radUpWS));
		else
			tanEastWS = Vector3::Normalize(tanEastWS);

		// 1.4   tangent north = radial × tangent-east
		Vector3 tanNorthWS = Vector3::Normalize(Vector3::Cross(radUpWS, tanEastWS));

		// to VIEW space (for the shader math)
		//auto ToView = [&](const DirectX::XMFLOAT3& vWS)
		//	{
		//		DirectX::XMVECTOR v = DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&vWS), viewMatrix);

		//		DirectX::XMFLOAT3 ret;
		//		DirectX::XMStoreFloat3(&ret, v);
		//		return ret;
		//	};

		cb.BasisTanEast = DirectX::XMFLOAT3({ (float)tanEastWS.x, (float)tanEastWS.y, (float)tanEastWS.z });
		cb.BasisTanNorth = DirectX::XMFLOAT3({ (float)tanNorthWS.x, (float)tanNorthWS.y, (float)tanNorthWS.z });
		cb.BasisRadUp =  DirectX::XMFLOAT3({ (float)radUpWS.x, (float)radUpWS.y, (float)radUpWS.z });

		cb.BasisLonEast = DirectX::XMFLOAT3({ (float)lonEastWS.x, (float)lonEastWS.y, (float)lonEastWS.z });
		cb.BasisLonNorth = DirectX::XMFLOAT3({ (float)lonNorthWS.x, (float)lonNorthWS.y, (float)lonNorthWS.z });
		cb.BasisSpinUp = DirectX::XMFLOAT3({ (float)spinUpWS.x, (float)spinUpWS.y, (float)spinUpWS.z });

		sBasisLonEast = cb.BasisLonEast;
		sBasisLonNorth = cb.BasisLonNorth;
		sBasisSpinUp = cb.BasisSpinUp;

		sPlanetFrameBuffer.Write(reinterpret_cast<uint8_t*>(&cb), sizeof(cb), 0);

		sPlanetFrameCBuffer->Map(sPlanetFrameBuffer);

		/* decide how many levels are visible this frame                */
		sActiveLevels = DetermineActiveLODLevels(camPosPS);

		const uint32_t L0 = sActiveLevels.first;
		const uint32_t Ln = L0 + sActiveLevels.count;

		Vector3 camTangent = { Vector3::Dot(camRel, tanEastWS), 0.0, Vector3::Dot(camRel, tanNorthWS) };
		UpdateLevelOrigins(camTangent);

		for (uint32_t L = 0; L < sNumLevels; ++L)
			sLevels[L].InFrustum = (L >= L0 && L < Ln);;
	}

	void PlanetSystem::DetailObjectPlacement(TerrainObjectComponent* objects, Matrix& planetNoScaleTransform)
	{
		//TOAST_PROFILE_FUNCTION();

		//gTerrainObjectsPosition.clear();
		//gTerrainObjectsPosition.reserve(objects->MaxNrOfObjects);

		//const siv::PerlinNoise& perlin = siv::PerlinNoise(static_cast<uint32_t>(19871102));

		//std::vector<PlanetNode*> visible;   // local snapshot
		//{
		//	std::scoped_lock lk(gActiveMutex);   // lock writer side
		//	visible = gVisibleNodes;             // cheap pointer copy
		//}

		//if (!visible.empty())
		//{
		//	for (PlanetNode* node : visible)
		//	{
		//		if (gTerrainObjectsPosition.size() >= static_cast<size_t>(objects->MaxNrOfObjects))
		//			break;

		//		if (node->SubdivisionLevel < objects->SubdivisionActivation)
		//			continue;

		//		if (node->CachedDetailObjectPosition.empty())
		//		{
		//			Vector2 centerUV = (node->A.UV + node->B.UV + node->C.UV) / 3.0;

		//			double noiseValue = perlin.octave2D_01(centerUV.x, centerUV.y, 4);
		//			int stonesInThisTriangle = static_cast<int>(std::round(static_cast<double>(objects->MaxNrOfObjectPerFace) * noiseValue));

		//			if (!(stonesInThisTriangle > 0))
		//				continue;

		//			std::mt19937 rng(PlanetNode::Hasher{}(*node));
		//			std::uniform_real_distribution<double> dist(0.0f, 1.0f);

		//			for (int j = 0; j < stonesInThisTriangle; ++j)
		//			{
		//				// Generate barycentric coordinates deterministically
		//				double u = dist(rng);
		//				double v = dist(rng);
		//				if (u + v > 1.0f) {
		//					u = 1.0f - u;
		//					v = 1.0f - v;
		//				}
		//				float w = 1.0f - u - v;

		//				// Calculate the object's local position
		//				Vector3 objectPosition = node->A.Position * u + node->B.Position * v + node->C.Position * w;
		//				Vector3 objectPositionworldPos = planetNoScaleTransform * objectPosition;

		//				node->CachedDetailObjectPosition.emplace_back(DirectX::XMFLOAT3((float)objectPositionworldPos.x, (float)objectPositionworldPos.y, (float)objectPositionworldPos.z));
		//			}
		//		}

		//		size_t remaining = objects->MaxNrOfObjects - gTerrainObjectsPosition.size();
		//		if (remaining == 0)
		//			break;

		//		if (node->CachedDetailObjectPosition.size() > remaining)
		//			gTerrainObjectsPosition.insert(gTerrainObjectsPosition.end(), node->CachedDetailObjectPosition.begin(), node->CachedDetailObjectPosition.begin() + remaining);
		//		else
		//			gTerrainObjectsPosition.insert(gTerrainObjectsPosition.end(), node->CachedDetailObjectPosition.begin(), node->CachedDetailObjectPosition.end());

		//		if (gTerrainObjectsPosition.size() >= static_cast<size_t>(objects->MaxNrOfObjects))
		//			break;
		//	}
		//}
	}

	void PlanetSystem::Shutdown()
	{
	}

	double PlanetSystem::ComputeCurvatureBias(double desiredSwitchHeight, double radius, double patchWidth, double focalLenPx, double screenErrorPx)
	{
		return desiredSwitchHeight *(8.0 * radius * screenErrorPx) / (patchWidth * patchWidth * focalLenPx);
	}

	void PlanetSystem::GenerateDistanceLUT(uint32_t maxLevels, double planetRadius, float FoVY, uint32_t viewportWidth, double metersPerFirstCell, float screenErrorPx, double spacingBias)
	{
		sDistanceLUT.clear();
		sDistanceLUT.reserve(maxLevels);

		double cell = metersPerFirstCell;                 // texel edge (m)
		double patchWidth = cell * (sGridSize - 1);

		double curvatureBias = ComputeCurvatureBias(10.0, planetRadius, patchWidth, (double(viewportWidth) /	(2.0 * std::tan(FoVY * 0.5f))), screenErrorPx);

		const double focalLenPx = double(viewportWidth) /
			(2.0 * std::tan(FoVY * 0.5f));

		uint32_t fineLevels = 7;
		float    fineError = 8.0f;       // instead of 2 px

		for (uint32_t L = 0; L < maxLevels; ++L)
		{
			float errorPx = (L < fineLevels) ? fineError : screenErrorPx;

			double sagitta = curvatureBias *
				(patchWidth * patchWidth) / (8.0 * planetRadius);

			double d = (sagitta / double(errorPx)) * focalLenPx;
			sDistanceLUT.emplace_back(d * d);

			cell *= 2.0;
			if (L >= 5) cell *= spacingBias;
			patchWidth = cell * (sGridSize - 1);
		}

		sDistanceLUT.back() = std::numeric_limits<double>::max();

		for (auto level : sDistanceLUT)
			TOAST_CORE_INFO("sDistanceLUT: %lf", level);
	}

	void PlanetSystem::GenerateFaceDotLevelLUT(std::vector<double>& faceLevelDotLUT, float planetRadius, float maxHeight)
	{
		//const int MAX_SUBDIVISION = 25;

		//std::lock_guard<std::mutex> lock(planetDataMutex);

		//// 1) the extra angle due to maxHeight above the sphere
		//double cullingAngle = std::acos(planetRadius / (planetRadius + maxHeight));

		//// 2) the “half-angle” of the base icosahedron face (for level 0)
		////    an icosahedron triangle subtends acos(0.5) at the center
		//double faceHalf0 = std::acos(0.5);

		//faceLevelDotLUT.clear();
		//faceLevelDotLUT.reserve(MAX_SUBDIVISION + 1);

		//double angle = faceHalf0;
		//for (int level = 0; level <= MAX_SUBDIVISION; ++level) {
		//	// dot threshold = sin(faceHalfAngle + cullingAngle)
		//	faceLevelDotLUT.push_back(std::sin(angle + cullingAngle));
		//	// next level’s half-angle is half as big
		//	angle *= 0.5;
		//}

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