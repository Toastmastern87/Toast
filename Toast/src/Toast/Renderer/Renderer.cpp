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

		mSceneRendererData->viewMatrix = viewMatrix;
		mSceneRendererData->inverseViewMatrix = DirectX::XMMatrixInverse(nullptr, viewMatrix);
		mSceneRendererData->projectionMatrix = camera.GetProjection();
		mSceneRendererData->cameraPos = cameraPos;
		MaterialLibrary::Get("Standard")->SetData("Camera", (void*)&mSceneRendererData->viewMatrix);

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

		mSceneRendererData->SceneData.SceneLightEnvironment = scene->mLightEnvironment;

		MaterialLibrary::Get("Standard")->SetData("DirectionalLight", (void*)&mSceneRendererData->SceneData.SceneLightEnvironment);
	}

	void Renderer::EndScene()
	{

	}

	void Renderer::Submit(const Ref<IndexBuffer>& indexBuffer, const Ref<Shader> shader, const Ref<BufferLayout> bufferLayout, const Ref<VertexBuffer> vertexBuffer, const DirectX::XMMATRIX& transform)
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

		struct EnvironmentCB
		{
			float intensity;
			float LOD;
			float padding[2];
		};

		struct SkyboxCB
		{
			DirectX::XMMATRIX viewMatrix;
			DirectX::XMMATRIX projectionMatrix;
			DirectX::XMFLOAT4 cameraPos;
		};

		const SkyboxCB skyboxTransforms = { viewMatrix,  projectionMatrix, cameraPos };
		const EnvironmentCB environmentConstants = { intensity, LOD, 0.0f, 0.0f };

		skybox->mMaterial->SetData("SkyboxTransforms", (void*)&skyboxTransforms);
		skybox->mMaterial->SetData("Environment", (void*)&environmentConstants);
		skybox->mMaterial->Bind();

		RenderCommand::DisableWireframeRendering();
		RenderCommand::SetPrimitiveTopology(skybox->mTopology);

		for (Submesh& submesh : skybox->mSubmeshes)
			RenderCommand::DrawIndexed(submesh.BaseVertex, submesh.BaseIndex, submesh.IndexCount);
	}

	void Renderer::SubmitMesh(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, const int entityID, bool wireframe)
	{
		struct ModelCB
		{
			DirectX::XMMATRIX Transform;
			int EntityID;
			int padding[3];
		};

		if (mesh->mVertexBuffer) mesh->mVertexBuffer->Bind();
		if (mesh->mIndexBuffer)	mesh->mIndexBuffer->Bind();

		if (wireframe)
			RenderCommand::EnableWireframeRendering();
		else
			RenderCommand::DisableWireframeRendering();

		RenderCommand::SetPrimitiveTopology(mesh->mTopology);

		for (Submesh& submesh : mesh->mSubmeshes) 
		{
			ModelCB modelCB = { DirectX::XMMatrixMultiply(transform, submesh.Transform), entityID, { 0, 0, 0 } };
			mesh->mMaterial->SetData("Model", (void*)&modelCB);
			mesh->mMaterial->Bind();
			
			RenderCommand::DrawIndexed(submesh.BaseVertex, submesh.BaseIndex, submesh.IndexCount);
		}
	}

	void Renderer::SubmitPlanet(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, const int entityID, PlanetComponent::PlanetGPUData planetData, bool wireframe)
	{
		struct ModelCB
		{
			DirectX::XMMATRIX Transform;
			int EntityID;
			int padding[3];
		};

		ModelCB modelCB = { transform, entityID, { 0, 0, 0 } };

		if(mesh->mVertexBuffer)	mesh->mVertexBuffer->Bind();
		if (mesh->mInstanceVertexBuffer) mesh->mInstanceVertexBuffer->Bind();
		if (mesh->mIndexBuffer)	mesh->mIndexBuffer->Bind();

		if (wireframe)
			RenderCommand::EnableWireframeRendering();
		else
			RenderCommand::DisableWireframeRendering();

		mesh->mMaterial->SetData("Planet", (void*)&planetData);
		mesh->mMaterial->SetData("PlanetPS", (void*)&planetData);
		mesh->mMaterial->SetData("Model", (void*)&modelCB);
		mesh->mMaterial->Bind();

		RenderCommand::SetPrimitiveTopology(mesh->mTopology);

		for (Submesh& submesh : mesh->mSubmeshes)
		{
			if (mesh->mPlanetPatches.size() > 495000)
				TOAST_CORE_WARN("Number of instances getting to high: {0}", mesh->mPlanetPatches.size());

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