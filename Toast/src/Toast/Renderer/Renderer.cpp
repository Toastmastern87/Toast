#include "tpch.h"
#include "Renderer.h"

#include "Toast/Renderer/Renderer2D.h"
#include "Toast/Renderer/RendererDebug.h"

namespace Toast {

	struct RendererStat
	{
		Renderer::Statistics Stats;
	};

	static RendererStat sData;

	Scope<Renderer::RendererData> Renderer::sRendererData = CreateScope<Renderer::RendererData>();

	void Renderer::Init()
	{
		TOAST_PROFILE_FUNCTION();

		RenderCommand::Init();
		Renderer2D::Init();
		RendererDebug::Init();

		// Setting up the constant buffer and data buffer for the camera rendering
		sRendererData->CameraCBuffer = ConstantBufferLibrary::Load("Camera", 272, D3D11_VERTEX_SHADER, 0);
		sRendererData->CameraCBuffer->Bind();
		sRendererData->CameraBuffer.Allocate(sRendererData->CameraCBuffer->GetSize());
		sRendererData->CameraBuffer.ZeroInitialize();

		//// Setting up the constant buffer and data buffer for lightning rendering
		sRendererData->LightningCBuffer = ConstantBufferLibrary::Load("DirectionalLight", 48, D3D11_PIXEL_SHADER, 0);
		sRendererData->LightningCBuffer->Bind();
		sRendererData->LightningBuffer.Allocate(sRendererData->LightningCBuffer->GetSize());
		sRendererData->LightningBuffer.ZeroInitialize();

		// Setting up the constant buffer and data buffer for enviromental rendering
		sRendererData->EnvironmentCBuffer = ConstantBufferLibrary::Load("Environment", 16, D3D11_PIXEL_SHADER, 3);
		sRendererData->EnvironmentCBuffer->Bind();
		sRendererData->EnvironmentBuffer.Allocate(sRendererData->EnvironmentCBuffer->GetSize());
		sRendererData->EnvironmentBuffer.ZeroInitialize();

		FramebufferSpecification baseFBSpec, pickingFBSpec;
		baseFBSpec.Attachments = { FramebufferTextureFormat::R32G32B32A32_FLOAT, FramebufferTextureFormat::Depth };
		baseFBSpec.Width = 1280;
		baseFBSpec.Height = 720;
		baseFBSpec.Samples = 4;
		sRendererData->BaseFramebuffer = CreateRef<Framebuffer>(baseFBSpec);
		pickingFBSpec.Attachments = { FramebufferTextureFormat::R32_SINT, FramebufferTextureFormat::Depth };
		pickingFBSpec.Width = 1280;
		pickingFBSpec.Height = 720;
		sRendererData->PickingFramebuffer = CreateRef<Framebuffer>(pickingFBSpec);
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
		sRendererData->CameraBuffer.Write((void*)&viewMatrix, 64, 0);
		sRendererData->CameraBuffer.Write((void*)&camera.GetProjection(), 64, 64);
		sRendererData->CameraBuffer.Write((void*)&DirectX::XMMatrixInverse(nullptr, viewMatrix), 64, 128);
		sRendererData->CameraBuffer.Write((void*)&DirectX::XMMatrixInverse(nullptr, camera.GetProjection()), 64, 192);
		sRendererData->CameraBuffer.Write((void*)&cameraPos.x, 16, 256);
		sRendererData->CameraCBuffer->Map(sRendererData->CameraBuffer);

		// Updating the lightning data in the buffer and mapping it to the GPU
		sRendererData->LightningBuffer.Write((void*)&scene->mLightEnvironment.DirectionalLights[0].Direction, 16, 0);
		sRendererData->LightningBuffer.Write((void*)&scene->mLightEnvironment.DirectionalLights[0].Radiance, 16, 16);
		sRendererData->LightningBuffer.Write((void*)&scene->mLightEnvironment.DirectionalLights[0].Multiplier, 4, 32);
		sRendererData->LightningBuffer.Write((void*)&scene->mLightEnvironment.DirectionalLights[0].SunDisc, 4, 36);
		sRendererData->LightningCBuffer->Map(sRendererData->LightningBuffer);

		sRendererData->SceneData.SceneEnvironment = scene->mEnvironment;
		sRendererData->SceneData.SceneEnvironmentIntensity = scene->mEnvironmentIntensity;

		if(sRendererData->SceneData.SceneEnvironment.IrradianceMap)
			sRendererData->SceneData.SceneEnvironment.IrradianceMap->Bind(0, D3D11_PIXEL_SHADER);
		if (sRendererData->SceneData.SceneEnvironment.RadianceMap)
			sRendererData->SceneData.SceneEnvironment.RadianceMap->Bind(1, D3D11_PIXEL_SHADER);
		if (sRendererData->SceneData.SceneEnvironment.SpecularBRDFLUT)
			sRendererData->SceneData.SceneEnvironment.SpecularBRDFLUT->Bind(2, D3D11_PIXEL_SHADER);

		TextureLibrary::GetSampler("Default")->Bind(0, D3D11_PIXEL_SHADER);
		if (TextureLibrary::ExistsSampler("BRDFSampler"))
			TextureLibrary::GetSampler("BRDFSampler")->Bind(1, D3D11_PIXEL_SHADER);
	}

