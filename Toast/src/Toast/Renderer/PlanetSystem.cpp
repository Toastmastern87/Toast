#include "tpch.h"

#include "PlanetSystem.h"

#include "Toast/Scene/Components.h"

#include "Toast/Core/Math/Math.h"

#include "Toast/Physics/PhysicsEngine.h"

#include "Toast/Utils/FixedThreadPool.h"

#include <chrono>

#pragma message("PlanetSystem.cpp is being compiled!")

namespace Toast {

	static const uint32_t LODBORDERSIZE = 12;

	static inline uint32_t V(uint32_t x, uint32_t y, uint32_t N)
	{
		return y * N + x;       
	}

	Planet::Planet()
	{
		//mAlbedoTexture = dynamic_cast<Texture2D*>(TextureLibrary::Get("assets/textures/Checkerboard.png"));
	}

	void Planet::Initialize()
	{
		// Setting up Shader Layout
		std::vector<ShaderLayout::ShaderInputElement> planetElements;
		ShaderLayout::ShaderInputElement pos(DXGI_FORMAT_R16G16_UINT, "POSITION", 0);
		pos.mInputClassification = D3D11_INPUT_PER_VERTEX_DATA; 
		planetElements.emplace_back(pos);

		mIcosphereMesh = CreateRef<PlanetMeshIcosphere>();
		mIcosphereMesh->Init();

		Shader* planetGPassShader = ShaderLibrary::Get("assets/shaders/Planet/PlanetGeometryPass.hlsl");

		ID3D10Blob* vsBlob = planetGPassShader->GetVSRaw();

		mShaderInputLayout = ShaderLayout(planetElements, vsBlob);

		// Setting up Constant Buffers
		mPlanetFrameCBuffer = ConstantBufferLibrary::Load("PlanetFrame", 112, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_VERTEX_SHADER, CBufferBindSlot::PlanetFrame), CBufferBindInfo(D3D11_PIXEL_SHADER, CBufferBindSlot::PlanetFrame), CBufferBindInfo(D3D11_COMPUTE_SHADER, CBufferBindSlot::PlanetFrame) });
		mPlanetFrameCBuffer->Bind();
		mPlanetFrameBuffer.Allocate(mPlanetFrameCBuffer->GetSize());
		mPlanetFrameBuffer.ZeroInitialize();

		mPlanetLevelCBuffer = ConstantBufferLibrary::Load("PlanetLevel", 32, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_VERTEX_SHADER, CBufferBindSlot::PlanetLevel) });
		mPlanetLevelCBuffer->Bind();
		mPlanetLevelBuffer.Allocate(mPlanetLevelCBuffer->GetSize());
		mPlanetLevelBuffer.ZeroInitialize();

		mRenderingSettingsCBuffer = ConstantBufferLibrary::Load("PlanetRenderingSettings", 32, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_PIXEL_SHADER, (CBufferBindSlot)5) });
		mRenderingSettingsCBuffer->Bind();
		mRenderingSettingsBuffer.Allocate(mRenderingSettingsCBuffer->GetSize());
		mRenderingSettingsBuffer.ZeroInitialize();

		mTerrainObjectCBuffer = ConstantBufferLibrary::Load("TerrainObject", 32, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_VERTEX_SHADER, (CBufferBindSlot)13) });
		mTerrainObjectCBuffer->Bind();
		mTerrainObjectBuffer.Allocate(mTerrainObjectCBuffer->GetSize());
		mTerrainObjectBuffer.ZeroInitialize();

		// Create texture for Starfield skybox
		mStarFieldTextureCube = CreateRef<TextureCube>(DXGI_FORMAT_R16G16B16A16_UNORM, DXGI_FORMAT_UNKNOWN, 2048, 2048, D3D11_USAGE_DEFAULT, (D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_RENDER_TARGET), 1, 0, 0);

		// Create textures for Atmospheric Scattering
		mTransmittanceLUT = CreateRef<Texture2D>(DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT, 512, 256, D3D11_USAGE_DEFAULT, (D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS), 1, 0);

		mMultiScatteringLUT = CreateRef<Texture2D>(DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT, 512, 256, D3D11_USAGE_DEFAULT, (D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS), 1, 0);

		mSkyViewLUT = CreateRef<Texture2D>(DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT, 512, 512, D3D11_USAGE_DEFAULT, (D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS), 1, 0);

		mAerielPerspectiveLUT = CreateRef<Texture3D>(DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT, 192, 108, 128, D3D11_USAGE_DEFAULT, (D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS), 0);

		mAPFar = CreateRef<Texture2D>(DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R32_UINT, 1, 1, D3D11_USAGE_DEFAULT,	(D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS), 1, 0);

		mAPNear = CreateRef<Texture2D>(DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R32_UINT, 1, 1, D3D11_USAGE_DEFAULT, (D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS), 1, 0);
	}

	void Planet::InitializeLevels()
	{
		TOAST_PROFILE_FUNCTION();

		TOAST_CORE_CRITICAL("Initializing levels Levels!");

		mLevels.assign(mNumLevels, {}); 
	}

	void Planet::RebuildGrid()
	{
		TOAST_PROFILE_FUNCTION();

		TOAST_CORE_CRITICAL("Rebuilding grid!");

		std::vector<uint16_t> vertices;                 // gx,gy packed as uint16
		std::vector<uint32_t> indices;

		const uint32_t N = mGridSize;     // 257, 513 …
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


		for (uint32_t y = LODBORDERSIZE; y < cells - LODBORDERSIZE; ++y)
			for (uint32_t x = LODBORDERSIZE; x < cells - LODBORDERSIZE; ++x)
				emit(x, y);

		mGridIndexCount = (uint32_t)indices.size();
		mGridVertexBuffer = CreateRef<VertexBuffer>(vertices.data(), (uint32_t)vertices.size() * sizeof(uint16_t), (uint32_t)vertices.size() / 2, 0, D3D11_USAGE_IMMUTABLE);
		mCenterGridIndexBuffer = CreateRef<IndexBuffer>(indices.data(), mGridIndexCount);

		mValidPlanet = true;
	}

	void Planet::RebuildRingGridIndices()
	{
		const uint32_t N = mGridSize;          // 257
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

		for (uint32_t y = 0; y < cells; ++y)
			for (uint32_t x = 0; x < cells; ++x)
			{
				/* Is this cell in the (old) w-wide ring? */
				bool inRing = (x < w || x >= cells - w ||
					y < w || y >= cells - w);

				/* Is it in the outer-most 1-cell band we now want to skip? */
				bool inOuterEdge = (x < LODBORDERSIZE || x >= cells - LODBORDERSIZE ||
					y < LODBORDERSIZE || y >= cells - LODBORDERSIZE);

				if (inRing && !inOuterEdge)        // keep all ring cells except the outer rim
					emit(x, y);
			}

		mRingGridIndexCount = static_cast<uint32_t>(idx.size());
		mRingGridIndexBuffer = CreateRef<IndexBuffer>(idx.data(), mRingGridIndexCount);
	}

	void Planet::RebuildLODEdgeGrid()
	{
		std::vector<uint16_t> vertices;
		std::vector<uint32_t> indices;

		const uint32_t cells = mGridSize - 1;
		const uint32_t lenFine = cells;              // matches your existing code
		const uint32_t lenCoarse = lenFine / 2 + 1;

		auto map = [&](uint32_t edge, uint16_t u, uint16_t v) -> std::pair<uint16_t, uint16_t>
			{
				switch (edge)
				{
				case 0:  return { u,  v };
				case 1:  return { static_cast<uint16_t>(cells - v), u };
				case 2:  return { static_cast<uint16_t>(cells - u), static_cast<uint16_t>(cells - v) };
				default: return { v, static_cast<uint16_t>(cells - u) };
				}
			};

		auto emitCell = [&](uint32_t a0, uint32_t a1, uint32_t b0, uint32_t b1, bool flip)
			{
				// (a0,a1) = row A u,u+1 ; (b0,b1) = row B u,u+1
				if (flip)
					indices.insert(indices.end(), { a0, b0, a1,  a1, b0, b1 });
				else
					indices.insert(indices.end(), { a0, a1, b0,  a1, b1, b0 });
			};

		for (uint32_t edge = 0; edge < 4; ++edge)
		{
			const bool flip = (edge == 2 || edge == 3);

			const uint32_t vOffset = static_cast<uint32_t>(vertices.size() / 2);

			// --- Coarse row (outer), v = 0 : u = 0,2,4,...
			for (uint32_t k = 0; k < lenCoarse; ++k)
			{
				auto [gx, gy] = map(edge, static_cast<uint16_t>(2 * k), 0);
				vertices.push_back(gx); vertices.push_back(gy);
			}

			// --- Fine rows v = 1..EDGE_CELLS (each has lenFine verts)
			for (uint32_t r = 1; r <= LODBORDERSIZE; ++r)
			{
				for (uint32_t u = 0; u < lenFine; ++u)
				{
					auto [gx, gy] = map(edge, static_cast<uint16_t>(u), static_cast<uint16_t>(r));
					vertices.push_back(gx); vertices.push_back(gy);
				}
			}

			const uint32_t cBase = vOffset;
			const uint32_t fBase1 = vOffset + lenCoarse;      // first fine row (v=1)
			auto fineRowBase = [&](uint32_t r /*1..EDGE_CELLS*/) -> uint32_t
				{
					return fBase1 + (r - 1) * lenFine;
				};

			// 1) Stitch coarse (v=0) -> fine row v=1 (same as your current logic)
			for (uint32_t k = 0; k + 1 < lenCoarse; ++k)
			{
				uint32_t c0 = cBase + k;
				uint32_t c1 = c0 + 1;

				uint32_t f0 = fBase1 + 2 * k;
				uint32_t f1 = f0 + 1;
				uint32_t f2 = f0 + 2;

				auto pushTri = [&](uint32_t a, uint32_t b, uint32_t c)
					{
						if (flip) indices.insert(indices.end(), { a, c, b });
						else      indices.insert(indices.end(), { a, b, c });
					};

				pushTri(f0, f1, c0);
				pushTri(f1, c0, c1);
				if (f2 < fBase1 + lenFine)
					pushTri(f1, f2, c1);
			}

			// 2) Fill the remaining band with regular fine quads: (v=1->2), (v=2->3)
			for (uint32_t r = 1; r < LODBORDERSIZE; ++r)
			{
				const uint32_t rowA = fineRowBase(r);
				const uint32_t rowB = fineRowBase(r + 1);

				for (uint32_t u = 0; u + 1 < lenFine; ++u)
				{
					uint32_t a0 = rowA + u;
					uint32_t a1 = a0 + 1;
					uint32_t b0 = rowB + u;
					uint32_t b1 = b0 + 1;

					// same winding convention used elsewhere
					indices.insert(indices.end(), { a0, b0, a1,  a1, b0, b1 });
					if (flip)
					{
						// If you need flip consistency for these quads too, use emitCell() instead
						// and remove the insert above. Keeping explicit here for clarity.
						indices.resize(indices.size() - 6);
						emitCell(a0, a1, b0, b1, true);
					}
				}
			}
		}

		const uint32_t vbSize = static_cast<uint32_t>(vertices.size()) * sizeof(uint16_t);
		mLODGridVertexBuffer = CreateRef<VertexBuffer>(
			vertices.data(), vbSize,
			static_cast<uint32_t>(vertices.size() / 2),
			0, D3D11_USAGE_IMMUTABLE);

		mLODGridIndexBuffer = CreateRef<IndexBuffer>(indices.data(), static_cast<uint32_t>(indices.size()));
		mLODGridIndexCount = static_cast<uint32_t>(indices.size());
	}

	LODDrawInfo Planet::DetermineActiveLODLevels(const Vector3& camPosPS, PhysicsEngine* physicsEngine)
	{
		double rCam;
		Vector3 groundN;
		double altitude = 0.0;

		if (!IsTerrainReady() || physicsEngine == nullptr)
		{
			altitude = 0.0;
		}
		else
		{
			altitude = physicsEngine->GetAltitudeAtWorldPos(camPosPS, rCam, groundN);

			// Hard safety net: never allow NaN/inf to propagate.
			if (!std::isfinite(altitude))
				altitude = 0.0;
			if (altitude < 0.0)
				altitude = 0.0; // optional: depends on whether you allow below-surface camera
		}

		double altitudeSq = altitude * altitude;

		uint32_t first = 0;                               
		while (first + 1 < mNumLevels && altitudeSq > mDistanceLUT[first])
			++first;                                         

		// How far can the player see
		const double dObserver = std::sqrt(altitude * (2.0 * mRadius + altitude));         // camera's horizon
		const double dPeak = std::sqrt(mMaxHeight * (2.0 * mRadius + mMaxHeight));           // extra for peaks
		double horizon = dObserver + dPeak;

		uint32_t last = first;                              // we already keep it
		double   cell = double(1u << first);         
		// meters / texel
		double   half = 0.5 * (mGridSize - 1) * cell;       // half-width

		while (half < horizon && last + 1 < mNumLevels)                     // still have rings
		{
			++last;                                          // add next ring
			cell *= 2.0;
			half *= 2.0;
		}

		mActiveLevels.first = first;               // finest level to draw
		mActiveLevels.count = last - first + 1;    // how many in total
		return mActiveLevels;
	}

	void Planet::UpdateLevelOrigins(const Vector3& camPosPS)
	{
		TOAST_PROFILE_FUNCTION();

		const int halfGrid = static_cast<int>(mGridSize) / 2;

		for (uint32_t L = 0; L < mNumLevels; ++L)
		{
			const int cellSize = 1 << L;

			std::pair<uint32_t, uint32_t> newOrigin;
			newOrigin.first = static_cast<int>(std::floor(camPosPS.x / double(cellSize))) - halfGrid;
			newOrigin.second = static_cast<int>(std::floor(camPosPS.z / double(cellSize))) - halfGrid;

			if (newOrigin != mLevels[L].Origin)
			{
				mLevels[L].Origin = newOrigin;
				mLevels[L].Dirty = true;
			}
			else
				mLevels[L].Dirty = false;
		}
	}

	Buffer& Planet::BuildLevelCB(uint32_t L)
	{
		TOAST_PROFILE_FUNCTION();

		static PlanetLevelCB cb;                // lives between calls
		const int halfGrid = int(mGridSize) / 2;
		const int cellSize = 1 << L;

		const ClipLevel& lvl = mLevels[L];

		cb.OriginX = lvl.Origin.first;
		cb.OriginY = lvl.Origin.second;
		cb.CellSize = cellSize;                  // 2^L meters
		cb.GridSize = mGridSize;                // e.g. 257
		cb.ScatterOriginMetersX = -mShiftEastM;
		cb.ScatterOriginMetersY = -mShiftNorthM;
		cb.FinestCellSize = float(1u << mActiveLevels.first);

		/* copy to the generic scratch buffer you created
		   when you built  sPlanetLevelCBuffer  */
		mPlanetLevelBuffer.Write(reinterpret_cast<uint8_t*>(&cb), sizeof(cb), 0);

		return mPlanetLevelBuffer;
	}

	// During runtime playerCamPosWS and RendererCamPosWS will be the same, but they can differ in the editor when the player camera is detached from the rendering camera. The worldTranslation is used to shift the planet's position in world space, allowing for large world coordinates without precision issues. The viewMatrix is used for culling and LOD calculations, while the editorCameraViewMatrix is used for rendering the planet correctly in the editor viewport.
	void Planet::OnUpdate(Camera* camera, const Quaternion& playerCamRot, const Vector3& playerCamPosWS, const Vector3& renderingCamPosWS, const Vector3& worldTranslation, DirectX::XMMATRIX viewMatrix, PhysicsEngine* physicsEngine, Frustum* frustum, const DirectX::XMFLOAT4X4& renderingCameraViewMatrix, float frustumBias)
	{
		TOAST_PROFILE_FUNCTION();

		Quaternion qCam = playerCamRot;
		qCam = Quaternion::Normalize(qCam);

		Vector3& playerCamForwardWS = camera->GetForwardVectorWS(playerCamRot);
		Vector3& playerCamUpWS = camera->GetUpVectorWS(playerCamRot);
		Vector3& playerCamRightWS = camera->GetRightVectorWS(playerCamRot);

		mRotationQuat = Quaternion(mRotationQuaternion) * Quaternion::FromRollPitchYaw(Math::DegreesToRadians(mRotationEulerAngles.x), Math::DegreesToRadians(mRotationEulerAngles.y), Math::DegreesToRadians(mRotationEulerAngles.z));
		mRotationQuat = Quaternion::Normalize(mRotationQuat);
		mInvRotationQuat = mRotationQuat.Conjugate();

		PlanetFrameCB cb{};
		Vector3 planetCenterWS = Vector3(mTranslation) + worldTranslation;
		cb.Center = DirectX::XMFLOAT3((float)planetCenterWS.x, (float)planetCenterWS.y, (float)planetCenterWS.z);
		cb.Radius = (float)mRadius;
		cb.MaxHeight = (float)mMaxHeight;
		cb.MinHeight = (float)mMinHeight;

		Vector3 playerCamRel = playerCamPosWS - planetCenterWS;
		Vector3 renderingCamRel = renderingCamPosWS - planetCenterWS;
		Vector3 playerCamPosPS = Vector3::Rotate(playerCamRel, mInvRotationQuat);
		Vector3 renderingCamPosPS = Vector3::Rotate(renderingCamRel, mInvRotationQuat);

		frustum->UpdatePlanetSpace(playerCamPosPS, playerCamRightWS, playerCamUpWS, playerCamForwardWS, mInvRotationQuat, camera->GetNearClip(), camera->GetFarClip(), camera->GetVerticalFOV(), camera->GetAspectRatio(), frustumBias);

		Vector3 planetPosWS = planetCenterWS - renderingCamPosWS + worldTranslation;

		DirectX::XMMATRIX viewMatrixPlanetRendering = DirectX::XMLoadFloat4x4(&renderingCameraViewMatrix);
		viewMatrixPlanetRendering.r[3] = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

		double dist = playerCamRel.Length();
		double alt = dist - mRadius;
		cb.Altitude = (float)alt;

		// planet-fixed triad – ONLY the quaternion is involved
		Vector3 lonEastWS = Vector3::Normalize(Vector3::Rotate({ 1,0,0 }, mRotationQuat)); // +longitude
		Vector3 spinUpWS = Vector3::Normalize(Vector3::Rotate({ 0,1,0 }, mRotationQuat)); // spin axis
		Vector3 lonNorthWS = Vector3::Normalize(Vector3::Rotate({ 0,0,1 }, mRotationQuat));

		// camera-dependent radial, kept for lifting the grid
		Vector3 radUpWS = Vector3::Normalize(playerCamRel);// Vector3::Normalize(camPosWS - Vector3(sTranslation));

		// 1.3   project planet-east into the tangent plane → tangent east
		Vector3 tanEastWS = lonEastWS - radUpWS * Vector3::Dot(lonEastWS, radUpWS);
		if (tanEastWS.LengthSquared() < 1e-6f)                      // at planet pole
			tanEastWS = Vector3::Normalize(Vector3::Cross(spinUpWS, radUpWS));
		else
			tanEastWS = Vector3::Normalize(tanEastWS);

		// 1.4   tangent north = radial × tangent-east
		Vector3 tanNorthWS = Vector3::Normalize(Vector3::Cross(radUpWS, tanEastWS));

		cb.BasisTanEast = DirectX::XMFLOAT3({ (float)tanEastWS.x, (float)tanEastWS.y, (float)tanEastWS.z });
		cb.BasisTanNorth = DirectX::XMFLOAT3({ (float)tanNorthWS.x, (float)tanNorthWS.y, (float)tanNorthWS.z });
		cb.BasisRadUp =  DirectX::XMFLOAT3({ (float)radUpWS.x, (float)radUpWS.y, (float)radUpWS.z });

		cb.BasisLonEast = DirectX::XMFLOAT3({ (float)lonEastWS.x, (float)lonEastWS.y, (float)lonEastWS.z });
		cb.BasisLonNorth = DirectX::XMFLOAT3({ (float)lonNorthWS.x, (float)lonNorthWS.y, (float)lonNorthWS.z });
		cb.BasisSpinUp = DirectX::XMFLOAT3({ (float)spinUpWS.x, (float)spinUpWS.y, (float)spinUpWS.z });

		int num = (int)mHeightDetails.size();
		cb.NumHeightDetails = num;

		mBasisLonEast = cb.BasisLonEast;
		mBasisLonNorth = cb.BasisLonNorth;
		mBasisSpinUp = cb.BasisSpinUp;
		mBasisRadUp = cb.BasisRadUp;
		mBasisTanEast = cb.BasisTanEast;
		mBasisTanNorth = cb.BasisTanNorth;

		mShiftEastM = Vector3::Dot(worldTranslation, mBasisTanEast);
		mShiftNorthM = Vector3::Dot(worldTranslation, mBasisTanNorth);

		mPlanetFrameBuffer.Write(reinterpret_cast<uint8_t*>(&cb), sizeof(cb), 0);

		mPlanetFrameCBuffer->Map(mPlanetFrameBuffer);

		mActiveLevels = DetermineActiveLODLevels(playerCamPosPS, physicsEngine);

		const uint32_t L0 = mActiveLevels.first;
		const uint32_t Ln = L0 + mActiveLevels.count;

		Vector3 camTangent = { Vector3::Dot(playerCamRel, tanEastWS), 0.0, Vector3::Dot(playerCamRel, tanNorthWS) };

		if (mMeshMode == PlanetMeshMode::Icosphere)
			mIcosphereMesh->OnUpdate(frustum, viewMatrixPlanetRendering, playerCamPosPS, renderingCamPosPS, planetPosWS, mRadius, mMaxHeight);

		if (!mRunOnce)
		{
			UpdateLevelOrigins(camTangent);
			mRunOnce = true;
		}

		for (uint32_t L = 0; L < mNumLevels; ++L)
			mLevels[L].InFrustum = (L >= L0 && L < Ln);

		if (mHeightDetailsDirty)
			UploadHeightDetailsToGPU();
	}

	DirectX::XMMATRIX Planet::GetTransformRotation()
	{
		return DirectX::XMMatrixIdentity() * (DirectX::XMMatrixRotationQuaternion(DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(mRotationEulerAngles.x), DirectX::XMConvertToRadians(mRotationEulerAngles.y), DirectX::XMConvertToRadians(mRotationEulerAngles.z)))) * DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&mRotationQuaternion));
	}

	DirectX::XMMATRIX Planet::GetTransformNoScale()
	{
		return DirectX::XMMatrixIdentity() * (DirectX::XMMatrixRotationQuaternion(DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(mRotationEulerAngles.x), DirectX::XMConvertToRadians(mRotationEulerAngles.y), DirectX::XMConvertToRadians(mRotationEulerAngles.z)))) * DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&mRotationQuaternion)) * DirectX::XMMatrixTranslation(mTranslation.x, mTranslation.y, mTranslation.z);
	}

	Ref<TextureCube> Planet::CreateHeightMapCube(const Texture2D* heightMapTexture)
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		const uint32_t cubemapSize = 2048;

		TextureSampler* defaultSampler = TextureLibrary::GetSampler("UWrapVClampLinearSampler");

		Ref<TextureCube> heightMapCube = CreateRef<TextureCube>("HeightMapCube", DXGI_FORMAT_R32_FLOAT, cubemapSize, cubemapSize);
		 
		heightMapCube->CreateUAV(0);

		ShaderLibrary::Get("assets/shaders/Planet/HeightMapToCubeMap.hlsl")->Bind();

		heightMapTexture->Bind(0, D3D11_COMPUTE_SHADER);
		defaultSampler->Bind(0, D3D11_COMPUTE_SHADER);

		heightMapCube->BindForReadWrite(0, D3D11_COMPUTE_SHADER);

		const uint32_t groupsX = (cubemapSize + 31) / 32;
		const uint32_t groupsY = (cubemapSize + 31) / 32;
		RenderCommand::DispatchCompute(groupsX, groupsY, 6);

		heightMapCube->UnbindUAV();

		return heightMapCube;
	}

	Ref<TextureCube> Planet::CreateNormalMapCube(const TextureCube* heightCube)
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		const uint32_t cubemapSize = 2048;
		TextureSampler* linearSampler = TextureLibrary::GetSampler("UWrapVClampLinearSampler");

		Ref<TextureCube> normalCube = CreateRef<TextureCube>("NormalMapCube", DXGI_FORMAT_R8G8B8A8_UNORM, cubemapSize, cubemapSize);
		normalCube->CreateUAV(0);

		// Set up constant buffer with PlanetRadius and CubemapSize
		// (however you normally do this — bind to b0 for CS)

		ShaderLibrary::Get("assets/shaders/Planet/HeightCubeToNormalCube.hlsl")->Bind();
		heightCube->Bind(0, D3D11_COMPUTE_SHADER);      // t0
		linearSampler->Bind(0, D3D11_COMPUTE_SHADER);    // s0
		normalCube->BindForReadWrite(0, D3D11_COMPUTE_SHADER); // u0

		const uint32_t groupsX = (cubemapSize + 31) / 32;
		const uint32_t groupsY = (cubemapSize + 31) / 32;
		RenderCommand::DispatchCompute(groupsX, groupsY, 6);

		normalCube->UnbindUAV();
		return normalCube;
	}

	Ref<TextureCube> Planet::CreateAlbedoCube(const Texture2D* albedoTexture)
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		const uint32_t cubemapSize = 4096; // higher res than height since this is what you see
		TextureSampler* defaultSampler = TextureLibrary::GetSampler("UWrapVClampLinearSampler");

		Ref<TextureCube> albedoCube = CreateRef<TextureCube>(
			"AlbedoCube", DXGI_FORMAT_R8G8B8A8_UNORM, cubemapSize, cubemapSize);
		albedoCube->CreateUAV(0);

		ShaderLibrary::Get("assets/shaders/Planet/AlbedoMapToCube.hlsl")->Bind();
		albedoTexture->Bind(0, D3D11_COMPUTE_SHADER);
		defaultSampler->Bind(0, D3D11_COMPUTE_SHADER);
		albedoCube->BindForReadWrite(0, D3D11_COMPUTE_SHADER);

		const uint32_t groupsX = (cubemapSize + 31) / 32;
		const uint32_t groupsY = (cubemapSize + 31) / 32;
		RenderCommand::DispatchCompute(groupsX, groupsY, 6);

		albedoCube->UnbindUAV();
		return albedoCube;
	}

	inline float HorizonDistance(float Rg, float h) {
		// d = sqrt( (Rg+h)^2 - Rg^2 ) = sqrt(h*h + 2*Rg*h )
		return std::sqrt(std::max(0.0f, h * h + 2.0f * Rg * h));
	}

	static float Smoothstep(float a, float b, float x)
	{
		float t = std::clamp((x - a) / (b - a), 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}

	void Planet::MapRenderingSettings()
	{
		mRenderingSettingsBuffer.Write((uint8_t*)&mSlopeSensitivity, 4, 0);
		mRenderingSettingsBuffer.Write((uint8_t*)&mSlopeThreshold, 4, 4);
		mRenderingSettingsBuffer.Write((uint8_t*)&mSlopeDarkening, 4, 8);

		mRenderingSettingsCBuffer->Map(mRenderingSettingsBuffer);
	}

	float Planet::GetSpaceFactor(Vector3 cameraPosition, const Vector3& worldTranslation)
	{
		const double RbPhys = mRadius + std::min(0.0, mMinHeight);
		const double Rt = mRadius + mAtmosphere.AtmosphereHeight;
		const double rCam = Vector3::Length(cameraPosition - (Vector3(mTranslation) + worldTranslation));

		// clamp within [RbPhys..Rt] for the "within atmosphere shell" evaluation
		const double rClamped = std::clamp(rCam, RbPhys, Rt);

		// airFrac = 1 at ground, 0 at TOA
		const double airFrac = std::clamp((Rt - rClamped) / std::max(Rt - RbPhys, 1e-6), 0.0, 1.0);

		// spaceFactor = 0 at ground, 1 at TOA
		// Make this transition WIDE to avoid popping:
		// - start when only ~50% air remains above you
		// - end when ~5% air remains
		const float startAir = 0.50f; // start "space look" when airFrac falls below this
		const float endAir = 0.05f; // full "space look" near TOA

		const float spaceFactor = 1.0f - Smoothstep(endAir, startAir, (float)airFrac);
		return spaceFactor;
	}

	void Planet::Shutdown()
	{
	}

	double Planet::ComputeCurvatureBias(double desiredSwitchHeight, double radius, double patchWidth, double focalLenPx, double screenErrorPx)
	{
		return desiredSwitchHeight *(8.0 * radius * screenErrorPx) / (patchWidth * patchWidth * focalLenPx);
	}

	void Planet::GenerateDistanceLUT(uint32_t maxLevels, double planetRadius, float FoVY, uint32_t viewportWidth, double metersPerFirstCell, float screenErrorPx, double spacingBias)
	{
		mDistanceLUT.clear();
		mDistanceLUT.reserve(maxLevels);

		double cell = metersPerFirstCell;                 // texel edge (m)
		double patchWidth = cell * (mGridSize - 1);

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
			mDistanceLUT.emplace_back(d * d);

			cell *= 2.0;
			if (L >= 5) cell *= spacingBias;
			patchWidth = cell * (mGridSize - 1);
		}

		mDistanceLUT.back() = std::numeric_limits<double>::max();

		//for (auto level : mDistanceLUT)
		//	TOAST_CORE_INFO("sDistanceLUT: %lf", level);
	}

	void Planet::GenerateFaceDotLevelLUT(std::vector<double>& faceLevelDotLUT, float planetRadius, float maxHeight)
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

	void Planet::GenerateHeightMultLUT(std::vector<double>& heightMultLUT, double planetRadius, double maxHeight)
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

	TerrainCubeData Planet::LoadTerrainDataFromTextureCube()
	{
		TerrainCubeData td{};

		ID3D11Device* device = RenderCommand::sRendererAPI->GetDevice();
		ID3D11DeviceContext* context = RenderCommand::sRendererAPI->GetDeviceContext();

		// Get underlying D3D texture
		Microsoft::WRL::ComPtr<ID3D11Texture2D> tex = mBaseHeightMapTextureCube->GetTexture();
		TOAST_CORE_ASSERT(tex, "TextureCube has no underlying texture!");

		D3D11_TEXTURE2D_DESC desc{};
		tex->GetDesc(&desc);

		// We only care about mip 0 for physics
		const UINT mipLevel = 0;
		const UINT faceCount = desc.ArraySize; // should be 6
		TOAST_CORE_ASSERT(faceCount == 6, "Height cube should have 6 faces.");
		TOAST_CORE_ASSERT(desc.Format == DXGI_FORMAT_R32_FLOAT, "Expected R32_FLOAT height cube.");

		td.Width = desc.Width;
		td.Height = desc.Height;

		// Create a staging texture to read back from GPU
		D3D11_TEXTURE2D_DESC stagingDesc = desc;
		stagingDesc.Usage = D3D11_USAGE_STAGING;
		stagingDesc.BindFlags = 0;
		stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		stagingDesc.MiscFlags = 0; 

		Microsoft::WRL::ComPtr<ID3D11Texture2D> stagingTex;
		HRESULT hr = device->CreateTexture2D(&stagingDesc, nullptr, &stagingTex);
		TOAST_CORE_ASSERT(SUCCEEDED(hr), "Failed to create staging texture for height cube readback.");

		for (UINT face = 0; face < faceCount; ++face)
		{
			// Copy face, mip 0 into staging
			UINT subresource = D3D11CalcSubresource(mipLevel, face, desc.MipLevels);
			context->CopySubresourceRegion(
				stagingTex.Get(),
				subresource,
				0, 0, 0,
				tex.Get(),
				subresource,
				nullptr
			);

			// Map and copy into CPU array
			D3D11_MAPPED_SUBRESOURCE mapped{};
			hr = context->Map(stagingTex.Get(), subresource, D3D11_MAP_READ, 0, &mapped);
			TOAST_CORE_ASSERT(SUCCEEDED(hr), "Failed to map staging height cube.");

			const uint8_t* srcBytes = static_cast<const uint8_t*>(mapped.pData);
			const size_t rowPitchBytes = mapped.RowPitch;

			td.FaceHeight[face].resize(static_cast<size_t>(td.Width) * td.Height);

			for (uint32_t y = 0; y < td.Height; ++y)
			{
				const float* srcRow = reinterpret_cast<const float*>(srcBytes + y * rowPitchBytes);
				for (uint32_t x = 0; x < td.Width; ++x)
				{
					float h = srcRow[x]; // R32_FLOAT
					td.FaceHeight[face][Index2D(x, y, td.Width)] = h;
				}
			}

			context->Unmap(stagingTex.Get(), subresource);
		}

		return td;
	}

	bool Planet::ProjectWorldPosToLevelGrid(const Vector3& worldPos, const Vector3& worldTranslation, PlanetProjectionResult& out)
	{
		// 0) Planet center in camera-relative world space
		Vector3 planetCenterWS = Vector3(mTranslation) + worldTranslation;

		// 1) Vector from planet center to object
		Vector3 pLocal = worldPos - planetCenterWS;
		double  r = pLocal.Length();
		if (r <= 1e-6)
			return false;

		// 2) Project object down to the reference sphere
		Vector3 nObj = pLocal / r;                         // direction center -> object
		double  R = mRadius;
		Vector3 groundWS = planetCenterWS + nObj * R;      // point on sphere under object

		// 3) Tangent basis at the camera (same as used in the VS)
		Vector3 radUp = Vector3(mBasisRadUp);
		Vector3 tanEast = Vector3(mBasisTanEast);
		Vector3 tanNorth = Vector3(mBasisTanNorth);

		// Camera is at (0,0,0) in your floating-origin world, so
		// "camera -> ground" is just groundWS in this space.
		Vector3 camToGroundWS = groundWS;

		// Tangent-plane offsets in metres (same meaning as 'off' in the VS)
		double offX = Vector3::Dot(camToGroundWS, tanEast);
		double offY = Vector3::Dot(camToGroundWS, tanNorth);

		// 4) LOD ring for this position (already correct with ground-projection)
		uint32_t L = GetLODForWorldPos(worldPos);
		double   cell = double(1u << L);                   // metres / cell

		// 5) Continuous global grid coordinates in this level's grid
		double gxCont = offX / cell;
		double gyCont = offY / cell;

		// Snap to nearest vertex
		int gWorldX = (int)std::floor(gxCont + 0.5);
		int gWorldY = (int)std::floor(gyCont + 0.5);

		// 6) Rebuild "off" exactly like VS: off = gWorld * CellSize
		double offXSnapped = double(gWorldX) * cell;
		double offYSnapped = double(gWorldY) * cell;

		// 7) Rebuild pSphereLocal and approximate normal exactly like the VS
		Vector3 pSphereLocal = radUp * R + tanEast * offXSnapped + tanNorth * offYSnapped;

		Vector3 nWSApprox = Vector3::Normalize(pSphereLocal);

		// 8) Fill result
		out.Level = L;
		out.GWorldX = gWorldX;
		out.GWorldY = gWorldY;
		out.NWSApprox = nWSApprox;
		out.TangentDist = std::sqrt(offX * offX + offY * offY); // for debug

		//TOAST_CORE_CRITICAL("ProjectWorldPosToLevelGrid: L=%u g=(%d,%d) off=(%.3lf,%.3lf) tanDist=%.3lf", L, gWorldX, gWorldY, offXSnapped, offYSnapped, out.TangentDist);

		return true;
	}

	uint32_t Planet::GetLODForWorldPos(const Vector3& worldPosWS)
	{
		// 1) Planet center in camera-relative world space
		Vector3 planetCenterWS = Vector3(mTranslation);

		// 2) Vector from planet center to object
		Vector3 pLocal = worldPosWS - planetCenterWS;
		double  r = pLocal.Length();
		if (r <= 1e-6)
			return mActiveLevels.first;   // degenerate, just clamp to finest active

		// 3) Radial direction and ground point on the reference sphere
		Vector3 n = pLocal / r;                         // unit vector planetCenter -> object
		Vector3 groundWS = planetCenterWS + n * mRadius; // point "under" the object on sphere

		// 4) Tangent-plane coordinates relative to camera
		//    Camera is at (0,0,0) in your floating-origin world space.
		Vector3 tanEast = Vector3(mBasisTanEast);
		Vector3 tanNorth = Vector3(mBasisTanNorth);

		// Vector from camera to ground point (camera is at origin)
		Vector3 camToGroundWS = groundWS;

		double offX = Vector3::Dot(camToGroundWS, tanEast);
		double offY = Vector3::Dot(camToGroundWS, tanNorth);

		// Use square metric because LOD regions are squares in this plane
		double squareDist = std::max(std::abs(offX), std::abs(offY));

		// 5) Active LOD range (same as rendering)
		uint32_t first = mActiveLevels.first;
		uint32_t last = first + mActiveLevels.count - 1;
		if (last >= mNumLevels)
			last = mNumLevels - 1;

		auto halfExtent = [&](uint32_t L) -> double
			{
				double cell = double(1u << L);                         // metres per cell
				double half = 0.5 * double(mGridSize - 1) * cell;      // half side length of that level
				return half;
			};

		// 6) Smallest L whose square covers this ground point
		for (uint32_t L = first; L <= last; ++L)
		{
			if (squareDist <= halfExtent(L))
				return L;
		}

		// Outside all rings -> clamp to outermost active
		return last;
	}

	void Planet::UploadHeightDetailsToGPU()
	{
		const uint32_t count = (uint32_t)mHeightDetails.size();

		if (count == 0)
		{
			mHeightDetailSettingsSB.reset();
			mHeightDetailPermSB.reset();
			mHeightDetailsDirty = false;
			return;
		}

		// --- Pack GPU arrays ---
		std::vector<HeightDetail::GPUData> settings(count);
		std::vector<Int4> permTables(count * 64);

		for (uint32_t i = 0; i < count; ++i)
		{
			HeightDetail& d = mHeightDetails[i];

			// Keep CPU perm valid for height queries
			BuildPermutationTable(d.Seed, d.Perm);

			// Each detail owns 64 int4 entries (256 ints)
			const int32_t permBase = (int32_t)(i * 64);

			// Pack perm[256] -> int4[64]
			for (int k = 0; k < 64; ++k)
			{
				const int idx = k * 4;
				permTables[permBase + k] = Int4(
					(int32_t)d.Perm[idx + 0],
					(int32_t)d.Perm[idx + 1],
					(int32_t)d.Perm[idx + 2],
					(int32_t)d.Perm[idx + 3]
				);
			}

			// Copy user-authored GPU settings, but inject the computed PermBase
			HeightDetail::GPUData s = d.GPUSettings;
			s.PermBase = permBase;
			settings[i] = s;
		}

		// If you expect count to change, I recommend recreating when it does:
		if (count != mLastHeightDetailCount) 
		{ 
			mHeightDetailSettingsSB = CreateRef<StructuredBuffer>((uint32_t)sizeof(HeightDetail::GPUData), count, D3D11_USAGE_DYNAMIC);
			mHeightDetailPermSB = CreateRef<StructuredBuffer>((uint32_t)sizeof(Int4), count * 64, D3D11_USAGE_DYNAMIC);

			mLastHeightDetailCount = count; 
		}

		// --- Update GPU ---
		mHeightDetailSettingsSB->Update(settings.data(), settings.size() * sizeof(HeightDetail::GPUData));
		mHeightDetailPermSB->Update(permTables.data(), permTables.size() * sizeof(Int4));

		mHeightDetailsDirty = false;
	}

	void Planet::BuildPermutationTable(uint32_t seed, int outPerm[256])
	{
		std::vector<int> values(256);
		for (int i = 0; i < 256; ++i)
			values[i] = i;

		std::mt19937 rng(seed);
		std::shuffle(values.begin(), values.end(), rng);

		for (int i = 0; i < 256; ++i)
			outPerm[i] = values[i];
	}

	uint32_t Planet::ObjectInstancesForLevelFromDensity(const TerrainObject& o, uint32_t cellSize, uint32_t gridSize)
	{
		const double cells = double(gridSize - 1);
		const double widthM = cells * double(cellSize);
		const double areaM2 = widthM * widthM;
		const double areaKm2 = areaM2 / 1e6;

		double inst = o.DensityPerKm2 * areaKm2;
		uint32_t u = (uint32_t)std::llround(inst);

		u = std::min<uint32_t>(u, (uint32_t)o.MaxTotal);
		u = std::min<uint32_t>(u, (uint32_t)o.MaxPerPatch); // if you mean per-level cap

		return u;
	}

	bool Planet::IsTerrainReady() const
	{
		// Whatever “ready” means in your engine. This is a common minimum.
		if (mTerrainCubeData.Width == 0 || mTerrainCubeData.Height == 0)
			return false;

		// Ensure all 6 faces exist and have the expected size.
		const uint32_t W = mTerrainCubeData.Width;
		const uint32_t H = mTerrainCubeData.Height;
		const size_t expected = size_t(W) * size_t(H);

		for (int f = 0; f < 6; ++f)
		{
			if (mTerrainCubeData.FaceHeight[f].size() != expected)
				return false;
		}

		return true;
	}

	// OLD PLANET SYSTEM BUT MAYBE BETTER

	static constexpr uint32_t MINGUARANTEEDSUBDIVISION = 4;

	void PlanetMeshIcosphere::Init()
	{
		TOAST_PROFILE_FUNCTION();

		double ratio = ((1.0 + sqrt(5.0)) / 2.0);

		mBaseIcosahedronVerts = std::vector<Vector3>{
			Vector3::Normalize({ ratio, 0.0, -1.0 }),
			Vector3::Normalize({ -ratio, 0.0, -1.0 }),
			Vector3::Normalize({ ratio, 0.0, 1.0 }),

			Vector3::Normalize({ -ratio, 0.0, 1.0 }),
			Vector3::Normalize({ 0.0, -1.0, ratio }),
			Vector3::Normalize({ 0.0, -1.0, -ratio }),
			Vector3::Normalize({ 0.0, 1.0, ratio }),
			Vector3::Normalize({ 0.0, 1.0, -ratio }),
			Vector3::Normalize({ -1.0, ratio, 0.0 }),
			Vector3::Normalize({ -1.0, -ratio, 0.0 }),
			Vector3::Normalize({ 1.0 , ratio, 0.0 }),
			Vector3::Normalize({ 1.0 , -ratio, 0.0 })
		};

		mStartIndices = std::vector<uint32_t>{
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

		// Setting up Shader Layout
		InitShaderLayout();

		// Setting up Constant Buffers
		mPlanetMeshCBuffer = ConstantBufferLibrary::Load("IcospherePlanet", 128, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_VERTEX_SHADER, (CBufferBindSlot)2) });
		mPlanetMeshCBuffer->Bind();
		mPlanetMeshBuffer.Allocate(mPlanetMeshCBuffer->GetSize());
		mPlanetMeshBuffer.ZeroInitialize();
	}

	void PlanetMeshIcosphere::InitShaderLayout()
	{
		TOAST_PROFILE_FUNCTION();

		std::vector<ShaderLayout::ShaderInputElement> elements;
		elements.reserve(9);

		// -------- Slot 0: Per-vertex --------
		// uint2 localPosition : TEXCOORD0;
		{
			ShaderLayout::ShaderInputElement e(DXGI_FORMAT_R16G16_UINT, "TEXCOORD", 0);
			e.mInputSlot = 0;
			e.mInputClassification = D3D11_INPUT_PER_VERTEX_DATA;
			e.mInstanceDataStepRate = 0;
			elements.emplace_back(e);
		}

		// -------- Slot 1: Per-instance --------
		// int level : TEXTUREID0;
		{
			ShaderLayout::ShaderInputElement e(DXGI_FORMAT_R32_SINT, "TEXTUREID", 0);
			e.mInputSlot = 1;
			e.mInputClassification = D3D11_INPUT_PER_INSTANCE_DATA;
			e.mInstanceDataStepRate = 1;
			elements.emplace_back(e);
		}

		// float3 V0 : POSITION0;
		{
			ShaderLayout::ShaderInputElement e(DXGI_FORMAT_R32G32B32_FLOAT, "POSITION", 0);
			e.mInputSlot = 1;
			e.mInputClassification = D3D11_INPUT_PER_INSTANCE_DATA;
			e.mInstanceDataStepRate = 1;
			elements.emplace_back(e);
		}

		// float3 V1 : POSITION1;
		{
			ShaderLayout::ShaderInputElement e(DXGI_FORMAT_R32G32B32_FLOAT, "POSITION", 1);
			e.mInputSlot = 1;
			e.mInputClassification = D3D11_INPUT_PER_INSTANCE_DATA;
			e.mInstanceDataStepRate = 1;
			elements.emplace_back(e);
		}

		// float3 V2 : POSITION2;
		{
			ShaderLayout::ShaderInputElement e(DXGI_FORMAT_R32G32B32_FLOAT, "POSITION", 2);
			e.mInputSlot = 1;
			e.mInputClassification = D3D11_INPUT_PER_INSTANCE_DATA;
			e.mInstanceDataStepRate = 1;
			elements.emplace_back(e);
		}

		// float3 P0RelHi : TEXCOORD1;
		{
			ShaderLayout::ShaderInputElement e(DXGI_FORMAT_R32G32B32_FLOAT, "TEXCOORD", 1);
			e.mInputSlot = 1;
			e.mInputClassification = D3D11_INPUT_PER_INSTANCE_DATA;
			e.mInstanceDataStepRate = 1;
			elements.emplace_back(e);
		}

		// float3 P1RelHi : TEXCOORD2;
		{
			ShaderLayout::ShaderInputElement e(DXGI_FORMAT_R32G32B32_FLOAT, "TEXCOORD", 2);
			e.mInputSlot = 1;
			e.mInputClassification = D3D11_INPUT_PER_INSTANCE_DATA;
			e.mInstanceDataStepRate = 1;
			elements.emplace_back(e);
		}

		// float3 P2RelHi : TEXCOORD3;
		{
			ShaderLayout::ShaderInputElement e(DXGI_FORMAT_R32G32B32_FLOAT, "TEXCOORD", 3);
			e.mInputSlot = 1;
			e.mInputClassification = D3D11_INPUT_PER_INSTANCE_DATA;
			e.mInstanceDataStepRate = 1;
			elements.emplace_back(e);
		}

		Shader* shader = ShaderLibrary::Get("assets/shaders/Planet/PlanetIcosphereGeometryPass.hlsl");
		ID3D10Blob* vsBlob = shader->GetVSRaw();

		mShaderInputLayout = CreateRef<ShaderLayout>(elements, vsBlob);
	}

	void PlanetMeshIcosphere::GeneratePatchGeometry()
	{
		TOAST_PROFILE_FUNCTION();

		mVertices.clear();
		mIndices.clear();

		// N = 2^mPatchLevels; grid has (N+1) rows
		const uint32_t N = 1u << mPatchLevels;
		const uint32_t mRC = N + 1u;

		uint32_t rowIndex = 0;
		uint32_t nextIndex = 0;

		for (uint32_t row = 0; row < mRC; row++)
		{
			uint32_t numCols = mRC - row;
			nextIndex += numCols;

			for (uint32_t col = 0; col < numCols; col++)
			{
				// Store integer barycentric grid coords (i,j)
				// i = col, j = row
				mVertices.emplace_back(PlanetVertexCPU((uint16_t)col, (uint16_t)row));

				if (row < mRC - 1 && col < numCols - 1)
				{
					mIndices.emplace_back(rowIndex + col);
					mIndices.emplace_back(nextIndex + col);
					mIndices.emplace_back(1 + rowIndex + col);

					if (col < numCols - 2)
					{
						mIndices.emplace_back(nextIndex + col);
						mIndices.emplace_back(1 + nextIndex + col);
						mIndices.emplace_back(1 + rowIndex + col);
					}
				}
			}

			rowIndex = nextIndex;
		}
	}

	void PlanetMeshIcosphere::OnUpdate(Frustum* frustum, DirectX::XMMATRIX viewMatrixPlanetRendering, Vector3& cameraPosPS, Vector3& renderingCameraPosPS, Vector3& planetCenterWS, double radius, double maxHeight)
	{
		TOAST_PROFILE_FUNCTION();

		mRadius = radius;
		mMaxHeight = maxHeight;

		mMidpointCache.clear();
		mSphereVertices.clear(); // keep a copy from Init()

		mSphereVertices.reserve(200000);
		for (const auto& v : mBaseIcosahedronVerts)
			mSphereVertices.push_back(v);

		// Updating Constant Buffer
		DirectX::XMFLOAT3 camHi, camLo;

		camHi = { (float)renderingCameraPosPS.x, (float)renderingCameraPosPS.y, (float)renderingCameraPosPS.z };
		camLo = { (float)(renderingCameraPosPS.x - (double)camHi.x), (float)(renderingCameraPosPS.y - (double)camHi.y), (float)(renderingCameraPosPS.z - (double)camHi.z) };

		mCamHiPS = Vector3(camHi.x, camHi.y, camHi.z);

		DirectX::XMFLOAT3 relHi = { (float)planetCenterWS.x, (float)planetCenterWS.y, (float)planetCenterWS.z };
		DirectX::XMFLOAT3 relLo = {	(float)(planetCenterWS.x - (double)relHi.x), (float)(planetCenterWS.y - (double)relHi.y), (float)(planetCenterWS.z - (double)relHi.z) };

		float radiusF = mRadius;
		uint32_t patchLevels = mPatchLevels;
		mPlanetMeshBuffer.Write((uint8_t*)&radiusF, 4, 0);
		mPlanetMeshBuffer.Write((uint8_t*)&camHi, 12, 4);
		mPlanetMeshBuffer.Write((uint8_t*)&viewMatrixPlanetRendering, 64, 16);
		mPlanetMeshBuffer.Write((uint8_t*)&patchLevels, 4, 80);
		mPlanetMeshBuffer.Write((uint8_t*)&camLo, 12, 84);
		mPlanetMeshBuffer.Write((uint8_t*)&relHi, 12, 96);
		mPlanetMeshBuffer.Write((uint8_t*)&relLo, 12, 112);
		mPlanetMeshCBuffer->Map(mPlanetMeshBuffer);

		if (mDistanceLUTIsDirty)
			GenerateDistanceLUT();

		if (mFaceLevelDotLUTIsDirty)
			GenerateFaceDotLevelLUT();

		if(mHeightMultLUTIsDirty)
			GenerateHeightMultLUT();

		if (mPatchIsDirty)
			GeneratePatchGeometry();

		mPatches.clear();

		for (size_t i = 0; i < mStartIndices.size(); i += 3)
			RecursiveFace(frustum, mStartIndices[i], mStartIndices[i + 1], mStartIndices[i + 2], 0, cameraPosPS, true);

		BuildGPUData();

		if (!mPatchesGPU.empty())
		{
			const uint32_t instBytes = (uint32_t)(mPatchesGPU.size() * sizeof(PlanetPatchGPU));
			mInstanceVertexBuffer = CreateRef<VertexBuffer>(mPatchesGPU.data(), instBytes, (uint32_t)mPatchesGPU.size(), 1);
		}
		else
			mInstanceVertexBuffer = nullptr;

		if (mPatchIsDirty)
		{
			if (!mVerticesGPU.empty())
			{
				const uint32_t vbBytes = (uint32_t)(mVerticesGPU.size() * sizeof(PlanetVertexGPU));
				mVertexBuffer = CreateRef<VertexBuffer>(mVerticesGPU.data(), vbBytes, (uint32_t)mVerticesGPU.size(), 0);
			}
			else
				mVertexBuffer = nullptr;

			if (!mIndices.empty())
				mIndexBuffer = CreateRef<IndexBuffer>(mIndices.data(), (uint32_t)mIndices.size());
			else
				mIndexBuffer = nullptr;
		}

		mPatchIsDirty = false;
		mDistanceLUTIsDirty = false;
		mFaceLevelDotLUTIsDirty = false;
		mHeightMultLUTIsDirty = false;
	}

	void PlanetMeshIcosphere::RecursiveFace(Frustum* frustum, uint32_t ia, uint32_t ib, uint32_t ic, int16_t subdivision, Vector3& cameraPosPS, bool splitCull)
	{
		//TOAST_PROFILE_FUNCTION();

		Vector3 va = mSphereVertices[ia];
		Vector3 vb = mSphereVertices[ib];
		Vector3 vc = mSphereVertices[ic];

		Vector3 A, B, C;

		NextPlanetFace nextPlanetFace = CheckFaceSplit(frustum, va, vb, vc, subdivision, cameraPosPS, splitCull);

		if (nextPlanetFace == NextPlanetFace::CULL)
			return;

		if (subdivision < HARDCAPSUBDIVISIONS && (nextPlanetFace == NextPlanetFace::SPLIT || nextPlanetFace == NextPlanetFace::SPLITCULL))
		{
			uint32_t iAB = GetMidpoint(ia, ib);
			uint32_t iBC = GetMidpoint(ib, ic);
			uint32_t iCA = GetMidpoint(ic, ia);

			Vector3 vAB = mSphereVertices[iAB];
			Vector3 vBC = mSphereVertices[iBC];
			Vector3 vCA = mSphereVertices[iCA];

			int16_t nextSubdivision = subdivision + 1;

			RecursiveFace(frustum, ia, iAB, iCA, nextSubdivision, cameraPosPS, nextPlanetFace == NextPlanetFace::SPLITCULL);
			RecursiveFace(frustum, iAB, ib, iBC, nextSubdivision, cameraPosPS, nextPlanetFace == NextPlanetFace::SPLITCULL);
			RecursiveFace(frustum, iCA, iBC, ic, nextSubdivision, cameraPosPS, nextPlanetFace == NextPlanetFace::SPLITCULL);
			RecursiveFace(frustum, iAB, iBC, iCA, nextSubdivision, cameraPosPS, nextPlanetFace == NextPlanetFace::SPLITCULL);
		}
		else
		{
			if (nextPlanetFace == NextPlanetFace::LEAF)
				mPatches.emplace_back(PlanetPatchCPU(subdivision, ia, ib, ic));

			else if(nextPlanetFace == NextPlanetFace::LEAFPATCH)
			{
				// Compute distances using the SAME threshold you used to classify inside/outside
				Vector3 aR = va * mRadius;
				Vector3 bR = vb * mRadius;
				Vector3 cR = vc * mRadius;

				double aD2 = Vector3::LengthSquared(aR - cameraPosPS);
				double bD2 = Vector3::LengthSquared(bR - cameraPosPS);
				double cD2 = Vector3::LengthSquared(cR - cameraPosPS);

				const double splitD2 = mDistanceLUT[(uint32_t)subdivision];

				bool aIn = (aD2 < splitD2);
				bool bIn = (bD2 < splitD2);
				bool cIn = (cD2 < splitD2);

				// Identify which edge is the "cracked" edge = between the two inside vertices
				if (!aIn && bIn && cIn)
				{
					// Edge BC is cracked
					uint32_t iBC = GetMidpoint(ib, ic);

					mPatches.emplace_back(PlanetPatchCPU(subdivision, ia, ib, iBC));
					mPatches.emplace_back(PlanetPatchCPU(subdivision, ia, iBC, ic));
					return;
				}
				if (!bIn && aIn && cIn)
				{
					// Edge AC is cracked
					uint32_t iCA = GetMidpoint(ic, ia);

					mPatches.emplace_back(PlanetPatchCPU(subdivision, ib, ic, iCA));
					mPatches.emplace_back(PlanetPatchCPU(subdivision, ib, iCA, ia));
					return;
				}
				if (!cIn && aIn && bIn)
				{
					// Edge AB is cracked
					uint32_t iAB = GetMidpoint(ia, ib);

					mPatches.emplace_back(PlanetPatchCPU(subdivision, ic, ia, iAB));
					mPatches.emplace_back(PlanetPatchCPU(subdivision, ic, iAB, ib));
					return;
				}

				// Fallback: Should never occur
				mPatches.emplace_back(PlanetPatchCPU(subdivision, ia, ib, ic));
				return;
			}	 
		}
	}

	PlanetMeshIcosphere::NextPlanetFace PlanetMeshIcosphere::CheckFaceSplit(Frustum* frustum, Vector3 a, Vector3 b, Vector3 c, int16_t subdivision, Vector3& cameraPosPS, bool frustumCheckNeeded)
	{
		a = a * mRadius;
		b = b * mRadius;
		c = c * mRadius;

		Vector3 center = (a + b + c) / 3.0;

		double dotProduct = Vector3::Dot(Vector3::Normalize(center), Vector3::Normalize(center - cameraPosPS));

		if (mBackfaceCulling && dotProduct >= mFaceLevelDotLUT[(uint32_t)subdivision])
			return NextPlanetFace::CULL;

		bool frustumKnown = false;
		VolumeTri frustumResult = VolumeTri::INTERSECT;

		if (mFrustumCulling && frustumCheckNeeded)
		{
			frustumKnown = true;
			frustumResult = frustum->ContainsPatchSphere(a, b, c, mRadius);

			if (frustumResult == VolumeTri::OUTSIDE)
				return NextPlanetFace::CULL;
		}

		if (subdivision >= mMaxSubdivisions)
			return NextPlanetFace::LEAF;

		if (subdivision <= MINGUARANTEEDSUBDIVISION)
			return NextPlanetFace::SPLITCULL;

		double aD2 = Vector3::LengthSquared(a - cameraPosPS);
		double bD2 = Vector3::LengthSquared(b - cameraPosPS);
		double cD2 = Vector3::LengthSquared(c - cameraPosPS);

		const double splitD2 = mDistanceLUT[(uint32_t)subdivision];

		uint32_t inside = 0;
		if (aD2 < splitD2) inside++;
		if (bD2 < splitD2) inside++; 
		if (cD2 < splitD2) inside++;

		if (inside == 3)
		{
			if (frustumKnown && frustumResult == VolumeTri::CONTAINS)
				return NextPlanetFace::SPLIT;       // safe to stop checking children
			else
				return NextPlanetFace::SPLITCULL;   // children must still be checked (or frustum not known)
		}
		else if (inside == 2)
			return NextPlanetFace::LEAFPATCH;
		else
			return NextPlanetFace::LEAF;
	}

	void PlanetMeshIcosphere::BuildGPUData()
	{
		TOAST_PROFILE_FUNCTION();

		// Patch-local vertices (shared)
		mVerticesGPU.clear();
		mVerticesGPU.reserve(mVertices.size());
		for (const auto& v : mVertices)
			mVerticesGPU.emplace_back(PlanetVertexGPU{ v.I, v.J });

		// Patch instances (per-leaf)
		mPatchesGPU.clear();
		mPatchesGPU.reserve(mPatches.size());

		for (const auto& p : mPatches)
		{
			Vector3 V0 = Vector3::Normalize(mSphereVertices[p.i0]);
			Vector3 V1 = Vector3::Normalize(mSphereVertices[p.i1]);
			Vector3 V2 = Vector3::Normalize(mSphereVertices[p.i2]);

			// world meters (planet space)
			Vector3 P0 = V0 * mRadius;
			Vector3 P1 = V1 * mRadius;
			Vector3 P2 = V2 * mRadius;

			// relative-to-camHi (meters)
			Vector3 P0Rel = P0 - mCamHiPS;
			Vector3 P1Rel = P1 - mCamHiPS;
			Vector3 P2Rel = P2 - mCamHiPS;

			PlanetPatchGPU g{};
			g.level = p.level;

			g.V0 = { (float)V0.x, (float)V0.y, (float)V0.z };
			g.V1 = { (float)V1.x, (float)V1.y, (float)V1.z };
			g.V2 = { (float)V2.x, (float)V2.y, (float)V2.z };

			g.P0_rel_hi = { (float)P0Rel.x, (float)P0Rel.y, (float)P0Rel.z };
			g.P1_rel_hi = { (float)P1Rel.x, (float)P1Rel.y, (float)P1Rel.z };
			g.P2_rel_hi = { (float)P2Rel.x, (float)P2Rel.y, (float)P2Rel.z };

			mPatchesGPU.emplace_back(g);
		}
	}

	void PlanetMeshIcosphere::BindGPUData()
	{ 
		TOAST_PROFILE_FUNCTION();

		if (mInstanceVertexBuffer)
		{
			RendererAPI* API = RenderCommand::sRendererAPI.get();
			ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

			UINT strides[2] = { sizeof(PlanetVertexGPU), sizeof(PlanetPatchGPU) };
			UINT offsets[2] = { 0, 0 };
			ID3D11Buffer* bufs[2] = { mVertexBuffer->GetBuffer(), mInstanceVertexBuffer->GetBuffer() };

			deviceContext->IASetVertexBuffers(0, 2, bufs, strides, offsets);
			deviceContext->IASetIndexBuffer(mIndexBuffer->GetBuffer(), DXGI_FORMAT_R32_UINT, 0);

			mPlanetMeshCBuffer->Bind();
		}
	}

	uint32_t PlanetMeshIcosphere::GetMidpoint(uint32_t i1, uint32_t i2)
	{
		// Ensure deterministic order for the key
		uint64_t smaller = std::min(i1, i2);
		uint64_t larger = std::max(i1, i2);
		uint64_t key = (smaller << 32) | larger;

		auto it = mMidpointCache.find(key);
		if (it != mMidpointCache.end())
			return it->second;

		// Not in cache, calculate it
		Vector3 v1 = mSphereVertices[i1];
		Vector3 v2 = mSphereVertices[i2];
		// Exact midpoint calculation
		Vector3 mid = Vector3::Normalize((v1 + v2) * 0.5);

		uint32_t idx = (uint32_t)mSphereVertices.size();
		mSphereVertices.push_back(mid);

		mMidpointCache[key] = idx;
		return idx;
	}

	void PlanetMeshIcosphere::GenerateDistanceLUT()
	{
		mDistanceLUT.clear();
		mDistanceLUT.reserve((size_t)mMaxSubdivisions);

		TOAST_CORE_ASSERT(mNearDistance > 0.0, "nearDist must be > 0");
		TOAST_CORE_ASSERT(mFarDistance > mNearDistance, "farDist must be > nearDist");

		const int N = mMaxSubdivisions;

		// Ratio for log spacing
		const double ratio = mNearDistance / mFarDistance;

		for (int l = 0; l < N; ++l)
		{
			const double t = (double)l / (double)(N - 1); // 0..1
			const double D = mFarDistance * std::pow(ratio, t); // far -> near
			mDistanceLUT.push_back(D * D);
		}

		//for (auto level : mDistanceLUT)
		//	TOAST_CORE_INFO("mDistanceLUT: %lf", level);
	}

	void PlanetMeshIcosphere::GenerateFaceDotLevelLUT()
	{
		double cullingAngle = acos((mRadius) / ((mRadius) + mMaxHeight));

		mFaceLevelDotLUT.clear();
		mFaceLevelDotLUT.emplace_back(0.5f + sinf(cullingAngle));
		float angle = acosf(0.5f);
		for (int i = 1; i <= mMaxSubdivisions; i++)
		{
			angle *= 0.5f;
			mFaceLevelDotLUT.emplace_back(sin(angle + cullingAngle));
		}

		//for (auto level : mFaceLevelDotLUT)
		//	TOAST_CORE_INFO("mFaceLevelDotLUT: %lf", level);
	}

	void PlanetMeshIcosphere::GenerateHeightMultLUT()
	{
		mHeightMultLUT.clear();
		Vector3 a = mSphereVertices[1] * mRadius;
		Vector3 b = mSphereVertices[3] * mRadius;
		Vector3 c = mSphereVertices[8] * mRadius;

		// Temporary before terrain is added!
		double maxHeight = 0.0;

		Vector3 center = (a + b + c) / 3.0;
		center *= mRadius / Vector3::Length(center);//+maxHeight
		mHeightMultLUT.push_back(1.0 / Vector3::Dot(a, Vector3::Normalize(center)));
		double normMaxHeight = maxHeight / mRadius;
		for (int i = 1; i <= mMaxSubdivisions; i++)
		{
			Vector3 A = b + ((c - b) * 0.5);
			Vector3 B = c + ((a - c) * 0.5);
			c = a + ((b - a) * 0.5f);
			a = A * mRadius / Vector3::Length(A);
			b = B * mRadius / Vector3::Length(B);
			c *= mRadius / Vector3::Length(c);
			mHeightMultLUT.push_back(1.0f / Vector3::Dot(Vector3::Normalize(a), Vector3::Normalize(center)) + normMaxHeight);
		}

		//for (auto level : mHeightMultLUT)
		//	TOAST_CORE_INFO("mHeightMultLUT: %lf", level);
	}

}