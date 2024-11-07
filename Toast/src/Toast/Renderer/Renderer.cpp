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
		sRendererData->CameraCBuffer = ConstantBufferLibrary::Load("Camera", 288, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_VERTEX_SHADER, CBufferBindSlot::Camera), CBufferBindInfo(D3D11_PIXEL_SHADER, CBufferBindSlot::Camera) });
		sRendererData->CameraCBuffer->Bind();
		sRendererData->CameraBuffer.Allocate(sRendererData->CameraCBuffer->GetSize());
		sRendererData->CameraBuffer.ZeroInitialize();

		// Setting up the constant buffer and data buffer for the Model
		sRendererData->ModelCBuffer = ConstantBufferLibrary::Load("Model", 80, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_VERTEX_SHADER, CBufferBindSlot::Model) });
		sRendererData->ModelCBuffer->Bind();
		sRendererData->ModelBuffer.Allocate(sRendererData->ModelCBuffer->GetSize());
		sRendererData->ModelBuffer.ZeroInitialize();

		// Setting up the constant buffer and data buffer for the PBR Material
		sRendererData->MaterialCBuffer = ConstantBufferLibrary::Load("Material", 48, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_VERTEX_SHADER, CBufferBindSlot::Material) });
		sRendererData->MaterialCBuffer->Bind();
		sRendererData->MaterialBuffer.Allocate(sRendererData->MaterialCBuffer->GetSize());
		sRendererData->MaterialBuffer.ZeroInitialize();

		// Setting up the constant buffer and data buffer for lightning rendering
		sRendererData->LightningCBuffer = ConstantBufferLibrary::Load("DirectionalLight", 48, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_PIXEL_SHADER, CBufferBindSlot::DirectionalLight) });
		sRendererData->LightningCBuffer->Bind();
		sRendererData->LightningBuffer.Allocate(sRendererData->LightningCBuffer->GetSize());
		sRendererData->LightningBuffer.ZeroInitialize();

		// Setting up the constant buffer and data buffer for environmental rendering
		sRendererData->EnvironmentCBuffer = ConstantBufferLibrary::Load("Environment", 16, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_PIXEL_SHADER, CBufferBindSlot::Environment) });
		sRendererData->EnvironmentCBuffer->Bind();
		sRendererData->EnvironmentBuffer.Allocate(sRendererData->EnvironmentCBuffer->GetSize());
		sRendererData->EnvironmentBuffer.ZeroInitialize();

		// Setting up the constant buffer and data buffer for the render settings
		sRendererData->RenderSettingsCBuffer = ConstantBufferLibrary::Load("RenderSettings", 16, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_PIXEL_SHADER, CBufferBindSlot::RenderSettings) });
		sRendererData->RenderSettingsCBuffer->Bind();
		sRendererData->RenderSettingsBuffer.Allocate(sRendererData->RenderSettingsCBuffer->GetSize());
		sRendererData->RenderSettingsBuffer.ZeroInitialize();

		// Setting up the constant buffer for atmosphere rendering
		sRendererData->AtmosphereCBuffer = ConstantBufferLibrary::Load("Atmosphere", 80, std::vector<CBufferBindInfo>{  CBufferBindInfo(D3D11_PIXEL_SHADER, CBufferBindSlot::Atmosphere) });
		sRendererData->AtmosphereCBuffer->Bind();
		sRendererData->AtmosphereBuffer.Allocate(sRendererData->AtmosphereCBuffer->GetSize());
		sRendererData->AtmosphereBuffer.ZeroInitialize();

		sRendererData->BaseRenderTarget = CreateRef<RenderTarget>(RenderTargetType::Color, 1280, 720, 1, TextureFormat::R16G16B16A16_FLOAT);
		sRendererData->PostProcessRenderTarget = CreateRef<RenderTarget>(RenderTargetType::Color, 1280, 720, 1, TextureFormat::R16G16B16A16_FLOAT);
		sRendererData->FinalRenderTarget = CreateRef<RenderTarget>(RenderTargetType::Color, 1280, 720, 1, TextureFormat::R16G16B16A16_FLOAT);
		sRendererData->OutlineRenderTarget = CreateRef<RenderTarget>(RenderTargetType::Color, 1280, 720, 1, TextureFormat::R8G8B8A8_UNORM);

		// Setting up the render targets for the Geometry Pass
		sRendererData->GPassPositionRT = CreateRef<RenderTarget>(RenderTargetType::Color, 1280, 720, 1, TextureFormat::R32G32B32A32_FLOAT);
		sRendererData->GPassNormalRT = CreateRef<RenderTarget>(RenderTargetType::Color, 1280, 720, 1, TextureFormat::R16G16B16A16_FLOAT);
		sRendererData->GPassAlbedoMetallicRT = CreateRef<RenderTarget>(RenderTargetType::Color, 1280, 720, 1, TextureFormat::R8G8B8A8_UNORM);
		sRendererData->GPassRoughnessAORT = CreateRef<RenderTarget>(RenderTargetType::Color, 1280, 720, 1, TextureFormat::R8G8B8A8_UNORM);
		sRendererData->GPassPickingRT = CreateRef<RenderTarget>(RenderTargetType::Color, 1280, 720, 1, TextureFormat::R32_SINT);
		sRendererData->GPassDepthRT = CreateRef<RenderTarget>(RenderTargetType::Depth, 1280, 720, 1, TextureFormat::D32_FLOAT);

		// Setting up the framebuffer for the Geometry Pass
		sRendererData->GPassFramebuffer = CreateRef<Framebuffer>(std::vector<Ref<RenderTarget>>{ sRendererData->GPassPositionRT, sRendererData->GPassNormalRT, sRendererData->GPassAlbedoMetallicRT, sRendererData->GPassRoughnessAORT, sRendererData->GPassPickingRT }, sRendererData->GPassDepthRT);

		sRendererData->BaseFramebuffer = CreateRef<Framebuffer>(std::vector<Ref<RenderTarget>>{ sRendererData->BaseRenderTarget}, sRendererData->GPassDepthRT);
		sRendererData->PostProcessFramebuffer = CreateRef<Framebuffer>(std::vector<Ref<RenderTarget>>{ sRendererData->PostProcessRenderTarget}, sRendererData->GPassDepthRT);
		sRendererData->FinalFramebuffer = CreateRef<Framebuffer>(
			std::vector<Ref<RenderTarget>>{ sRendererData->FinalRenderTarget, sRendererData->GPassPickingRT },
			sRendererData->GPassDepthRT
		);
		sRendererData->PickingFramebuffer = CreateRef<Framebuffer>(std::vector<Ref<RenderTarget>>{ sRendererData->GPassPickingRT});
		sRendererData->OutlineFramebuffer = CreateRef<Framebuffer>(std::vector<Ref<RenderTarget>>{ sRendererData->OutlineRenderTarget});
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

	void Renderer::BeginScene(const Scene* scene, Camera& camera, const DirectX::XMFLOAT4 cameraPos)
	{
		TOAST_PROFILE_FUNCTION();

		// Updating the camera data in the buffer and mapping it to the GPU
		sRendererData->CameraBuffer.Write((uint8_t*)&camera.GetViewMatrix(), 64, 0);
		sRendererData->CameraBuffer.Write((uint8_t*)&camera.GetProjection(), 64, 64);
		sRendererData->CameraBuffer.Write((uint8_t*)&camera.GetInvViewMatrix(), 64, 128);
		sRendererData->CameraBuffer.Write((uint8_t*)&camera.GetInvProjection(), 64, 192);
		sRendererData->CameraBuffer.Write((uint8_t*)&cameraPos.x, 16, 256);
		sRendererData->CameraBuffer.Write((uint8_t*)&camera.GetFarClip(), 4, 272);
		sRendererData->CameraBuffer.Write((uint8_t*)&camera.GetNearClip(), 4, 276);
		sRendererData->CameraCBuffer->Map(sRendererData->CameraBuffer);

		// Updating the lightning data in the buffer and mapping it to the GPU
		sRendererData->LightningBuffer.Write((uint8_t*)&scene->mLightEnvironment.DirectionalLights[0].Direction, 16, 0);
		sRendererData->LightningBuffer.Write((uint8_t*)&scene->mLightEnvironment.DirectionalLights[0].Radiance, 16, 16);
		sRendererData->LightningBuffer.Write((uint8_t*)&scene->mLightEnvironment.DirectionalLights[0].Multiplier, 4, 32);
		sRendererData->LightningBuffer.Write((uint8_t*)&scene->mLightEnvironment.DirectionalLights[0].SunDisc, 4, 36);
		sRendererData->LightningCBuffer->Map(sRendererData->LightningBuffer);

		sRendererData->SceneData.SceneEnvironment = scene->mEnvironment;
		sRendererData->SceneData.SceneEnvironmentIntensity = scene->mEnvironmentIntensity;

		if (sRendererData->SceneData.SceneEnvironment.IrradianceMap)
			sRendererData->SceneData.SceneEnvironment.IrradianceMap->Bind(0, D3D11_PIXEL_SHADER);

		if (sRendererData->SceneData.SceneEnvironment.RadianceMap)
			sRendererData->SceneData.SceneEnvironment.RadianceMap->Bind(1, D3D11_PIXEL_SHADER);

		if (sRendererData->SceneData.SceneEnvironment.SpecularBRDFLUT)
			sRendererData->SceneData.SceneEnvironment.SpecularBRDFLUT->Bind(2, D3D11_PIXEL_SHADER);

		TextureLibrary::GetSampler("Default")->Bind(0, D3D11_PIXEL_SHADER);
		if (TextureLibrary::ExistsSampler("BRDFSampler"))
			TextureLibrary::GetSampler("BRDFSampler")->Bind(1, D3D11_PIXEL_SHADER);

		// Updating the render settings data in the buffer and mapping it to the GPU
		float renderOverlay = (float)scene->mSettings.PlanetOverlaySetting;
		sRendererData->RenderSettingsBuffer.Write((uint8_t*)&renderOverlay, 4, 0);
		sRendererData->RenderSettingsCBuffer->Map(sRendererData->RenderSettingsBuffer);
	}

	void Renderer::EndScene(const bool debugActivated)
	{
		// Deffered Renderer
		GeometryPass();
		LightningPass();

		//BaseRenderPass();
		//PickingRenderPass();
		//PostProcessPass();

		if (!debugActivated) 
		{
			RenderCommand::BindBackbuffer();
			RenderCommand::Clear({ 0.24f, 0.24f, 0.24f, 1.0f });
		}

		ClearDrawList();

		//TOAST_CORE_CRITICAL("END OF SCENE!");
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
	void Renderer::SubmitSkybox(const Ref<Mesh> skybox, const DirectX::XMFLOAT4& cameraPos, const DirectX::XMFLOAT4X4& viewMatrix, const DirectX::XMFLOAT4X4& projectionMatrix, float intensity, float LOD)
	{
		sRendererData->CameraPos = cameraPos;
		sRendererData->ViewMatrix = viewMatrix;
		sRendererData->ProjectionMatrix = projectionMatrix;
		sRendererData->SceneData.SkyboxData.Skybox = skybox;
		sRendererData->SceneData.SkyboxData.Intensity = intensity;
		sRendererData->SceneData.SkyboxData.LOD = LOD;
	}

	void Renderer::SubmitMesh(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, const int entityID, bool wireframe, PlanetComponent::GPUData* planetData, bool atmosphere)
	{
		sRendererData->PlanetData.Atmosphere = atmosphere;
;		sRendererData->MeshDrawList.emplace_back(mesh, transform, wireframe, entityID, planetData);
	}

	void Renderer::SubmitSelecetedMesh(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, bool wireframe)
	{
		sRendererData->MeshSelectedDrawList.emplace_back(mesh, transform, wireframe);
	}

	void Renderer::DrawFullscreenQuad()
	{
		RenderCommand::Draw(3);
	}

	void Renderer::ClearDrawList()
	{
		sRendererData->MeshDrawList.clear();
		sRendererData->MeshColliderDrawList.clear();
	}

	static Scope<Shader> equirectangularConversionShader, envFilteringShader, envIrradianceShader;

	std::pair<Ref<TextureCube>, Ref<TextureCube>> Renderer::CreateEnvironmentMap(const std::string& filepath)
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		const uint32_t cubemapSize = 2048;
		const uint32_t irradianceMapSize = 32;

		Ref<ConstantBuffer> specularMapFilterSettingsCB = CreateRef<ConstantBuffer>("SpecularMapFilterSettings", 16, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_COMPUTE_SHADER, CBufferBindSlot::SpecularLightEnvironmental) }, D3D11_USAGE_DEFAULT);

		Ref<Texture2D> starMap = CreateRef<Texture2D>(filepath);
		TextureSampler* defaultSampler = TextureLibrary::GetSampler("Default");
		Ref<TextureCube> envMapUnfiltered = CreateRef<TextureCube>("EnvMapUnfiltered", cubemapSize, cubemapSize);
		Ref<TextureCube> envMapFiltered = CreateRef<TextureCube>("EnvMapFiltered", cubemapSize, cubemapSize);

		envMapUnfiltered->CreateUAV(0);

		if (!equirectangularConversionShader)
			equirectangularConversionShader = CreateScope<Shader>("assets/shaders/EquirectangularToCubeMap.hlsl");

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
			envFilteringShader = CreateScope<Shader>("assets/shaders/EnvironmentMipFilter.hlsl");

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
			envIrradianceShader = CreateScope<Shader>("assets/shaders/EnvironmentIrradiance.hlsl");

		irradianceMap->CreateUAV(0);

		envMapFiltered->Bind(0, D3D11_COMPUTE_SHADER);
		irradianceMap->BindForReadWrite(0, D3D11_COMPUTE_SHADER);
		defaultSampler->Bind(0, D3D11_COMPUTE_SHADER);
		envIrradianceShader->Bind();
		RenderCommand::DispatchCompute(irradianceMap->GetWidth() / 32, irradianceMap->GetHeight() / 32, 6);
		irradianceMap->UnbindUAV();

		return { envMapFiltered, irradianceMap };
	}

	void Renderer::GeometryPass()
	{
		TOAST_PROFILE_FUNCTION();

#ifdef TOAST_DEBUG
		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation)
			annotation->BeginEvent(L"Geometry Pass");