	void Renderer::EndScene(const bool debugActivated)
	{
		BaseRenderPass();
		PickingRenderPass();

		if (!debugActivated) 
		{
			RenderCommand::BindBackbuffer();
			RenderCommand::Clear({ 0.24f, 0.24f, 0.24f, 1.0f });
		}

		ClearDrawList();
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
		sRendererData->CameraPos = cameraPos;
		sRendererData->ViewMatrix = viewMatrix;
		sRendererData->ProjectionMatrix = projectionMatrix;
		sRendererData->SceneData.SkyboxData.Skybox = skybox;
		sRendererData->SceneData.SkyboxData.Intensity = intensity;
		sRendererData->SceneData.SkyboxData.LOD = LOD;
	}

	void Renderer::SubmitMesh(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, const int entityID, bool wireframe)
	{
		sRendererData->MeshDrawList.emplace_back(mesh, transform, wireframe, entityID);
	}

	void Renderer::SubmitPlanet(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, const int entityID, PlanetComponent::PlanetGPUData planetData, bool wireframe)
	{
		sRendererData->MeshDrawList.emplace_back(mesh, transform, wireframe, entityID, &planetData);
	}

	void Renderer::ClearDrawList()
	{
		sRendererData->MeshDrawList.clear();
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

	void Renderer::BaseRenderPass()
	{
		RenderCommand::EnableBlending();

		sRendererData->BaseFramebuffer->Bind();
		sRendererData->BaseFramebuffer->Clear({ 0.24f, 0.24f, 0.24f, 1.0f });

		if (sRendererData->SceneData.SkyboxData.Skybox)
		{
			if (sRendererData->SceneData.SkyboxData.Skybox->mVertexBuffer) sRendererData->SceneData.SkyboxData.Skybox->mVertexBuffer->Bind();
			if (sRendererData->SceneData.SkyboxData.Skybox->mIndexBuffer) sRendererData->SceneData.SkyboxData.Skybox->mIndexBuffer->Bind();

			sRendererData->SceneData.SkyboxData.Skybox->Bind();

			sRendererData->EnvironmentBuffer.Write((void*)&sRendererData->SceneData.SkyboxData.Intensity, 4, 0);
			sRendererData->EnvironmentBuffer.Write((void*)&sRendererData->SceneData.SkyboxData.LOD, 4, 4);
			sRendererData->EnvironmentCBuffer->Map(sRendererData->EnvironmentBuffer);

			RenderCommand::DisableWireframe();
			RenderCommand::SetPrimitiveTopology(sRendererData->SceneData.SkyboxData.Skybox->mTopology);

			for (Submesh& submesh : sRendererData->SceneData.SkyboxData.Skybox->mSubmeshes)
				RenderCommand::DrawIndexed(submesh.BaseVertex, submesh.BaseIndex, submesh.IndexCount);
		}

		for (const auto& meshCommand : sRendererData->MeshDrawList) {
			if (meshCommand.Mesh->mVertexBuffer)	meshCommand.Mesh->mVertexBuffer->Bind();
			if (meshCommand.Mesh->mInstanceVertexBuffer && meshCommand.PlanetData) meshCommand.Mesh->mInstanceVertexBuffer->Bind();
			if (meshCommand.Mesh->mIndexBuffer)		meshCommand.Mesh->mIndexBuffer->Bind();

			if (meshCommand.Wireframe)
				RenderCommand::EnableWireframe();
			else
				RenderCommand::DisableWireframe();

			RenderCommand::SetPrimitiveTopology(meshCommand.Mesh->mTopology);

			for (Submesh& submesh : meshCommand.Mesh->mSubmeshes)
			{
				meshCommand.Mesh->Set<DirectX::XMMATRIX>("Model", "worldMatrix", DirectX::XMMatrixMultiply(submesh.Transform, meshCommand.Transform));
				meshCommand.Mesh->Set<int>("Model", "entityID", meshCommand.EntityID);
				meshCommand.Mesh->Map();
				meshCommand.Mesh->Bind();

				if (meshCommand.PlanetData)
				{
					// Planet mesh data
					meshCommand.Mesh->Set<DirectX::XMFLOAT4>("Planet", "radius", meshCommand.PlanetData->radius);
					meshCommand.Mesh->Set<DirectX::XMFLOAT4>("Planet", "minAltitude", meshCommand.PlanetData->minAltitude);
					meshCommand.Mesh->Set<DirectX::XMFLOAT4>("Planet", "maxAltitude", meshCommand.PlanetData->maxAltitude);
					meshCommand.Mesh->Set<DirectX::XMFLOAT4>("PlanetPS", "radius", meshCommand.PlanetData->radius);
					meshCommand.Mesh->Set<DirectX::XMFLOAT4>("PlanetPS", "minAltitude", meshCommand.PlanetData->minAltitude);
					meshCommand.Mesh->Set<DirectX::XMFLOAT4>("PlanetPS", "maxAltitude", meshCommand.PlanetData->maxAltitude);
				}

				if (meshCommand.PlanetData)
					RenderCommand::DrawIndexedInstanced(submesh.IndexCount, static_cast<uint32_t>(meshCommand.Mesh->mPlanetPatches.size()), 0, 0, 0);
				else
					RenderCommand::DrawIndexed(submesh.BaseVertex, submesh.BaseIndex, submesh.IndexCount);
			}
		}
	}

	void Renderer::PickingRenderPass()
	{
		RenderCommand::DisableBlending();

		sRendererData->PickingFramebuffer->Bind();
		sRendererData->PickingFramebuffer->Clear({ 0.24f, 0.24f, 0.24f, 1.0f });

		for (const auto& meshCommand : sRendererData->MeshDrawList) {
			if (meshCommand.Mesh->mVertexBuffer)	meshCommand.Mesh->mVertexBuffer->Bind();
			if (meshCommand.Mesh->mIndexBuffer)		meshCommand.Mesh->mIndexBuffer->Bind();

			RenderCommand::SetPrimitiveTopology(meshCommand.Mesh->mTopology);

			for (Submesh& submesh : meshCommand.Mesh->mSubmeshes)
			{
				meshCommand.Mesh->Set<DirectX::XMMATRIX>("Model", "worldMatrix", DirectX::XMMatrixMultiply(submesh.Transform, meshCommand.Transform));
				meshCommand.Mesh->Set<int>("Model", "entityID", meshCommand.EntityID);
				meshCommand.Mesh->Map();
				meshCommand.Mesh->Bind();

				auto& pickingShader = ShaderLibrary::Get("Picking");
				pickingShader->Bind();

				RenderCommand::DrawIndexed(submesh.BaseVertex, submesh.BaseIndex, submesh.IndexCount);
			}
		}
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