#include "tpch.h"
#include "PlanetMesh.h"

#include "Toast/Assets/AssetManager.h"

#include "Toast/Renderer/PlanetMeshBaker.h"
#include "Toast/Renderer/RenderCommand.h"
#include "Toast/Renderer/TerrainSampler.h"

#include <cfloat>

namespace Toast {

	static constexpr uint32_t MINGUARANTEEDSUBDIVISION = 4;
	static constexpr double HEIGHT_UNSAMPLED = std::numeric_limits<double>::max();

	void PlanetMesh::Init()
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
		mPlanetMeshCBuffer = ConstantBufferLibrary::Load("PlanetMesh", 128, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_VERTEX_SHADER, (CBufferBindSlot)2) });
		mPlanetMeshCBuffer->Bind();
		mPlanetMeshBuffer.Allocate(mPlanetMeshCBuffer->GetSize());
		mPlanetMeshBuffer.ZeroInitialize();

		mMeshBaker = CreateRef<PlanetMeshBaker>();
		mMeshBaker->Init();
	}

	void PlanetMesh::InitShaderLayout()
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

		// float3 PatchOriginPS : TEXCOORD3;
		{
			ShaderLayout::ShaderInputElement e(DXGI_FORMAT_R32G32B32_FLOAT, "TEXCOORD", 4);
			e.mInputSlot = 1;
			e.mInputClassification = D3D11_INPUT_PER_INSTANCE_DATA;
			e.mInstanceDataStepRate = 1;
			elements.emplace_back(e);
		}

		mGPassShaderHandle = AssetManager::GetEngineShaderHandle("PlanetGeometryPass");
		ID3D10Blob* vsBlob = AssetManager::GetAsset<Shader>(mGPassShaderHandle)->GetVSRaw();

		mShaderInputLayout = CreateRef<ShaderLayout>(elements, vsBlob);
	}

	void PlanetMesh::GeneratePatchGeometry()
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

	static uint32_t sRecurseCalls;
	static uint32_t sHeightSamples;
	static uint32_t sFrustumChecks;
	static uint32_t sMidpointLookups;

	void PlanetMesh::OnUpdate(Frustum* frustum, DirectX::XMMATRIX viewMatrixPlanetRendering, Vector3& cameraPosPS, Vector3& renderingCameraPosPS, Vector3& planetCenterWS, double radius, double maxHeight, const CubeData<float>* terrainData, uint32_t materialCount)
	{
		TOAST_PROFILE_FUNCTION();

		mHighestSubdivision = 0;

		mRadius = radius;
		mMaxHeight = maxHeight;
		mTerrainCubeData = terrainData;

		mHeightCache.clear();

		mMidpointCache.clear(4096);

		if (mSphereVertices.size() < mBaseIcosahedronVerts.size())
			mSphereVertices = mBaseIcosahedronVerts;
		else
			mSphereVertices.resize(mBaseIcosahedronVerts.size());

		// Updating Constant Buffer
		DirectX::XMFLOAT3 camHi, camLo;

		camHi = { (float)renderingCameraPosPS.x, (float)renderingCameraPosPS.y, (float)renderingCameraPosPS.z };
		camLo = { (float)(renderingCameraPosPS.x - (double)camHi.x), (float)(renderingCameraPosPS.y - (double)camHi.y), (float)(renderingCameraPosPS.z - (double)camHi.z) };

		mCamHiPS = Vector3(camHi.x, camHi.y, camHi.z);

		DirectX::XMFLOAT3 relHi = { (float)planetCenterWS.x, (float)planetCenterWS.y, (float)planetCenterWS.z };
		DirectX::XMFLOAT3 relLo = { (float)(planetCenterWS.x - (double)relHi.x), (float)(planetCenterWS.y - (double)relHi.y), (float)(planetCenterWS.z - (double)relHi.z) };

		float radiusF = mRadius;
		uint32_t patchLevels = mPatchLevels;
		mPlanetMeshBuffer.Write((uint8_t*)&radiusF, 4, 0);
		mPlanetMeshBuffer.Write((uint8_t*)&camHi, 12, 4);
		mPlanetMeshBuffer.Write((uint8_t*)&viewMatrixPlanetRendering, 64, 16);
		mPlanetMeshBuffer.Write((uint8_t*)&patchLevels, 4, 80);
		mPlanetMeshBuffer.Write((uint8_t*)&camLo, 12, 84);
		mPlanetMeshBuffer.Write((uint8_t*)&relHi, 12, 96);
		mPlanetMeshBuffer.Write((uint8_t*)&materialCount, 4, 108);
		mPlanetMeshBuffer.Write((uint8_t*)&relLo, 12, 112);
		mPlanetMeshCBuffer->Map(mPlanetMeshBuffer);

		if (mDistanceLUTIsDirty)
			GenerateDistanceLUT();

		if (mFaceLevelDotLUTIsDirty)
			GenerateFaceDotLevelLUT();

		if (mHeightMultLUTIsDirty)
			GenerateHeightMultLUT();

		if (mPatchIsDirty)
			GeneratePatchGeometry();

		// Updating backface culling safe distance based on camera altitude, to avoid popping culling triangles closer to the camera, this was a bug when camera where below hills before
		{
			double camAlt = std::max(Vector3::Length(cameraPosPS) - mRadius, 1.0);
			double safeDist = mRadius * 0.1 + camAlt * 2.0;
			mBackfaceSafeDistSq = safeDist * safeDist;
		}

		mPatches.clear();

		//TOAST_CORE_INFO("Recurse: %d calls, %d height samples, %d frustum checks, %d midpoint lookups",	sRecurseCalls, sHeightSamples, sFrustumChecks, sMidpointLookups);
		sRecurseCalls = 0; sHeightSamples = 0; sFrustumChecks = 0; sMidpointLookups = 0;
		{
			TOAST_PROFILE_SCOPE("Icosphere::Recursion");
			mPatches.clear();
			mDeferredLeafPatches.clear();

			for (size_t i = 0; i < mStartIndices.size(); i += 3)
				RecursiveFace(frustum, mStartIndices[i], mStartIndices[i + 1], mStartIndices[i + 2], 0, cameraPosPS, true);

			for (const auto& lp : mDeferredLeafPatches)
				EmitLeafPatchChecked(lp.ia, lp.ib, lp.ic, lp.subdivision, cameraPosPS);
		}

		{
			TOAST_PROFILE_SCOPE("Icosphere::BuildGPUData");
			BuildGPUData();
		}

		{
			TOAST_PROFILE_SCOPE("Icosphere::UploadGPU");
			if (!mPatchesGPU.empty())
			{
				const uint32_t instBytes = (uint32_t)(mPatchesGPU.size() * sizeof(PlanetPatchGPU));

				// First time or buffer too small — create DYNAMIC with 2x headroom
				if (!mInstanceVertexBuffer || mPatchesGPU.size() > mInstanceBufferCapacity)
				{
					mInstanceBufferCapacity = (uint32_t)(mPatchesGPU.size() * 2);
					uint32_t capacityBytes = mInstanceBufferCapacity * sizeof(PlanetPatchGPU);

					mInstanceVertexBuffer = CreateRef<VertexBuffer>(capacityBytes, mInstanceBufferCapacity, 1);
				}

				// Update count for draw call
				mInstanceVertexBuffer->SetCount((uint32_t)mPatchesGPU.size());
				mInstanceVertexBuffer->SetData(mPatchesGPU.data(), instBytes);
			}
			else
				mInstanceVertexBuffer = nullptr;
		}

		mMeshBaker->EnsureCapacity((uint32_t)mPatchesGPU.size(), (uint32_t)mVertices.size());
		mMeshBaker->Upload(mPatchesGPU, (int)mPatchLevels, materialCount, radiusF, camHi);

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

	void PlanetMesh::RecursiveFace(Frustum* frustum, uint32_t ia, uint32_t ib, uint32_t ic, int16_t subdivision, Vector3& cameraPosPS, bool splitCull)
	{
		//TOAST_PROFILE_FUNCTION();
		sRecurseCalls++;

		const Vector3& va = mSphereVertices[ia];
		const Vector3& vb = mSphereVertices[ib];
		const Vector3& vc = mSphereVertices[ic];

		static constexpr int16_t HEIGHT_SKIP_LEVEL = 4;
		double hA = (subdivision >= HEIGHT_SKIP_LEVEL && mTerrainCubeData) ? GetCachedHeight(ia, subdivision) : 0.0;
		double hB = (subdivision >= HEIGHT_SKIP_LEVEL && mTerrainCubeData) ? GetCachedHeight(ib, subdivision) : 0.0;
		double hC = (subdivision >= HEIGHT_SKIP_LEVEL && mTerrainCubeData) ? GetCachedHeight(ic, subdivision) : 0.0;

		NextPlanetFace nextPlanetFace = CheckFaceSplit(frustum, va, vb, vc, subdivision, cameraPosPS, splitCull, hA, hB, hC);

		if (nextPlanetFace == NextPlanetFace::CULL)
			return;

		if (subdivision < HARDCAPSUBDIVISIONS && (nextPlanetFace == NextPlanetFace::SPLIT || nextPlanetFace == NextPlanetFace::SPLITCULL))
		{
			uint32_t iAB = GetMidpoint(ia, ib);
			uint32_t iBC = GetMidpoint(ib, ic);
			uint32_t iCA = GetMidpoint(ic, ia);

			int16_t nextSubdivision = subdivision + 1;
			bool nextCull = (nextPlanetFace == NextPlanetFace::SPLITCULL);

			RecursiveFace(frustum, iAB, iBC, iCA, nextSubdivision, cameraPosPS, nextCull);
			RecursiveFace(frustum, ia, iAB, iCA, nextSubdivision, cameraPosPS, nextCull);
			RecursiveFace(frustum, iAB, ib, iBC, nextSubdivision, cameraPosPS, nextCull);
			RecursiveFace(frustum, iCA, iBC, ic, nextSubdivision, cameraPosPS, nextCull);
		}
		else
			mDeferredLeafPatches.emplace_back(DeferredLeafPatch{ ia, ib, ic, subdivision });
	}

	void PlanetMesh::EmitLeafPatchChecked(uint32_t ia, uint32_t ib, uint32_t ic, int16_t subdivision, Vector3& cameraPosPS)
	{
		bool hasAB = HasMidpoint(ia, ib);
		bool hasBC = HasMidpoint(ib, ic);
		bool hasCA = HasMidpoint(ic, ia);

		int crackCount = (int)hasAB + (int)hasBC + (int)hasCA;

		if (crackCount == 0)
		{
			mPatches.emplace_back(PlanetPatchCPU(subdivision, ia, ib, ic));
			return;
		}

		if (crackCount == 3)
		{
			// All 3 edges cracked — full 4-way split
			uint32_t iAB = GetMidpoint(ia, ib);
			uint32_t iBC = GetMidpoint(ib, ic);
			uint32_t iCA = GetMidpoint(ic, ia);
			mPatches.emplace_back(PlanetPatchCPU(subdivision, ia, iAB, iCA));
			mPatches.emplace_back(PlanetPatchCPU(subdivision, iAB, ib, iBC));
			mPatches.emplace_back(PlanetPatchCPU(subdivision, iCA, iBC, ic));
			mPatches.emplace_back(PlanetPatchCPU(subdivision, iAB, iBC, iCA));
			return;
		}

		// 1 or 2 cracks — split on the first found edge and recurse.
		// Sub-triangles will pick up any remaining midpoints.
		if (hasAB)
		{
			uint32_t iAB = GetMidpoint(ia, ib);
			EmitLeafPatchChecked(ic, ia, iAB, subdivision, cameraPosPS);
			EmitLeafPatchChecked(ic, iAB, ib, subdivision, cameraPosPS);
			return;
		}
		if (hasBC)
		{
			uint32_t iBC = GetMidpoint(ib, ic);
			EmitLeafPatchChecked(ia, ib, iBC, subdivision, cameraPosPS);
			EmitLeafPatchChecked(ia, iBC, ic, subdivision, cameraPosPS);
			return;
		}
		if (hasCA)
		{
			uint32_t iCA = GetMidpoint(ic, ia);
			EmitLeafPatchChecked(ib, ic, iCA, subdivision, cameraPosPS);
			EmitLeafPatchChecked(ib, iCA, ia, subdivision, cameraPosPS);
			return;
		}
	}

	PlanetMesh::NextPlanetFace PlanetMesh::CheckFaceSplit(Frustum* frustum, const Vector3& a, const Vector3& b, const Vector3& c, int16_t subdivision, Vector3& cameraPosPS, bool frustumCheckNeeded, double hA, double hB, double hC)
	{
		// Actual surface positions
		Vector3 aS = a * (mRadius + hA);
		Vector3 bS = b * (mRadius + hB);
		Vector3 cS = c * (mRadius + hC);

		Vector3 aToCam = aS - cameraPosPS;
		Vector3 bToCam = bS - cameraPosPS;
		Vector3 cToCam = cS - cameraPosPS;
		double aD2 = Vector3::LengthSquared(aToCam);
		double bD2 = Vector3::LengthSquared(bToCam);
		double cD2 = Vector3::LengthSquared(cToCam);

		if (mBackfaceCulling)
		{
			double minDist2 = std::min({ aD2, bD2, cD2 });

			if (minDist2 > mBackfaceSafeDistSq)
			{
				Vector3 center = (aS + bS + cS) / 3.0;
				double centerLenSq = Vector3::LengthSquared(center);
				Vector3 toCamera = center - cameraPosPS;
				double toCameraLenSq = Vector3::LengthSquared(toCamera);
				double dot = Vector3::Dot(center, toCamera);

				double threshold = mFaceLevelDotLUT[subdivision];

				// dot / (|center| * |toCamera|) >= threshold
				// dot >= threshold * |center| * |toCamera|
				// For positive threshold: dot² >= threshold² * centerLenSq * toCameraLenSq (when dot > 0)
				if (dot > 0.0)
				{
					if (dot * dot >= threshold * threshold * centerLenSq * toCameraLenSq)
						return NextPlanetFace::CULL;
				}
			}
		}

		bool frustumKnown = false;
		VolumeTri frustumResult = VolumeTri::INTERSECT;

		if (mFrustumCulling && frustumCheckNeeded)
		{
			//TOAST_PROFILE_SCOPE("Frustum");

			frustumKnown = true;

			double minH = std::min({ hA, hB, hC });
			double padding = mMaxHeight - minH;

			sFrustumChecks++;

			frustumResult = frustum->ContainsPatchSphere(aS, bS, cS, mRadius + padding, subdivision);

			if (frustumResult == VolumeTri::OUTSIDE)
				return NextPlanetFace::CULL;
		}

		if (subdivision >= mMaxSubdivisions)
			return NextPlanetFace::LEAF;

		if (subdivision <= MINGUARANTEEDSUBDIVISION)
			return NextPlanetFace::SPLITCULL;

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
		else
			return NextPlanetFace::LEAF;
	}

	void PlanetMesh::BuildGPUData()
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
			const Vector3& V0 = mSphereVertices[p.i0];
			const Vector3& V1 = mSphereVertices[p.i1];
			const Vector3& V2 = mSphereVertices[p.i2];

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

			Vector3 patchCenter = (V0 + V1 + V2) / 3.0 * mRadius;
			double gridStep = 100000.0; // 100km
			Vector3 patchOrigin = Vector3(
				floor(patchCenter.x / gridStep) * gridStep,
				floor(patchCenter.y / gridStep) * gridStep,
				floor(patchCenter.z / gridStep) * gridStep
			);
			g.PatchOriginPS = { (float)patchOrigin.x, (float)patchOrigin.y, (float)patchOrigin.z };

			mPatchesGPU.emplace_back(g);
		}
	}

	void PlanetMesh::BindGPUData()
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

			if (mMeshBaker->GetBakedVertices())
				RenderCommand::SetShaderResource(D3D11_VERTEX_SHADER, 5, mMeshBaker->GetBakedVertices()->GetSRV());
		}
	}

	void PlanetMesh::UnbindGPUData()
	{
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> nullSRV = nullptr;
		RenderCommand::SetShaderResource(D3D11_VERTEX_SHADER, 5, nullSRV);
	}

	void PlanetMesh::Bake()
	{
		mMeshBaker->Bake();
	}

	uint32_t PlanetMesh::GetMidpoint(uint32_t i1, uint32_t i2)
	{
		sMidpointLookups++;

		uint64_t smaller = std::min(i1, i2);
		uint64_t larger = std::max(i1, i2);
		uint64_t key = (smaller << 32) | larger;

		uint32_t cached = mMidpointCache.find(key);
		if (cached != MidpointHash::EMPTY)
			return cached;

		const Vector3& v1 = mSphereVertices[i1];
		const Vector3& v2 = mSphereVertices[i2];
		Vector3 mid = Vector3::Normalize((v1 + v2) * 0.5);

		uint32_t idx = (uint32_t)mSphereVertices.size();
		mSphereVertices.push_back(mid);
		mMidpointCache.insert(key, idx);
		return idx;
	}

	void PlanetMesh::GenerateDistanceLUT()
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

	void PlanetMesh::GenerateFaceDotLevelLUT()
	{
		double cullingAngle = acos((mRadius) / ((mRadius)+mMaxHeight));

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

	void PlanetMesh::GenerateHeightMultLUT()
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

	double PlanetMesh::GetCachedHeight(uint32_t idx, int16_t subdivision)
	{
		if (idx >= mHeightCache.size())
			mHeightCache.resize(idx + 1, DBL_MAX);

		if (mHeightCache[idx] == DBL_MAX)
		{
			sHeightSamples++;

			static constexpr int16_t BILINEAR_LEVEL = 15;

			if (subdivision >= BILINEAR_LEVEL)
				mHeightCache[idx] = (double)SampleHeightFromDir(*mTerrainCubeData, mSphereVertices[idx]);
			else
				mHeightCache[idx] = (double)SampleHeightNearest(*mTerrainCubeData, mSphereVertices[idx]);
		}

		return mHeightCache[idx];
	}

	bool PlanetMesh::HasMidpoint(uint32_t i1, uint32_t i2) const
	{
		uint64_t smaller = std::min(i1, i2);
		uint64_t larger = std::max(i1, i2);
		uint64_t key = (smaller << 32) | larger;
		return mMidpointCache.find(key) != MidpointHash::EMPTY;
	}

}