#endif

		// Temp
		sRendererData->FinalFramebuffer->Clear({ 0.0f, 0.0f, 0.0f, 1.0f });

		sRendererData->GPassFramebuffer->Bind();
		sRendererData->GPassFramebuffer->Clear({ 0.0f, 0.0f, 0.0f, 1.0f });

		ShaderLibrary::Get("assets/shaders/Deffered Rendering/GeometryPass.hlsl")->Bind();

		for (const auto& meshCommand : sRendererData->MeshDrawList)
		{
			if (meshCommand.Wireframe)
				RenderCommand::EnableWireframe();
			else
				RenderCommand::DisableWireframe();

			RenderCommand::SetPrimitiveTopology(meshCommand.Mesh->mTopology);

			int planet;

			if (!meshCommand.PlanetData)
			{
				if (meshCommand.Mesh->IsInstanced())
				{
					for (Submesh& submesh : meshCommand.Mesh->mSubmeshes)
					{
						bool environment = sRendererData->SceneData.SceneEnvironment.IrradianceMap && sRendererData->SceneData.SceneEnvironment.RadianceMap;

						planet = 0;

						sRendererData->ModelBuffer.Write((uint8_t*)&DirectX::XMMatrixMultiply(submesh.Transform, meshCommand.Transform), 64, 0);
						sRendererData->ModelBuffer.Write((uint8_t*)&meshCommand.EntityID, 4, 64);
						sRendererData->ModelBuffer.Write((uint8_t*)&planet, 4, 68);
						sRendererData->ModelCBuffer->Map(sRendererData->ModelBuffer);

						//meshCommand.Mesh->Map(submesh.MaterialName);
						meshCommand.Mesh->Bind();

						uint32_t bufferElements = meshCommand.Mesh->mInstanceVertexBuffer->GetBufferSize() / sizeof(DirectX::XMFLOAT3);
						RenderCommand::DrawIndexedInstanced(meshCommand.Mesh->mSubmeshes[0].IndexCount, meshCommand.Mesh->GetNumberOfInstances(), 0, 0, 0);
					}
				}
				else
				{
					for (Submesh& submesh : meshCommand.Mesh->mSubmeshes)
					{
						//TOAST_CORE_INFO("Rendering submesh with material: %s", submesh.MaterialName.c_str());
						bool environment = sRendererData->SceneData.SceneEnvironment.IrradianceMap && sRendererData->SceneData.SceneEnvironment.RadianceMap;

						planet = 0;

						sRendererData->ModelBuffer.Write((uint8_t*)&DirectX::XMMatrixMultiply(submesh.Transform, meshCommand.Transform), 64, 0);
						sRendererData->ModelBuffer.Write((uint8_t*)&meshCommand.EntityID, 4, 64);
						sRendererData->ModelBuffer.Write((uint8_t*)&planet, 4, 68);
						sRendererData->ModelCBuffer->Map(sRendererData->ModelBuffer);

						//meshCommand.Mesh->Bind(submesh.MaterialName, environment, false);
						meshCommand.Mesh->Bind();

						RenderCommand::DrawIndexed(submesh.BaseVertex, submesh.BaseIndex, submesh.IndexCount);
					}
				}
			}
			else
			{
				//meshCommand.Mesh->Map("Planet");
				//meshCommand.Mesh->Bind("Planet", true, false);

				//meshCommand.Mesh->Set<int>(meshCommand.Mesh->mSubmeshes[0].MaterialName, "Model", "entityID", -1);
				//meshCommand.Mesh->Set<int>(meshCommand.Mesh->mSubmeshes[0].MaterialName, "Model", "planet", 1);

				//RenderCommand::DrawIndexed(meshCommand.Mesh->mSubmeshes[0].BaseVertex, meshCommand.Mesh->mSubmeshes[0].BaseIndex, meshCommand.Mesh->mSubmeshes[0].IndexCount);
			}
		}

		sRendererData->GPassFramebuffer->Unbind();

