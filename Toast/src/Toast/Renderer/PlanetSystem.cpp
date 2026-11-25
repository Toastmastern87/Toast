#include "tpch.h"

#include "PlanetSystem.h"

#include "Toast/Scene/Components.h"

#include "Toast/Core/Math/Math.h"

#include "Toast/Renderer/TerrainSampler.h"
#include "Toast/Renderer/SamplerStates.h"

#include "Toast/Renderer/PlanetHeightDetails.h"

#include <random>

namespace Toast {

	template<typename T>
	T ReadPixel(const uint8_t* srcByte);

	template<>
	float ReadPixel<float>(const uint8_t* srcByte)
	{
		return *reinterpret_cast<const float*>(srcByte);
	}

	template<>
	uint32_t ReadPixel<uint32_t>(const uint8_t* srcByte)
	{
		// R8G8B8A8 packed: R in lowest byte, A in highest
		return  (uint32_t)srcByte[0]
			| ((uint32_t)srcByte[1] << 8)
			| ((uint32_t)srcByte[2] << 16)
			| ((uint32_t)srcByte[3] << 24);
	}

	template<typename T>
	size_t BytesPerPixel();

	template<> size_t BytesPerPixel<float>() { return 4; }
	template<> size_t BytesPerPixel<uint32_t >() { return 4; }

	Planet::Planet()
	{
	}

