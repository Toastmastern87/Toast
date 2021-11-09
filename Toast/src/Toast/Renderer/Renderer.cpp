#include "tpch.h"
#include "Renderer.h"

#include "Toast/Renderer/Renderer2D.h"
#include "Toast/Renderer/RendererDebug.h"

namespace Toast {

	struct RendererData
	{
		Renderer::Statistics Stats;
	};

	static RendererData sData;

	Scope<Renderer::SceneRendererData> Renderer::mSceneRendererData = CreateScope<Renderer::SceneRendererData>();

	void Renderer::Init()
	{
		TOAST_PROFILE_FUNCTION();

		RenderCommand::Init();
		Renderer2D::Init();
		RendererDebug::Init();

		// Setting up the constant buffer and data buffer for the camera rendering
		mSceneRendererData->mCameraCBuffer = ConstantBufferLibrary::Load("Camera", 208, D3D11_VERTEX_SHADER, 0);
		mSceneRendererData->mCameraCBuffer->Bind();
		mSceneRendererData->mCameraBuffer.Allocate(mSceneRendererData->mCameraCBuffer->GetSize());
		mSceneRendererData->mCameraBuffer.ZeroInitialize();

		//// Setting up the constant buffer and data buffer for lightning rendering
		mSceneRendererData->mLightningCBuffer = ConstantBufferLibrary::Load("DirectionalLight", 48, D3D11_PIXEL_SHADER, 0);
		mSceneRendererData->mLightningCBuffer->Bind();
		mSceneRendererData->mLightningBuffer.Allocate(mSceneRendererData->mLightningCBuffer->GetSize());
		mSceneRendererData->mLightningBuffer.ZeroInitialize();

		// Setting up the constant buffer and data buffer for enviromental rendering
		mSceneRendererData->mEnvironmentCBuffer = ConstantBufferLibrary::Load("Environment", 16, D3D11_PIXEL_SHADER, 3);
		mSceneRendererData->mEnvironmentCBuffer->Bind();
		mSceneRendererData->mEnvironmentBuffer.Allocate(mSceneRendererData->mEnvironmentCBuffer->GetSize());
		mSceneRendererData->mEnvironmentBuffer.ZeroInitialize();
	}

	void Renderer::Shutdown()
	{
		Renderer2D::Shutdown();
		RendererDebug::Shutdown();
	}

	void Renderer::OnWindowResize(uint32_t width, uint32_t height)
	{
		RenderCommand::ResizeViewport(0, 0, width, height);
	}

	void Renderer::BeginScene(const Scene* scene, const Camera& camera, const DirectX::XMMATRIX& viewMatrix, const DirectX::XMFLOAT4 cameraPos)
	{
		TOAST_PROFILE_FUNCTION();

		// Updating the camera data in the buffer and mapping it to the GPU
		mSceneRendererData->mCameraBuffer.Write((void*)&viewMatrix, 64, 0);
		mSceneRendererData->mCameraBuffer.Write((void*)&camera.GetProjection(), 64, 64);
		mSceneRendererData->mCameraBuffer.Write((void*)&DirectX::XMMatrixInverse(nullptr, viewMatrix), 64, 128);
		mSceneRendererData->mCameraBuffer.Write((void*)&cameraPos.x, 16, 192);
		mSceneRendererData->mCameraCBuffer->Map(mSceneRendererData->mCameraBuffer);

		// Updating the lightning data in the buffer and mapping it to the GPU
		mSceneRendererData->mLightningBuffer.Write((void*)&scene->mLightEnvironment.DirectionalLights[0].Direction, 16, 0);
		mSceneRendererData->mLightningBuffer.Write((void*)&scene->mLightEnvironment.DirectionalLights[0].Radiance, 16, 16);
		mSceneRendererData->mLightningBuffer.Write((void*)&scene->mLightEnvironment.DirectionalLights[0].Multiplier, 4, 32);
		mSceneRendererData->mLightningBuffer.Write((void*)&scene->mLightEnvironment.DirectionalLights[0].SunDisc, 4, 36);
		mSceneRendererData->mLightningCBuffer->Map(mSceneRendererData->mLightningBuffer);

		mSceneRendererData->SceneData.SceneEnvironment = scene->mEnvironment;
		mSceneRendererData->SceneData.SceneEnvironmentIntensity = scene->mEnvironmentIntensity;

		if(mSceneRendererData->SceneData.SceneEnvironment.IrradianceMap)
			mSceneRendererData->SceneData.SceneEnvironment.IrradianceMap->Bind(0, D3D11_PIXEL_SHADER);
		if (mSceneRendererData->SceneData.SceneEnvironment.RadianceMap)
			mSceneRendererData->SceneData.SceneEnvironment.RadianceMap->Bind(1, D3D11_PIXEL_SHADER);
		if (mSceneRendererData->SceneData.SceneEnvironment.SpecularBRDFLUT)
			mSceneRendererData->SceneData.SceneEnvironment.SpecularBRDFLUT->Bind(2, D3D11_PIXEL_SHADER);

		TextureLibrary::GetSampler("Default")->Bind(0, D3D11_PIXEL_SHADER);
		if (TextureLibrary::ExistsSampler("BRDFSampler"))
			TextureLibrary::GetSampler("BRDFSampler")->Bind(1, D3D11_PIXEL_SHADER);
	}