#ifdef TOAST_DEBUG
		if (annotation)
			annotation->EndEvent();
#endif
	}

	void Renderer::LightningPass()
	{
		TOAST_PROFILE_FUNCTION();

#ifdef TOAST_DEBUG
		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation)
			annotation->BeginEvent(L"Lightning Pass");
#endif

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();
		deviceContext->PSSetShaderResources(0, 1, sRendererData->GPassPositionRT->GetSRV().GetAddressOf());
		deviceContext->PSSetShaderResources(1, 1, sRendererData->GPassNormalRT->GetSRV().GetAddressOf());
		deviceContext->PSSetShaderResources(2, 1, sRendererData->GPassAlbedoMetallicRT->GetSRV().GetAddressOf());
		deviceContext->PSSetShaderResources(3, 1, sRendererData->GPassRoughnessAORT->GetSRV().GetAddressOf());

		RenderCommand::Draw(3);

#ifdef TOAST_DEBUG
		if (annotation)
			annotation->EndEvent();
#endif
	}

//	void Renderer::BaseRenderPass()
//	{
//	TOAST_PROFILE_FUNCTION();
//
//#ifdef TOAST_DEBUG
//		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
//		RenderCommand::GetAnnotation(annotation);
//		if (annotation)
//			annotation->BeginEvent(L"Base Render Pass");
//#endif
//
//		//TOAST_CORE_CRITICAL("Base Render Pass");
//
//		RenderCommand::EnableBlending();
//
//		sRendererData->BaseFramebuffer->Bind();
//		sRendererData->BaseFramebuffer->Clear({ 0.24f, 0.24f, 0.24f, 1.0f });
//
//		if (sRendererData->SceneData.SkyboxData.Skybox)
//		{
//			if (sRendererData->SceneData.SkyboxData.Skybox->mVertexBuffer) sRendererData->SceneData.SkyboxData.Skybox->mVertexBuffer->Bind();
//			if (sRendererData->SceneData.SkyboxData.Skybox->mIndexBuffer) sRendererData->SceneData.SkyboxData.Skybox->mIndexBuffer->Bind();
//
//			sRendererData->SceneData.SkyboxData.Skybox->Bind("Skybox");
//
//			sRendererData->EnvironmentBuffer.Write((uint8_t*)&sRendererData->SceneData.SkyboxData.Intensity, 4, 0);
//			sRendererData->EnvironmentBuffer.Write((uint8_t*)&sRendererData->SceneData.SkyboxData.LOD, 4, 4);
//			sRendererData->EnvironmentCBuffer->Map(sRendererData->EnvironmentBuffer);
//
//			RenderCommand::DisableWireframe();
//			RenderCommand::SetPrimitiveTopology(sRendererData->SceneData.SkyboxData.Skybox->mTopology);
//
//			for (Submesh& submesh : sRendererData->SceneData.SkyboxData.Skybox->mSubmeshes)
//				RenderCommand::DrawIndexed(submesh.BaseVertex, submesh.BaseIndex, submesh.IndexCount);
//		}
//
//		for (const auto& meshCommand : sRendererData->MeshDrawList) 
//		{
//			if (meshCommand.Wireframe)
//				RenderCommand::EnableWireframe();
//			else
//				RenderCommand::DisableWireframe();
//
//			RenderCommand::SetPrimitiveTopology(meshCommand.Mesh->mTopology);
//
//			int planet;
//
//			if (!meshCommand.PlanetData)
//			{
//				if (meshCommand.Mesh->IsInstanced())
//				{
//					for (Submesh& submesh : meshCommand.Mesh->mSubmeshes)
//					{
//						bool environment = sRendererData->SceneData.SceneEnvironment.IrradianceMap && sRendererData->SceneData.SceneEnvironment.RadianceMap;
//
//						planet = 0;
//
//						sRendererData->ModelBuffer.Write((uint8_t*)&DirectX::XMMatrixMultiply(submesh.Transform, meshCommand.Transform), 64, 0);
//						sRendererData->ModelBuffer.Write((uint8_t*)&meshCommand.EntityID, 4, 64);
//						sRendererData->ModelBuffer.Write((uint8_t*)&planet, 4, 68);
//						sRendererData->ModelCBuffer->Map(sRendererData->ModelBuffer);
//
//						//meshCommand.Mesh->Map(submesh.MaterialName);
//						meshCommand.Mesh->Bind(submesh.MaterialName, environment);
//
//						uint32_t bufferElements = meshCommand.Mesh->mInstanceVertexBuffer->GetBufferSize() / sizeof(DirectX::XMFLOAT3);
//						RenderCommand::DrawIndexedInstanced(meshCommand.Mesh->mSubmeshes[0].IndexCount, meshCommand.Mesh->GetNumberOfInstances(), 0, 0, 0);
//					}
//				}
//				else
//				{
//					for (Submesh& submesh : meshCommand.Mesh->mSubmeshes)
//					{
//						//TOAST_CORE_INFO("Rendering submesh with material: %s", submesh.MaterialName.c_str());
//						bool environment = sRendererData->SceneData.SceneEnvironment.IrradianceMap && sRendererData->SceneData.SceneEnvironment.RadianceMap;
//
//						planet = 0;
//
//						sRendererData->ModelBuffer.Write((uint8_t*)&DirectX::XMMatrixMultiply(submesh.Transform, meshCommand.Transform), 64, 0);
//						sRendererData->ModelBuffer.Write((uint8_t*)&meshCommand.EntityID, 4, 64);
//						sRendererData->ModelBuffer.Write((uint8_t*)&planet, 4, 68);
//						sRendererData->ModelCBuffer->Map(sRendererData->ModelBuffer);
//
//						//meshCommand.Mesh->Map(submesh.MaterialName);
//						meshCommand.Mesh->Bind(submesh.MaterialName, environment);
//
//						RenderCommand::DrawIndexed(submesh.BaseVertex, submesh.BaseIndex, submesh.IndexCount);
//					}
//				}
//			}
//			else 
//			{
//				//meshCommand.Mesh->Map("Planet");
//				meshCommand.Mesh->Bind("Planet");
//
//				RenderCommand::DrawIndexed(meshCommand.Mesh->mSubmeshes[0].BaseVertex, meshCommand.Mesh->mSubmeshes[0].BaseIndex, meshCommand.Mesh->mSubmeshes[0].IndexCount);
//			}
//		}
//
//#ifdef TOAST_DEBUG
//		if (annotation)
//			annotation->EndEvent();
//#endif
//	}
//
//	void Renderer::PostProcessPass()
//	{
//		TOAST_PROFILE_FUNCTION();
//
//#ifdef TOAST_DEBUG
//		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
//		RenderCommand::GetAnnotation(annotation);
//		if (annotation)
//			annotation->BeginEvent(L"Atmosphere Pass");
//#endif
//		//TOAST_CORE_CRITICAL("Post Process Pass");
//
//		RendererAPI* API = RenderCommand::sRendererAPI.get();
//		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();
//
//		RenderCommand::EnableBlending(); 
//
//		ID3D11RenderTargetView* nullRTV = nullptr;
//		deviceContext->OMSetRenderTargets(1, &nullRTV, nullptr);
//
//		sRendererData->PostProcessFramebuffer->DisableDepth();
//		sRendererData->PostProcessFramebuffer->Bind();
//		sRendererData->PostProcessFramebuffer->Clear({ 0.2f, 0.2f, 0.2f, 1.0f });
//
//		auto depthMask = sRendererData->GPassDepthRT->GetSRV();
//		deviceContext->PSSetShaderResources(9, 1, depthMask.GetAddressOf());
//		auto baseTexture = sRendererData->BaseRenderTarget->GetSRV();
//		deviceContext->PSSetShaderResources(10, 1, baseTexture.GetAddressOf());
//
//		auto sampler = TextureLibrary::GetSampler("Default");
//		sampler->Bind(1, D3D11_PIXEL_SHADER);
//
//		auto atmosphereShader = ShaderLibrary::Get("assets/shaders/Planet/Atmosphere.hlsl");
//		atmosphereShader->Bind();	
//
//		for (const auto& meshCommand : sRendererData->MeshDrawList)
//		{
//			if (meshCommand.PlanetData)
//			{
//				int atmosphereToggle = meshCommand.PlanetData->atmosphereToggle ? 1 : 0;
//
//				sRendererData->AtmosphereBuffer.Write((uint8_t*)&meshCommand.PlanetData->radius, 4, 0);
//				sRendererData->AtmosphereBuffer.Write((uint8_t*)&meshCommand.PlanetData->minAltitude, 4, 4);
//				sRendererData->AtmosphereBuffer.Write((uint8_t*)&meshCommand.PlanetData->maxAltitude, 4, 8);
//				sRendererData->AtmosphereBuffer.Write((uint8_t*)&meshCommand.PlanetData->atmosphereHeight, 4, 12);
//				sRendererData->AtmosphereBuffer.Write((uint8_t*)&meshCommand.PlanetData->mieAnisotropy, 4, 16);
//				sRendererData->AtmosphereBuffer.Write((uint8_t*)&meshCommand.PlanetData->rayScaleHeight, 4, 20);
//				sRendererData->AtmosphereBuffer.Write((uint8_t*)&meshCommand.PlanetData->mieScaleHeight, 4, 24);
//				sRendererData->AtmosphereBuffer.Write((uint8_t*)&meshCommand.PlanetData->rayBaseScatteringCoefficient, 12, 32);
//				sRendererData->AtmosphereBuffer.Write((uint8_t*)&meshCommand.PlanetData->mieBaseScatteringCoefficient, 4, 44);
//				sRendererData->AtmosphereBuffer.Write((uint8_t*)&meshCommand.PlanetData->planetCenter, 16, 48);
//				sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphereToggle, 4, 60);
//				sRendererData->AtmosphereBuffer.Write((uint8_t*)&meshCommand.PlanetData->inScatteringPoints, 4, 64);
//				sRendererData->AtmosphereBuffer.Write((uint8_t*)&meshCommand.PlanetData->opticalDepthPoints, 4, 68);
//
//				sRendererData->AtmosphereCBuffer->Map(sRendererData->AtmosphereBuffer);
//			}
//		}
//
//		RenderCommand::SetPrimitiveTopology(PrimitiveTopology::TRIANGLELIST);
//
//		RenderCommand::Draw(3);
//
//#ifdef TOAST_DEBUG
//		if (annotation)
//			annotation->EndEvent();
//
//		if (annotation)
//			annotation->BeginEvent(L"Tonemapping Pass");
//#endif
//
//		ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
//		deviceContext->PSSetShaderResources(9, 1, nullSRV);
//		deviceContext->PSSetShaderResources(10, 1, nullSRV);
//
//		//Tonemapping
//		sRendererData->FinalFramebuffer->DisableDepth();
//		sRendererData->FinalFramebuffer->Bind();
//		sRendererData->FinalFramebuffer->Clear({ 0.2f, 0.2f, 0.2f, 1.0f });
//
//		sampler->Bind(1, D3D11_PIXEL_SHADER);
//
//		auto toneMappingShader = ShaderLibrary::Get("assets/shaders/ToneMapping.hlsl");
//		toneMappingShader->Bind();
//
//		auto texture = sRendererData->PostProcessRenderTarget->GetSRV();
//		deviceContext->PSSetShaderResources(10, 1, texture.GetAddressOf());
//
//		RenderCommand::Draw(3);
//
//		deviceContext->PSSetShaderResources(10, 1, nullSRV);
//#ifdef TOAST_DEBUG
//		if (annotation)
//			annotation->EndEvent();
//#endif
//	}

	void Renderer::ResetStats()
	{
		memset(&sData.Stats, 0, sizeof(Statistics));
	}

	Renderer::Statistics Renderer::GetStats()
	{
		return sData.Stats;
	}
}