	void Planet::Initialize()
	{
		mMesh = CreateRef<PlanetMesh>();
		mMesh->Init();

		mHeightMapToCubeMapShaderHandle = AssetManager::GetEngineShaderHandle("HeightMapToCubeMap");
		mHeightCubeToNormalCubeShaderHandle = AssetManager::GetEngineShaderHandle("HeightCubeToNormalCube");
		mHeightCubeMaxMipShaderHandle = AssetManager::GetEngineShaderHandle("HeightCubeMaxMip");
		mAlbedoMapToCubeShaderHandle = AssetManager::GetEngineShaderHandle("AlbedoMapToCube");

		// Setting up Constant Buffers
		mPlanetFrameCBuffer = ConstantBufferLibrary::Load("PlanetFrame", 112, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_VERTEX_SHADER, CBufferBindSlot::PlanetFrame), CBufferBindInfo(D3D11_PIXEL_SHADER, CBufferBindSlot::PlanetFrame), CBufferBindInfo(D3D11_COMPUTE_SHADER, CBufferBindSlot::PlanetFrame) });
		mPlanetFrameCBuffer->Bind();
		mPlanetFrameBuffer.Allocate(mPlanetFrameCBuffer->GetSize());
		mPlanetFrameBuffer.ZeroInitialize();

		mRenderingSettingsCBuffer = ConstantBufferLibrary::Load("PlanetRenderingSettings", 160, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_VERTEX_SHADER, (CBufferBindSlot)5), CBufferBindInfo(D3D11_PIXEL_SHADER, (CBufferBindSlot)5), CBufferBindInfo(D3D11_COMPUTE_SHADER, (CBufferBindSlot)11) });
		mRenderingSettingsCBuffer->Bind();
		mRenderingSettingsBuffer.Allocate(mRenderingSettingsCBuffer->GetSize());
		mRenderingSettingsBuffer.ZeroInitialize();

		// Create texture for Starfield skybox
		mStarFieldTextureCube = CreateRef<TextureCube>(DXGI_FORMAT_R16G16B16A16_UNORM, DXGI_FORMAT_UNKNOWN, 2048, 2048, D3D11_USAGE_DEFAULT, (D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_RENDER_TARGET), 1, 0, 0);

		// Create textures for Atmospheric Scattering
		mTransmittanceLUT = CreateRef<Texture2D>(DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT, 512, 256, D3D11_USAGE_DEFAULT, (D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS), 1, 0);

		mMultiScatteringLUT = CreateRef<Texture2D>(DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT, 512, 256, D3D11_USAGE_DEFAULT, (D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS), 1, 0);

		mGroundIrradianceLUT = CreateRef<Texture2D>(DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT, 128, 32, D3D11_USAGE_DEFAULT, (D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS), 1, 0);

		mSkyViewLUT = CreateRef<Texture2D>(DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT, 256, 128, D3D11_USAGE_DEFAULT, (D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS), 1, 0);

		mAerielPerspectiveLUT = CreateRef<Texture3D>(DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT, 192, 108, 128, D3D11_USAGE_DEFAULT, (D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS), 0);

		mAPFar = CreateRef<Texture2D>(DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R32_UINT, 1, 1, D3D11_USAGE_DEFAULT,	(D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS), 1, 0);
	}

	// During runtime playerCamPosWS and RendererCamPosWS will be the same, but they can differ in the editor when the player camera is detached from the rendering camera. The worldTranslation is used to shift the planet's position in world space, allowing for large world coordinates without precision issues. The renderingCameraViewMatrix is used for rendering the planet correctly in the editor viewport
	void Planet::OnUpdate(Camera* camera, const Quaternion& playerCamRot, const Vector3& playerCamPosWS, const Vector3& renderingCamPosWS, const Vector3& worldTranslation, Frustum* frustum, const DirectX::XMFLOAT4X4& renderingCameraViewMatrix, float frustumBias)
	{
		TOAST_PROFILE_FUNCTION();

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
		 
		mCameraPlanetSpace = Vector3::Rotate(playerCamRel, mInvRotationQuat);
		Vector3 renderingCamPosPS = Vector3::Rotate(renderingCamRel, mInvRotationQuat);

		frustum->UpdatePlanetSpace(mCameraPlanetSpace, playerCamRightWS, playerCamUpWS, playerCamForwardWS, mInvRotationQuat, camera->GetNearClip(), camera->GetFarClip(), camera->GetVerticalFOV(), camera->GetAspectRatio(), frustumBias);

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
		mBasisTanEast = cb.BasisTanEast;
		mBasisTanNorth = cb.BasisTanNorth;

		mPlanetFrameBuffer.Write(reinterpret_cast<uint8_t*>(&cb), sizeof(cb), 0);
		mPlanetFrameCBuffer->Map(mPlanetFrameBuffer);

		mMesh->OnUpdate(frustum, viewMatrixPlanetRendering, mCameraPlanetSpace, renderingCamPosPS, planetPosWS, mRadius, mMaxHeight, &mTerrainCubeData, mMaterials.size());

		if (mMaterialsIsDirty)
			UploadMaterialsToGPU();

		if (mPBRTexturesDirty)
			RebuildPBRTextureArrays();
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
		const uint32_t mipCount = 12;

		Ref<TextureCube> heightMapCube = CreateRef<TextureCube>("HeightMapCube", DXGI_FORMAT_R32_FLOAT, cubemapSize, cubemapSize, mipCount);
		 
		heightMapCube->CreateUAV(0);

		auto shader = AssetManager::GetAsset<Shader>(mHeightMapToCubeMapShaderHandle);
		if (shader) 
			shader->Bind();

		if (!heightMapTexture)
		{
			TOAST_CORE_WARN("Planet heightmap handle %llu is not valid, this might be cause the project has no planet set up yet; skipping cube creation.", (uint64_t)heightMapTexture);
			return nullptr; 
		}

		heightMapTexture->Bind(0, D3D11_COMPUTE_SHADER);
		RenderCommand::BindSampler(D3D11_COMPUTE_SHADER, 0, SamplerStates::Get(SamplerType::UWrapVClamp));

		heightMapCube->BindForReadWrite(0, D3D11_COMPUTE_SHADER);

		const uint32_t groupsX = (cubemapSize + 31) / 32;
		const uint32_t groupsY = (cubemapSize + 31) / 32;
		RenderCommand::DispatchCompute(groupsX, groupsY, 6);

		heightMapCube->UnbindUAV();

		BuildHeightMaxMips(heightMapCube);

		return heightMapCube;
	}

	void Planet::BuildHeightMaxMips(const Ref<TextureCube>& heightCube)
	{
		auto shader = AssetManager::GetAsset<Shader>(mHeightCubeMaxMipShaderHandle);
		if (!shader || !heightCube)
			return;

		shader->Bind();

		const uint32_t mipCount = heightCube->GetMipLevelCount();
		const uint32_t size0 = heightCube->GetWidth();

		for (uint32_t mip = 1; mip < mipCount; ++mip)
		{
			const uint32_t size = std::max(size0 >> mip, 1u);
			const uint32_t groups = (size + 7) / 8;

			for (uint32_t face = 0; face < 6; ++face)
			{
				heightCube->BindForReadWriteUpdated(0, D3D11_COMPUTE_SHADER, mip - 1, face);
				heightCube->BindForReadWriteUpdated(1, D3D11_COMPUTE_SHADER, mip, face);
				RenderCommand::DispatchCompute(groups, groups, 1);
			}
		}

		heightCube->UnbindUAVUpdated(0, D3D11_COMPUTE_SHADER);
		heightCube->UnbindUAVUpdated(1, D3D11_COMPUTE_SHADER);
	}

	Ref<TextureCube> Planet::CreateNormalMapCube(const TextureCube* heightCube)
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		const uint32_t cubemapSize = 2048;

		Ref<TextureCube> normalCube = CreateRef<TextureCube>("NormalMapCube", DXGI_FORMAT_R8G8B8A8_UNORM, cubemapSize, cubemapSize);
		normalCube->CreateUAV(0);

		auto shader = AssetManager::GetAsset<Shader>(mHeightCubeToNormalCubeShaderHandle);
		if (shader)
			shader->Bind();

		if (!heightCube)
		{
			TOAST_CORE_WARN("Planet heightmap handle %llu is not valid, this might be cause the project has no planet set up yet; skipping cube creation.", (uint64_t)heightCube);
			return nullptr;
		}

		heightCube->Bind(0, D3D11_COMPUTE_SHADER);      // t0
		RenderCommand::BindSampler(D3D11_COMPUTE_SHADER, 0, SamplerStates::Get(SamplerType::UWrapVClamp));
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

		Ref<TextureCube> albedoCube = CreateRef<TextureCube>("AlbedoCube", DXGI_FORMAT_R8G8B8A8_UNORM, cubemapSize, cubemapSize);
		albedoCube->CreateUAV(0);

		auto shader = AssetManager::GetAsset<Shader>(mAlbedoMapToCubeShaderHandle);
		if (shader)
			shader->Bind();

		if (!albedoTexture)
		{
			TOAST_CORE_WARN("Planet heightmap handle %llu is not valid, this might be cause the project has no planet set up yet; skipping cube creation.", (uint64_t)albedoTexture);
			return nullptr;
		}

		albedoTexture->Bind(0, D3D11_COMPUTE_SHADER);
		RenderCommand::BindSampler(D3D11_COMPUTE_SHADER, 0, SamplerStates::Get(SamplerType::UWrapVClamp));
		albedoCube->BindForReadWrite(0, D3D11_COMPUTE_SHADER);

		const uint32_t groupsX = (cubemapSize + 31) / 32;
		const uint32_t groupsY = (cubemapSize + 31) / 32;
		RenderCommand::DispatchCompute(groupsX, groupsY, 6);

		albedoCube->UnbindUAV();
		return albedoCube;
	}

	double Planet::GetHeightDetailsAtDir(const Vector3& dirPlanet, const DirectX::XMVECTOR& cameraPlanetSpace) const
	{
		DirectX::XMVECTOR dir = DirectX::XMVectorSet((float)dirPlanet.x, (float)dirPlanet.y, (float)dirPlanet.z, 0.0f);
		dir = DirectX::XMVector3Normalize(dir);

		// Base height
		float baseHeight = SampleHeightFromDir(mTerrainCubeData, dirPlanet);

		// Wall steepening
		float wallMask;
		float wallBoost = ComputeWallSteepenBoost(*this, dirPlanet, baseHeight, wallMask);
		float wallEnhancedHeight = baseHeight + wallBoost;

		// Erosion
		float erosionMask, erosionPattern;
		float erosionDelta = ComputeRuneStyleErosionCPU(*this, dir, cameraPlanetSpace, wallEnhancedHeight, erosionMask, erosionPattern);

		float slope = ComputeBaseSlope(*this, dirPlanet);
		float colorAvg = SampleColorAvgFromDir(mAlbedoCubeData, dirPlanet);
		float materialNoise = ComputeMaterialNoiseCPU(*this, dirPlanet, slope, colorAvg, cameraPlanetSpace);

		return (double)wallBoost + (double)erosionDelta + (double)materialNoise;
	}

	static float Smoothstep(float a, float b, float x)
	{
		float t = std::clamp((x - a) / (b - a), 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}

	void Planet::UploadMaterialsToGPU()
	{
		const uint32_t materialCount = (uint32_t)mMaterials.size();
		if (materialCount == 0)
		{
			mMaterialSB.reset();
			mMaterialNoiseSB.reset();
			mMaterialNoisePermSB.reset();
			mMaterialsIsDirty = false;
			return;
		}

		// --- Count total noise layers across all materials ---
		uint32_t totalNoiseLayers = 0;
		for(const auto& material : mMaterials)
			totalNoiseLayers += (uint32_t)material.NoiseLayers.size();

		// --- Pack flat arrays ---
		std::vector<PlanetMaterial::GPUData> materialsGPU(materialCount);
		std::vector<NoiseLayer::GPUData> noiseGPU(totalNoiseLayers);
		std::vector<Int4> permTables(totalNoiseLayers * 64);

		uint32_t noiseOffset = 0;

		for (uint32_t m = 0; m < materialCount; ++m) 
		{
			PlanetMaterial& material = mMaterials[m];
			const uint32_t layerCount = (uint32_t)material.NoiseLayers.size();

			// Inject indirection indices
			PlanetMaterial::GPUData& GPUMaterial = material.GPU;
			GPUMaterial.NoiseLayerStart = (int32_t)noiseOffset;
			GPUMaterial.NoiseLayerCount = (int32_t)layerCount;

			GPUMaterial.DebugColor = material.GPU.DebugColor;

			// Mirror PBR settings into GPU data
			GPUMaterial.UVTilingScale = material.PBR.TilingScale;

			materialsGPU[m] = GPUMaterial;

			for (uint32_t n = 0; n < layerCount; ++n)
			{
				NoiseLayer& layer = material.NoiseLayers[n];

				BuildPermutationTable(layer.Seed, layer.Perm);

				const int32_t permBase = (int32_t)((noiseOffset + n) * 64);

				for (int k = 0; k < 64; ++k)
				{
					const int idx = k * 4;
					permTables[permBase + k] = Int4(
						(int32_t)layer.Perm[idx + 0], 
						(int32_t)layer.Perm[idx + 1], 
						(int32_t)layer.Perm[idx + 2], 
						(int32_t)layer.Perm[idx + 3]
					);
				}

				NoiseLayer::GPUData GPUNoise = layer.GPU;
				GPUNoise.PermBase = permBase;
				noiseGPU[noiseOffset + n] = GPUNoise;
			}

			noiseOffset += layerCount;
		}

		// --- Recreate buffers if counts changed ---
		if (materialCount != mLastMaterialCount)
		{
			mMaterialSB = CreateRef<StructuredBuffer>((uint32_t)sizeof(PlanetMaterial::GPUData), materialCount, D3D11_USAGE_DYNAMIC);
			mLastMaterialCount = materialCount;
		}

		if (totalNoiseLayers != mLastMaterailNoiseCount)
		{
			mMaterialNoiseSB = CreateRef<StructuredBuffer>((uint32_t)sizeof(NoiseLayer::GPUData), totalNoiseLayers, D3D11_USAGE_DYNAMIC);
			mMaterialNoisePermSB = CreateRef<StructuredBuffer>((uint32_t)sizeof(Int4), totalNoiseLayers * 64, D3D11_USAGE_DYNAMIC);
			mLastMaterailNoiseCount = totalNoiseLayers;
		}

		// --- Upload to GPU ---
		mMaterialSB->Update(materialsGPU.data(), materialsGPU.size() * sizeof(PlanetMaterial::GPUData));
		mMaterialNoiseSB->Update(noiseGPU.data(), noiseGPU.size() * sizeof(NoiseLayer::GPUData));
		mMaterialNoisePermSB->Update(permTables.data(), permTables.size() * sizeof(Int4));

		mMaterialsIsDirty = false;
	}

	void Planet::RebuildPBRTextureArrays()
	{
		const uint32_t matCount = (uint32_t)mMaterials.size();

		if (matCount == 0)
		{
			mPBRAlbedoArray.reset();
			mPBRNormalArray.reset();
			mPBRRoughnessArray.reset();
			mPBRAOArray.reset();
			mPBRDisplacementArray.reset();
			mPBRTexturesDirty = false;
			mLastPBRMaterialCount = 0;
			mLastPBRTextureSize = 0;
			return;
		}

		//  Find reference size from the first valid albedo texture
		uint32_t refWidth = 0;
		uint32_t refHeight = 0;
		DXGI_FORMAT albedoFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		DXGI_FORMAT normalFormat = DXGI_FORMAT_R16G16B16A16_UNORM;
		DXGI_FORMAT roughnessFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		DXGI_FORMAT aoFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		DXGI_FORMAT dispFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

		for (const auto& mat : mMaterials)
		{
			if (mat.PBR.AlbedoHandle != 0)
			{
				auto tex = AssetManager::GetAsset<Texture2D>(AssetHandle(mat.PBR.AlbedoHandle));
				if (tex && refWidth == 0)
				{
					refWidth = tex->GetWidth();
					refHeight = tex->GetHeight();
					albedoFormat = tex->GetFormat();
				}
			}
			if (mat.PBR.NormalHandle != 0)
			{
				auto tex = AssetManager::GetAsset<Texture2D>(AssetHandle(mat.PBR.NormalHandle));
				if (tex) 
					normalFormat = tex->GetFormat();
			}
			if (mat.PBR.RoughnessHandle != 0)
			{
				auto tex = AssetManager::GetAsset<Texture2D>(AssetHandle(mat.PBR.RoughnessHandle));
				if (tex) 
					roughnessFormat = tex->GetFormat();
			}
			if (mat.PBR.AOHandle != 0)
			{
				auto tex = AssetManager::GetAsset<Texture2D>(AssetHandle(mat.PBR.AOHandle));
				if (tex) 
					aoFormat = tex->GetFormat();
			}
			if (mat.PBR.DisplacementHandle != 0)
			{
				auto tex = AssetManager::GetAsset<Texture2D>(AssetHandle(mat.PBR.DisplacementHandle));
				if (tex) 
					dispFormat = tex->GetFormat();
			}
		}

		//  Recreate arrays when count or size changes.
		const bool needRecreate = (matCount != mLastPBRMaterialCount) || (refWidth != mLastPBRTextureSize) || !mPBRAlbedoArray;

		if (needRecreate)
		{
			mPBRAlbedoArray = CreateRef<Texture2DArray>(albedoFormat, refWidth, refHeight, matCount, true /*generateMips*/);

			mPBRNormalArray = CreateRef<Texture2DArray>(normalFormat, refWidth, refHeight, matCount, true);

			mPBRRoughnessArray = CreateRef<Texture2DArray>(roughnessFormat, refWidth, refHeight, matCount, true);

			mPBRAOArray = CreateRef<Texture2DArray>(aoFormat, refWidth, refHeight, matCount, true);

			mPBRDisplacementArray = CreateRef<Texture2DArray>(dispFormat, refWidth, refHeight, matCount, true);

			mLastPBRMaterialCount = matCount;
			mLastPBRTextureSize = refWidth;
		}

		// ────────────────────────────────────────────────────────────
		//  Helper lambda: copy texture into slice, or fill with fallback
		// ────────────────────────────────────────────────────────────
		auto CopyOrFill = [&](uint64_t handle, Ref<Texture2DArray>& targetArray, uint32_t sliceIndex, uint32_t fallbackColor)
			{
				auto tex = (handle != 0) ? AssetManager::GetAsset<Texture2D>(AssetHandle(handle)) : nullptr;

				if (tex && tex->GetWidth() == refWidth && tex->GetHeight() == refHeight)
					targetArray->CopyFromTexture(tex.get(), sliceIndex);
				else
					targetArray->FillSliceSolid(sliceIndex, fallbackColor);
			};

		// ────────────────────────────────────────────────────────────
		//  Fill each slice from each material's assigned texture
		// ────────────────────────────────────────────────────────────
		for (uint32_t m = 0; m < matCount; ++m)
		{
			const PBRMaterialLayer& pbr = mMaterials[m].PBR;

			CopyOrFill(pbr.AlbedoHandle, mPBRAlbedoArray, m, 0xFF808080); // mid-gray
			CopyOrFill(pbr.NormalHandle, mPBRNormalArray, m, 0xFFFF8080); // flat normal
			CopyOrFill(pbr.RoughnessHandle, mPBRRoughnessArray, m, 0xFF808080); // mid roughness
			CopyOrFill(pbr.AOHandle, mPBRAOArray, m, 0xFFFFFFFF); // no occlusion
			CopyOrFill(pbr.DisplacementHandle, mPBRDisplacementArray, m, 0xFF808080); // neutral
		}

		mPBRAlbedoArray->GenerateMips();
		mPBRNormalArray->GenerateMips();
		mPBRRoughnessArray->GenerateMips();
		mPBRAOArray->GenerateMips();
		mPBRDisplacementArray->GenerateMips();

		mPBRTexturesDirty = false;
	}

	void Planet::MapRenderingSettings()
	{
		uint32_t materialCount = (uint32_t)mMaterials.size();

		mRenderingSettingsBuffer.Write((uint8_t*)&materialCount, 4, 0);
		uint32_t enabled = mMaterialsEnabled ? 1 : 0;
		mRenderingSettingsBuffer.Write((uint8_t*)&enabled, 4, 4);
		mRenderingSettingsBuffer.Write((uint8_t*)&mPBRColorDominance, 4, 8);
		mRenderingSettingsBuffer.Write((uint8_t*)&mColorNoiseFrequency, 4, 12);

		mRenderingSettingsBuffer.Write((uint8_t*)&mColorNoiseStrength, 4, 16);
		mRenderingSettingsBuffer.Write((uint8_t*)&mColorNoiseOctaves, 4, 20);
		float wallEnhancementEnabled = mWallEnhancementEnabled ? 1.0f : 0.0f;
		mRenderingSettingsBuffer.Write((uint8_t*)&wallEnhancementEnabled, 4, 24);
		mRenderingSettingsBuffer.Write((uint8_t*)&mWallStrength, 4, 28);

		mRenderingSettingsBuffer.Write((uint8_t*)&mWallStepMeters, 4, 32);
		mRenderingSettingsBuffer.Write((uint8_t*)&mWallSlopeStart, 4, 36);
		mRenderingSettingsBuffer.Write((uint8_t*)&mWallSlopeEnd, 4, 40);
		mRenderingSettingsBuffer.Write((uint8_t*)&mWallSharpStart, 4, 44);

		mRenderingSettingsBuffer.Write((uint8_t*)&mWallSharpEnd, 4, 48);
		mRenderingSettingsBuffer.Write((uint8_t*)&mWallMaxDelta, 4, 52);
		float wallDebugEnabled = mWallDebugEnabled ? 1.0f : 0.0f;
		mRenderingSettingsBuffer.Write((uint8_t*)&wallDebugEnabled, 4, 56);
		float wallDebugMode = static_cast<float>(mWallDebugMode);
		mRenderingSettingsBuffer.Write((uint8_t*)&wallDebugMode, 4, 60);

		mRenderingSettingsBuffer.Write((uint8_t*)&mTerrainNormalStepMeters, 4, 64);
		float erosionEnabled = mErosionEnabled ? 1.0f : 0.0f;
		mRenderingSettingsBuffer.Write((uint8_t*)&erosionEnabled, 4, 68);
		mRenderingSettingsBuffer.Write((uint8_t*)&mErosionStrength, 4, 72);
		mRenderingSettingsBuffer.Write((uint8_t*)&mErosionStepMeters, 4, 76);

		mRenderingSettingsBuffer.Write((uint8_t*)&mErosionTilingMeters, 4, 80);
		mRenderingSettingsBuffer.Write((uint8_t*)&mErosionSlopeStart, 4, 84);
		mRenderingSettingsBuffer.Write((uint8_t*)&mErosionSlopeFull, 4, 88);
		mRenderingSettingsBuffer.Write((uint8_t*)&mErosionSlopeEnd, 4, 92);

		mRenderingSettingsBuffer.Write((uint8_t*)&mErosionSlopeFadeOut, 4, 96);
		mRenderingSettingsBuffer.Write((uint8_t*)&mErosionOctaves, 4, 100);
		mRenderingSettingsBuffer.Write((uint8_t*)&mErosionLacunarity, 4, 104);
		mRenderingSettingsBuffer.Write((uint8_t*)&mErosionPersistence, 4, 108);

		float erosionDebugEnabled = mErosionDebugEnabled ? 1.0f : 0.0f;
		mRenderingSettingsBuffer.Write((uint8_t*)&erosionDebugEnabled, 4, 112);
		mRenderingSettingsBuffer.Write((uint8_t*)&mErosionDebugMode, 4, 116);
		mRenderingSettingsBuffer.Write((uint8_t*)&mErosionGullyWeight, 4, 120);
		mRenderingSettingsBuffer.Write((uint8_t*)&mErosionDetail, 4, 124);

		mRenderingSettingsBuffer.Write((uint8_t*)&mErosionCellScale, 4, 128);
		mRenderingSettingsBuffer.Write((uint8_t*)&mErosionNormalization, 4, 132);
		mRenderingSettingsBuffer.Write((uint8_t*)&mErosionAssumedSlope, 4, 136);
		mRenderingSettingsBuffer.Write((uint8_t*)&mErosionAssumedSlopeBlend, 4, 140);

		mRenderingSettingsBuffer.Write((uint8_t*)&mErosionMaxDistance, 4, 144);
		mRenderingSettingsBuffer.Write((uint8_t*)&mErosionFadeStart, 4, 148);

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


	template<typename T>
	CubeData<T> Planet::LoadCubeData(const Ref<TextureCube>& source)
	{
		CubeData<T> cd{};

		ID3D11Device* device = RenderCommand::sRendererAPI->GetDevice();
		ID3D11DeviceContext* context = RenderCommand::sRendererAPI->GetDeviceContext();

		if (!source)
			return cd;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> tex = source->GetTexture();
		TOAST_CORE_ASSERT(tex, "TextureCube has no underlying texture!");

		D3D11_TEXTURE2D_DESC desc{};
		tex->GetDesc(&desc);

		const UINT mipLevel = 0;
		const UINT faceCount = desc.ArraySize;
		TOAST_CORE_ASSERT(faceCount == 6, "Cube should have 6 faces.");

		cd.Width = desc.Width;
		cd.Height = desc.Height;

		// Create staging texture
		D3D11_TEXTURE2D_DESC stagingDesc = desc;
		stagingDesc.Usage = D3D11_USAGE_STAGING;
		stagingDesc.BindFlags = 0;
		stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		stagingDesc.MiscFlags = 0;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> stagingTex;
		HRESULT hr = device->CreateTexture2D(&stagingDesc, nullptr, &stagingTex);
		TOAST_CORE_ASSERT(SUCCEEDED(hr), "Failed to create staging texture for cube readback.");

		const size_t bpp = BytesPerPixel<T>();

		for (UINT face = 0; face < faceCount; ++face)
		{
			UINT subresource = D3D11CalcSubresource(mipLevel, face, desc.MipLevels);
			context->CopySubresourceRegion(
				stagingTex.Get(), subresource,
				0, 0, 0,
				tex.Get(), subresource,
				nullptr);

			D3D11_MAPPED_SUBRESOURCE mapped{};
			hr = context->Map(stagingTex.Get(), subresource, D3D11_MAP_READ, 0, &mapped);
			TOAST_CORE_ASSERT(SUCCEEDED(hr), "Failed to map staging cube.");

			const uint8_t* srcBytes = static_cast<const uint8_t*>(mapped.pData);
			const size_t rowPitchBytes = mapped.RowPitch;

			cd.FaceData[face].resize(static_cast<size_t>(cd.Width) * cd.Height);

			for (uint32_t y = 0; y < cd.Height; ++y)
			{
				const uint8_t* srcRow = srcBytes + y * rowPitchBytes;
				for (uint32_t x = 0; x < cd.Width; ++x)
				{
					cd.FaceData[face][Index2D(x, y, cd.Width)] =
						ReadPixel<T>(srcRow + x * bpp);
				}
			}

			context->Unmap(stagingTex.Get(), subresource);
		}

		return cd;
	}

	template CubeData<float> Planet::LoadCubeData<float>(const Ref<TextureCube>& source);
	template CubeData<uint32_t> Planet::LoadCubeData<uint32_t>(const Ref<TextureCube>& source);

}