	void Renderer::EndScene()
	{

	}

	void Renderer::Submit(const Ref<IndexBuffer>& indexBuffer, const Ref<Shader> shader, const Ref<ShaderLayout> bufferLayout, const Ref<VertexBuffer> vertexBuffer, const DirectX::XMMATRIX& transform)
	{
		bufferLayout->Bind();
		vertexBuffer->Bind();
		indexBuffer->Bind();
		shader->Bind();

		//RenderCommand::DrawIndexed(indexBuffer);
	}

	//Todo should be integrated into SubmitMesh later on
	void Renderer::SubmitSkybox(const Ref<Mesh> skybox, const DirectX::XMFLOAT4& cameraPos, const DirectX::XMMATRIX& viewMatrix, const DirectX::XMMATRIX& projectionMatrix, float intensity, float LOD)
	{
		if (skybox->mVertexBuffer) skybox->mVertexBuffer->Bind();
		if (skybox->mIndexBuffer) skybox->mIndexBuffer->Bind();

		skybox->Bind();

		mSceneRendererData->mEnvironmentBuffer.Write((void*)&intensity, 4, 0);
		mSceneRendererData->mEnvironmentBuffer.Write((void*)&LOD, 4, 4);
		mSceneRendererData->mEnvironmentCBuffer->Map(mSceneRendererData->mEnvironmentBuffer);

		RenderCommand::DisableWireframeRendering();
		RenderCommand::SetPrimitiveTopology(skybox->mTopology);

		for (Submesh& submesh : skybox->mSubmeshes)
			RenderCommand::DrawIndexed(submesh.BaseVertex, submesh.BaseIndex, submesh.IndexCount);
	}

	void Renderer::SubmitMesh(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, const int entityID, bool wireframe)
	{
		if (mesh->mVertexBuffer) mesh->mVertexBuffer->Bind();
		if (mesh->mIndexBuffer)	mesh->mIndexBuffer->Bind();

		if (wireframe)
			RenderCommand::EnableWireframeRendering();
		else
			RenderCommand::DisableWireframeRendering();

		RenderCommand::SetPrimitiveTopology(mesh->mTopology);

		for (Submesh& submesh : mesh->mSubmeshes) 
		{
			mesh->Set<DirectX::XMMATRIX>("Model", "worldMatrix", DirectX::XMMatrixMultiply(submesh.Transform, transform));
			mesh->Set<int>("Model", "entityID", entityID);
			mesh->Map();
			mesh->Bind();
			
			RenderCommand::DrawIndexed(submesh.BaseVertex, submesh.BaseIndex, submesh.IndexCount);
		}
	}

	void Renderer::SubmitPlanet(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, const int entityID, PlanetComponent::PlanetGPUData planetData, bool wireframe)
	{
		if(mesh->mVertexBuffer)	mesh->mVertexBuffer->Bind();
		if (mesh->mInstanceVertexBuffer) mesh->mInstanceVertexBuffer->Bind();
		if (mesh->mIndexBuffer)	mesh->mIndexBuffer->Bind();

		if (wireframe)
			RenderCommand::EnableWireframeRendering();
		else
			RenderCommand::DisableWireframeRendering();

		// Planet mesh data
		mesh->Set<DirectX::XMFLOAT4>("Planet", "radius", planetData.radius);
		mesh->Set<DirectX::XMFLOAT4>("Planet", "minAltitude", planetData.minAltitude);
		mesh->Set<DirectX::XMFLOAT4>("Planet", "maxAltitude", planetData.maxAltitude);
		mesh->Set<DirectX::XMFLOAT4>("PlanetPS", "radius", planetData.radius);
		mesh->Set<DirectX::XMFLOAT4>("PlanetPS", "minAltitude", planetData.minAltitude);
		mesh->Set<DirectX::XMFLOAT4>("PlanetPS", "maxAltitude", planetData.maxAltitude);

		mesh->Set<DirectX::XMMATRIX>("Model", "worldMatrix", transform);
		mesh->Set<int>("Model", "entityID", entityID);
		mesh->Map();
		mesh->Bind();

		RenderCommand::SetPrimitiveTopology(mesh->mTopology);

		for (Submesh& submesh : mesh->mSubmeshes)
		{
			if (mesh->mPlanetPatches.size() > 495000)
				TOAST_CORE_WARN("Number of instances getting to high: %d", mesh->mPlanetPatches.size());

			RenderCommand::DrawIndexedInstanced(submesh.IndexCount, mesh->mPlanetPatches.size(), 0, 0, 0);
		}
	}

