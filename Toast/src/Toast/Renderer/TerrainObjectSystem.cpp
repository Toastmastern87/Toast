#include "tpch.h"

#include "TerrainObjectSystem.h"

#include "Toast/Renderer/Renderer.h"

#include "Toast/Renderer/PlanetSystem.h"

namespace Toast {

	bool TerrainObjectSystem::Init() 
	{
		if (mInitialized)
		{
			TOAST_CORE_WARN("TerrainObjectSystem::Init called twice - ignoring");
			return true;
		}

		for (uint32_t i = 0; i < MAX_TERRAIN_OBJECT_TYPES; ++i)
			mInstanceBuffers[i] = CreateRef<StructuredBuffer>(sizeof(TerrainInstanceGPU), MAX_TERRAIN_INSTANCES_PER_TYPE, D3D11_USAGE_DEFAULT, true, false);

		// Indirect args: one DrawIndexedInstancedIndirect block per type.
		{
			RendererAPI* API = RenderCommand::sRendererAPI.get();
			TOAST_CORE_ASSERT(API, "TerrainObjectSystem::Initialize: no RendererAPI! Initialize() must be called AFTER the renderer API exists.");
			if (!API) 
				return false;

			ID3D11Device* device = API->GetDevice();
			TOAST_CORE_ASSERT(device, "TerrainObjectSystem::Initialize: no D3D11 device!");
			if (!device) 
				return false;

			const uint32_t argsBytes = MAX_TERRAIN_OBJECT_TYPES * TERRAIN_ARGS_STRIDE;

			D3D11_BUFFER_DESC bd = {};
			bd.ByteWidth = argsBytes;                       
			bd.Usage = D3D11_USAGE_DEFAULT;
			bd.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
			bd.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS | D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;

			HRESULT hr = device->CreateBuffer(&bd, nullptr, &mIndirectArgs);
			TOAST_CORE_ASSERT(SUCCEEDED(hr), "TerrainObjectSystem: indirect args buffer failed");
			if (FAILED(hr)) 
				return false;

			D3D11_UNORDERED_ACCESS_VIEW_DESC uavd = {};
			uavd.Format = DXGI_FORMAT_R32_TYPELESS;  // required for raw views
			uavd.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			uavd.Buffer.FirstElement = 0;
			uavd.Buffer.NumElements = argsBytes / 4;
			uavd.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;

			hr = device->CreateUnorderedAccessView(mIndirectArgs.Get(), &uavd, &mIndirectArgsUAV);
			TOAST_CORE_ASSERT(SUCCEEDED(hr), "TerrainObjectSystem: indirect args UAV failed");
			if (FAILED(hr)) 
				return false;
		}

		mScatterCBuffer = ConstantBufferLibrary::Load("TerrainScatter", 64, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_COMPUTE_SHADER, CBufferBindSlot(12)) });
		mScatterBuffer.Allocate(mScatterCBuffer->GetSize());
		mScatterBuffer.ZeroInitialize();

		// Making sure everything is reseted and ready to be used
		Reset();

		mInitialized = true;

		TOAST_CORE_INFO("TerrainObjectSystem initialized: %d types x %d instances, %d KB VRAM.", MAX_TERRAIN_OBJECT_TYPES, MAX_TERRAIN_INSTANCES_PER_TYPE, (MAX_TERRAIN_OBJECT_TYPES * MAX_TERRAIN_INSTANCES_PER_TYPE * (uint32_t)sizeof(TerrainInstanceGPU)) / 1024);

		return true;
	}

	void TerrainObjectSystem::LoadShaders()
	{
		mKickoffShaderHandle = AssetManager::GetEngineShaderHandle("TerrainScatterKickoff");
		TOAST_CORE_ASSERT(mKickoffShaderHandle, "TerrainScatterKickoff shader not found in the asset registry!");

		mScatterShaderHandle = AssetManager::GetEngineShaderHandle("TerrainScatter");
		TOAST_CORE_ASSERT(mScatterShaderHandle, "TerrainScatter  shader not found in the asset registry!");
	}

	void TerrainObjectSystem::Reset()
	{
		TOAST_CORE_ASSERT(mIndirectArgs, "TerrainObjectSystem::Reset before Init!");
		if (!mIndirectArgs)
			return;

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		TOAST_CORE_ASSERT(API, "TerrainObjectSystem::Initialize: no RendererAPI! Initialize() must be called AFTER the renderer API exists.");
		if (!API)
			return;

		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		const uint32_t zeros[MAX_TERRAIN_OBJECT_TYPES * (TERRAIN_ARGS_STRIDE / 4)] = {};
		deviceContext->UpdateSubresource(mIndirectArgs.Get(), 0, nullptr, zeros, 0, 0);

		TOAST_CORE_INFO("TerrainObjectSystem reset - %d type arg blocks zeroed.", MAX_TERRAIN_OBJECT_TYPES);
	}

	void TerrainObjectSystem::Scatter(Planet* planet, const Vector3& worldTranslation)
	{
		if (!mInitialized || !planet)
			return;

		const auto& objects = planet->GetTerrainObjects();
		if (objects.empty())
			return;

		uint32_t typeCount = (uint32_t)objects.size();
		if (typeCount > MAX_TERRAIN_OBJECT_TYPES)
		{
			TOAST_CORE_WARN("TerrainObjectSystem: %d terrain object types but only %d supported - extra types are skipped", typeCount, MAX_TERRAIN_OBJECT_TYPES);
			typeCount = MAX_TERRAIN_OBJECT_TYPES;
		}

		Vector3 playerOffsetWS = -worldTranslation;
		float playerTangentEast = (float)Vector3::Dot(playerOffsetWS, planet->GetBasisTanEast());
		float playerTangentNorth = (float)Vector3::Dot(playerOffsetWS, planet->GetBasisTanNorth());

		Ref<Shader> kickoffShader = AssetManager::GetAsset<Shader>(mKickoffShaderHandle);
		Ref<Shader> scatterShader = AssetManager::GetAsset<Shader>(mScatterShaderHandle);

		if(!kickoffShader || !scatterShader)
			return;

		for (uint32_t t = 0; t < typeCount; ++t)
		{
			const TerrainObject& object = objects[t];
			if (!object.MeshObject)
				continue;

			const auto& sub = object.MeshObject->GetSubmeshes(0)[0];

			uint32_t candidateCount = object.CandidateGridSize * object.CandidateGridSize;
			if (candidateCount == 0)
				continue;

			mScatterBuffer.Write((uint8_t*)&t, 4, 0);
			mScatterBuffer.Write((uint8_t*)&object.CandidateGridSize, 4, 4);
			mScatterBuffer.Write((uint8_t*)&candidateCount, 4, 8);
			mScatterBuffer.Write((uint8_t*)&object.Seed, 4, 12);

			mScatterBuffer.Write((uint8_t*)&object.DensityProb, 4, 16);
			mScatterBuffer.Write((uint8_t*)&object.ScatterRadiusMeters, 4, 20);
			mScatterBuffer.Write((uint8_t*)&object.MinScale, 4, 24);
			mScatterBuffer.Write((uint8_t*)&object.MaxScale, 4, 28);

			mScatterBuffer.Write((uint8_t*)&playerTangentEast, 4, 32);
			mScatterBuffer.Write((uint8_t*)&playerTangentNorth, 4, 36);
			mScatterBuffer.Write((uint8_t*)&sub.IndexCount, 4, 40);
			mScatterBuffer.Write((uint8_t*)&sub.BaseIndex, 4, 44);

			float planetRadius = planet->GetRadius();
			Vector3 camHiPSV3 = planet->GetIcosphereMesh()->GetCameraHiPS();
			DirectX::XMFLOAT3 camHiPS = { (float)camHiPSV3.x, (float)camHiPSV3.y, (float)camHiPSV3.z };
			mScatterBuffer.Write((uint8_t*)&planetRadius, 4, 48);
			mScatterBuffer.Write((uint8_t*)&camHiPS, 12, 52);

			mScatterCBuffer->Map(mScatterBuffer);
			mScatterCBuffer->Bind();

			// Bind UAVs
			mInstanceBuffers[t]->BindUAV(0);

			RendererAPI* API = RenderCommand::sRendererAPI.get();
			ID3D11DeviceContext* deviceContext = API->GetDeviceContext();
			ID3D11UnorderedAccessView* argsUAV = mIndirectArgsUAV.Get();
			deviceContext->CSSetUnorderedAccessViews(1, 1, &argsUAV, nullptr);

			// Kickoff
			kickoffShader->Bind();
			RenderCommand::DispatchCompute(1, 1, 1);

			// Scattering, one thread per candidate
			scatterShader->Bind();
			const uint32_t groups = (candidateCount + TERRAIN_SCATTER_THREADGROUP_SIZE - 1) / TERRAIN_SCATTER_THREADGROUP_SIZE;;
			RenderCommand::DispatchCompute(groups, 1, 1);

			mInstanceBuffers[t]->UnbindUAV(0);
			ID3D11UnorderedAccessView* nullUAV = nullptr;
			deviceContext->CSSetUnorderedAccessViews(1, 1, &nullUAV, nullptr);
		}

		RenderCommand::ClearShaderResources();
	}

}