	static Ref<Shader> equirectangularConversionShader, envFilteringShader, envIrradianceShader, spBRDFShader;

	std::pair<Ref<TextureCube>, Ref<TextureCube>> Renderer::CreateEnvironmentMap(const std::string& filepath)
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		const uint32_t cubemapSize = 2048;
		const uint32_t irradianceMapSize = 32;

		Ref<ConstantBuffer> specularMapFilterSettingsCB = CreateRef<ConstantBuffer>("SpecularMapFilterSettings", 16, D3D11_COMPUTE_SHADER, 0, D3D11_USAGE_DEFAULT);

		Ref<Texture2D> starMap = CreateRef<Texture2D>(filepath);
		Ref<TextureSampler> defaultSampler = TextureLibrary::GetSampler("Default");
		Ref<TextureCube> envMapUnfiltered = CreateRef<TextureCube>("EnvMapUnfiltered", cubemapSize, cubemapSize);
		Ref<TextureCube> envMapFiltered = CreateRef<TextureCube>("EnvMapFiltered", cubemapSize, cubemapSize);

		envMapUnfiltered->CreateUAV(0);

		if (!equirectangularConversionShader)
			equirectangularConversionShader = CreateRef<Shader>("assets/shaders/EquirectangularToCubeMap.hlsl");

		equirectangularConversionShader->Bind();
		starMap->Bind();
		defaultSampler->Bind(0, D3D11_COMPUTE_SHADER);
		envMapUnfiltered->BindForReadWrite(0, D3D11_COMPUTE_SHADER);
		RenderCommand::DispatchCompute(cubemapSize / 32, cubemapSize / 32, 6);
		envMapUnfiltered->UnbindUAV();

		envMapUnfiltered->GenerateMips();

		for (int arraySlice = 0; arraySlice < 6; ++arraySlice) {
			const uint32_t subresourceIndex = D3D11CalcSubresource(0, arraySlice, envMapFiltered->GetMipLevelCount());
			deviceContext->CopySubresourceRegion(envMapFiltered->GetResource(), subresourceIndex, 0, 0, 0, envMapUnfiltered->GetResource(), subresourceIndex, nullptr);
		}

		struct SpecularMapFilterSettingsCB
		{
			float roughness;
			float padding[3];
		};

		if (!envFilteringShader)
			envFilteringShader = CreateRef<Shader>("assets/shaders/EnvironmentMipFilter.hlsl");

		envFilteringShader->Bind();
		envMapUnfiltered->Bind(0, D3D11_COMPUTE_SHADER);
		defaultSampler->Bind(0, D3D11_COMPUTE_SHADER);

		// Pre-filter rest of the mip chain.
		const float deltaRoughness = 1.0f / std::max(float(envMapFiltered->GetMipLevelCount() - 1.0f), 1.0f);
		for (int level = 1, size = cubemapSize / 2; level < envMapFiltered->GetMipLevelCount(); ++level, size /= 2) {
			const int numGroups = std::max(1, size / 32);

			envMapFiltered->CreateUAV(level);
			
			const SpecularMapFilterSettingsCB spmapConstants = { level * deltaRoughness };
			deviceContext->UpdateSubresource(specularMapFilterSettingsCB->GetBuffer(), 0, nullptr, &spmapConstants, 0, 0);

			specularMapFilterSettingsCB->Bind();
			envMapFiltered->BindForReadWrite(0, D3D11_COMPUTE_SHADER);
			RenderCommand::DispatchCompute(numGroups, numGroups, 6);
		}
		envMapFiltered->UnbindUAV();

		Ref<TextureCube> irradianceMap = CreateRef<TextureCube>("IrradianceMap", irradianceMapSize, irradianceMapSize, 1);

		if (!envIrradianceShader)
			envIrradianceShader = CreateRef<Shader>("assets/shaders/EnvironmentIrradiance.hlsl");

		irradianceMap->CreateUAV(0);

		envMapFiltered->Bind(0, D3D11_COMPUTE_SHADER);
		irradianceMap->BindForReadWrite(0, D3D11_COMPUTE_SHADER);
		defaultSampler->Bind(0, D3D11_COMPUTE_SHADER);
		envIrradianceShader->Bind();
		RenderCommand::DispatchCompute(irradianceMap->GetWidth() / 32, irradianceMap->GetHeight() / 32, 6);
		irradianceMap->UnbindUAV();

		return { envMapFiltered, irradianceMap };
	}

	void Renderer::ResetStats()
	{
		memset(&sData.Stats, 0, sizeof(Statistics));
	}

	Renderer::Statistics Renderer::GetStats()
	{
		return sData.Stats;
	}
}