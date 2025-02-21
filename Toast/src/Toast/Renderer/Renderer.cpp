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

	void Renderer::Init(uint32_t width, uint32_t height)
	{
		TOAST_PROFILE_FUNCTION();

		RenderCommand::Init();
		Renderer2D::Init();
		RendererDebug::Init(width, height);

		sRendererData->EditorViewport.TopLeftX = 0.0f;
		sRendererData->EditorViewport.TopLeftY = 0.0f;
		sRendererData->EditorViewport.Width = static_cast<float>(width);
		sRendererData->EditorViewport.Height = static_cast<float>(height);
		sRendererData->EditorViewport.MinDepth = 0.0f;
		sRendererData->EditorViewport.MaxDepth = 1.0f;

		// Setting viewport for shadow mapping
		sRendererData->ShadowMapViewport.TopLeftX = 0.0f;
		sRendererData->ShadowMapViewport.TopLeftY = 0.0f;
		sRendererData->ShadowMapViewport.Width = 8192.0f;
		sRendererData->ShadowMapViewport.Height = 8192.0f;
		sRendererData->ShadowMapViewport.MinDepth = 0.0f;
		sRendererData->ShadowMapViewport.MaxDepth = 1.0f;

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
		sRendererData->MaterialCBuffer = ConstantBufferLibrary::Load("Material", 48, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_PIXEL_SHADER, CBufferBindSlot::Material) });
		sRendererData->MaterialCBuffer->Bind();
		sRendererData->MaterialBuffer.Allocate(sRendererData->MaterialCBuffer->GetSize());
		sRendererData->MaterialBuffer.ZeroInitialize();

		// Setting up the constant buffer and data buffer for lightning rendering
		sRendererData->LightningCBuffer = ConstantBufferLibrary::Load("DirectionalLight", 112, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_VERTEX_SHADER, CBufferBindSlot::DirectionalLight), CBufferBindInfo(D3D11_PIXEL_SHADER, CBufferBindSlot::DirectionalLight) });
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
		sRendererData->AtmosphereCBuffer = ConstantBufferLibrary::Load("Atmosphere", 96, std::vector<CBufferBindInfo>{  CBufferBindInfo(D3D11_PIXEL_SHADER, CBufferBindSlot::Atmosphere) });
		sRendererData->AtmosphereCBuffer->Bind();
		sRendererData->AtmosphereBuffer.Allocate(sRendererData->AtmosphereCBuffer->GetSize());
		sRendererData->AtmosphereBuffer.ZeroInitialize();

		// Setting up the constant buffer for dynamic environmental mapping
		sRendererData->SpecularMapFilterSettingsCBuffer = CreateRef<ConstantBuffer>("SpecularMapFilterSettings", 16, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_COMPUTE_SHADER, CBufferBindSlot::SpecularLightEnvironmental) } );
		sRendererData->SpecularMapFilterSettingsCBuffer->Bind();
		sRendererData->SpecularMapFilterSettingsBuffer.Allocate(sRendererData->SpecularMapFilterSettingsCBuffer->GetSize());
		sRendererData->SpecularMapFilterSettingsBuffer.ZeroInitialize();

		// Setting up the constant buffer for SSAO
		sRendererData->SSAOCBuffer = ConstantBufferLibrary::Load("SSAO", 1040, std::vector<CBufferBindInfo>{  CBufferBindInfo(D3D11_PIXEL_SHADER, CBufferBindSlot::SSAO) });
		sRendererData->SSAOCBuffer->Bind();
		sRendererData->SSAOBuffer.Allocate(sRendererData->SSAOCBuffer->GetSize());
		sRendererData->SSAOBuffer.ZeroInitialize();

		// Setting up the render targets for the Geometry Pass
		sRendererData->GPassPositionRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R32G32B32A32_FLOAT);
		sRendererData->GPassNormalRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R16G16B16A16_FLOAT);
		sRendererData->GPassAlbedoMetallicRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R8G8B8A8_UNORM);
		sRendererData->GPassRoughnessAORT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R8G8B8A8_UNORM);
		sRendererData->GPassPickingRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R32_SINT);

		// Setting up the render target for Shadow Pass
		sRendererData->ShadowMapRT = CreateRef<RenderTarget>(RenderTargetType::Color, 8192, 8192, 1, TextureFormat::R8G8B8A8_UNORM);

		// Setting up the render target for SSAO Pass
		sRendererData->SSAORT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R8G8B8A8_UNORM);

		// Setting up the render target for the Lightning Pass
		sRendererData->LPassRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R16G16B16A16_FLOAT);

		// Setting up the render targets for the Post Process pass
		sRendererData->FinalRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R16G16B16A16_FLOAT, false, true);

		// Setting up the render target for the back buffer
		sRendererData->BackbufferRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R16G16B16A16_FLOAT, true);

		// Setting up the render target for the Atmosphere Pass
		sRendererData->AtmospherePassRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R16G16B16A16_FLOAT);
		sRendererData->AtmosphereCubeRT = CreateRef<RenderTarget>(RenderTargetType::ColorCube, 256, 256, 1, TextureFormat::R16G16B16A16_FLOAT);
		// Setting -Y led to the black since nothing should reflect. 
		// TODO this should most likely be dynamic in the future depending on which color the surface is. It is gray during the night but orange during the day.
		RenderCommand::ClearRenderTargets({ sRendererData->AtmosphereCubeRT->GetRTVFace(0).Get() }, { 0.0f, 0.0f, 0.0f, 0.0f });
		RenderCommand::ClearRenderTargets({ sRendererData->AtmosphereCubeRT->GetRTVFace(1).Get() }, { 0.0f, 0.0f, 0.0f, 0.0f });
		RenderCommand::ClearRenderTargets({ sRendererData->AtmosphereCubeRT->GetRTVFace(2).Get() }, { 0.0f, 0.0f, 0.0f, 0.0f });
		RenderCommand::ClearRenderTargets({ sRendererData->AtmosphereCubeRT->GetRTVFace(3).Get() }, { 0.0f, 0.0f, 0.0f, 0.0f });
		RenderCommand::ClearRenderTargets({ sRendererData->AtmosphereCubeRT->GetRTVFace(4).Get() }, { 0.0f, 0.0f, 0.0f, 0.0f });
		RenderCommand::ClearRenderTargets({ sRendererData->AtmosphereCubeRT->GetRTVFace(5).Get() }, { 0.0f, 0.0f, 0.0f, 0.0f });

		// Setting up dynamic environmental maps 
		sRendererData->EnvMapFiltered = CreateRef<TextureCube>("EnvMapFiltered", 256, 256, 9);
		sRendererData->IrradianceCubeMap = CreateRef<TextureCube>("IrradianceCubemap", 256, 256, 1);

		CreateRasterizerStates();
		CreateDepthBuffer(width, height);
		CreateDepthStencilView();
		CreateDepthStencilStates();

		CreateBlendStates();

		SetUpAtmosphericScatteringMatrices();

		GenerateSampleKernel();
		GenerateNoiseTexture();

		GenerateParticleBuffers();
	}

	void Renderer::Shutdown()
	{
		Renderer2D::Shutdown();
		RendererDebug::Shutdown();
	}

	void Renderer::OnWindowResize(uint32_t width, uint32_t height)
	{
		sRendererData->EditorViewport.TopLeftX = 0.0f;
		sRendererData->EditorViewport.TopLeftY = 0.0f;
		sRendererData->EditorViewport.Width = static_cast<float>(width);
		sRendererData->EditorViewport.Height = static_cast<float>(height);
		sRendererData->EditorViewport.MinDepth = 0.0f;
		sRendererData->EditorViewport.MaxDepth = 1.0f;

		sRendererData->BackbufferRT.reset();
		RenderCommand::ResizeViewport(0, 0, width, height);
		sRendererData->BackbufferRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R16G16B16A16_FLOAT, true);
	}

	void Renderer::OnViewportResize(uint32_t width, uint32_t height)
	{
		sRendererData->Viewport.TopLeftX = 0.0f;
		sRendererData->Viewport.TopLeftY = 0.0f;
		sRendererData->Viewport.Width = static_cast<float>(width);
		sRendererData->Viewport.Height = static_cast<float>(height);
		sRendererData->Viewport.MinDepth = 0.0f;
		sRendererData->Viewport.MaxDepth = 1.0f;

		sRendererData->GPassPositionRT->Resize(width, height);
		sRendererData->GPassNormalRT->Resize(width, height);
		sRendererData->GPassAlbedoMetallicRT->Resize(width, height);
		sRendererData->GPassRoughnessAORT->Resize(width, height);
		sRendererData->GPassPickingRT->Resize(width, height);

		sRendererData->SSAORT->Resize(width, height);

		sRendererData->LPassRT->Resize(width, height);

		sRendererData->AtmospherePassRT->Resize(width, height);

		sRendererData->FinalRT->Resize(width, height);

		sRendererData->DepthStencilView.Reset();
		sRendererData->ShadowPassStencilView.Reset();

		CreateDepthBuffer(width, height);
		CreateDepthStencilView();

		RendererDebug::OnWindowResize(width, height);
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
		sRendererData->CameraBuffer.Write((uint8_t*)&sRendererData->Viewport.Width, 4, 280);
		sRendererData->CameraBuffer.Write((uint8_t*)&sRendererData->Viewport.Height, 4, 284);
		sRendererData->CameraCBuffer->Map(sRendererData->CameraBuffer);

		// Updating the lightning data in the buffer and mapping it to the GPU
		sRendererData->LightningBuffer.Write((uint8_t*)&scene->mLightEnvironment.DirectionalLights[0].ViewProjectionMatrix, 64, 0);
		sRendererData->LightningBuffer.Write((uint8_t*)&scene->mLightEnvironment.DirectionalLights[0].Direction, 16, 64);
		sRendererData->LightningBuffer.Write((uint8_t*)&scene->mLightEnvironment.DirectionalLights[0].Radiance, 16, 80);
		sRendererData->LightningBuffer.Write((uint8_t*)&scene->mLightEnvironment.DirectionalLights[0].Multiplier, 4, 96);
		sRendererData->LightningCBuffer->Map(sRendererData->LightningBuffer);

		sRendererData->SceneData.SceneEnvironment = scene->mEnvironment;
		sRendererData->SceneData.SceneEnvironmentIntensity = scene->mEnvironmentIntensity;

		// TODO: These one could most likely be removed 
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
		float renderOverlay = (float)scene->mSettings.RenderOverlaySetting;
		sRendererData->RenderSettingsBuffer.Write((uint8_t*)&renderOverlay, 4, 0);
		sRendererData->RenderSettingsCBuffer->Map(sRendererData->RenderSettingsBuffer);
	}

	void Renderer::EndScene(const bool debugActivated, const bool shadows, const bool SSAO, const bool dynamicIBL, Camera& camera, const DirectX::XMFLOAT4 cameraPos)
	{
		RenderCommand::SetViewport(sRendererData->Viewport);

		// Deffered Renderer
		GeometryPass();

		if(shadows)
			ShadowPass();
		else
			RenderCommand::ClearDepthStencilView(sRendererData->ShadowPassStencilView);

		if(SSAO)
			SSAOPass();
		else
			RenderCommand::ClearRenderTargets(sRendererData->SSAORT->GetRTV().Get(), { 1.0f, 1.0f, 1.0f, 1.0f });

		LightningPass();

		// Post Processes
		SkyboxPass();
		AtmospherePass(dynamicIBL);

		// Particles only for now, but will most likely be renamed and handle more things in the future.
		// If there are no particles that needs to be rendered, this pass will be skipped.
		if (sRendererData->ParticleIndexBuffer.Get())
			ParticlesPass(camera, cameraPos);

		PostProcessPass();

		if (!debugActivated) 
		{
			RenderCommand::SetRenderTargets({ sRendererData->BackbufferRT->GetRTV().Get() }, nullptr);
			RenderCommand::ClearRenderTargets(sRendererData->BackbufferRT->GetRTV().Get() , { 0.0f, 0.0f, 0.0f, 1.0f });
		}

		ClearDrawList();
	}

	void Renderer::CreateDepthBuffer(uint32_t width, uint32_t height)
	{
		sRendererData->DepthBuffer = CreateScope<Texture2D>((DXGI_FORMAT)TextureFormat::R32_TYPELESS, (DXGI_FORMAT)TextureFormat::R32_FLOAT, width, height, D3D11_USAGE_DEFAULT, (D3D11_BIND_FLAG)(D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE), 1);

		sRendererData->ShadowPassDepth = CreateScope<Texture2D>((DXGI_FORMAT)TextureFormat::R32_TYPELESS, (DXGI_FORMAT)TextureFormat::R32_FLOAT, 8192, 8192, D3D11_USAGE_DEFAULT, (D3D11_BIND_FLAG)(D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE), 1);
	}

	void Renderer::CreateDepthStencilView()
	{
		HRESULT result;

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = (DXGI_FORMAT)TextureFormat::D32_FLOAT;
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Texture2D.MipSlice = 0;

		result = device->CreateDepthStencilView(sRendererData->DepthBuffer->GetTexture().Get(), &dsvDesc, &sRendererData->DepthStencilView);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create depth stencil view!");

		result = device->CreateDepthStencilView(sRendererData->ShadowPassDepth->GetTexture().Get(), &dsvDesc, &sRendererData->ShadowPassStencilView);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create depth stencil view!");
	}

	void Renderer::CreateDepthStencilStates()
	{
		HRESULT result;

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		// Create Depth Stencil State
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
		depthStencilDesc.DepthEnable = true;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;

		depthStencilDesc.StencilEnable = true;
		depthStencilDesc.StencilReadMask = 0xFF;
		depthStencilDesc.StencilWriteMask = 0xFF;

		depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
		depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

		depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
		depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

		result = device->CreateDepthStencilState(&depthStencilDesc, &sRendererData->DepthEnabledStencilState);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create enabled depth stencil state");

		depthStencilDesc.DepthEnable = TRUE;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
		depthStencilDesc.StencilEnable = FALSE;

		HRESULT hr = device->CreateDepthStencilState(&depthStencilDesc, &sRendererData->ParticleDepthStencilState);
		TOAST_CORE_ASSERT(SUCCEEDED(hr), "Failed to create depth stencil state");

		depthStencilDesc.DepthEnable = false;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

		depthStencilDesc.StencilEnable = false;
		depthStencilDesc.StencilReadMask = 0x00;
		depthStencilDesc.StencilWriteMask = 0x00;

		result = device->CreateDepthStencilState(&depthStencilDesc, &sRendererData->DepthDisabledStencilState);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create disabled depth stencil state");

		depthStencilDesc.DepthEnable = true;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;

		depthStencilDesc.StencilEnable = false;
		depthStencilDesc.StencilReadMask = 0x00;
		depthStencilDesc.StencilWriteMask = 0x00;

		result = device->CreateDepthStencilState(&depthStencilDesc, &sRendererData->DepthSkyboxPassStencilState);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create Skybox pass depth stencil state");
	}

	void Renderer::CreateBlendStates()
	{
		HRESULT result;
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		// Geometry Pass Blend State
		{
			D3D11_BLEND_DESC blendDesc = {};
			blendDesc.AlphaToCoverageEnable = FALSE;
			blendDesc.IndependentBlendEnable = TRUE; // Allows different settings per render target

			const std::vector<Ref<RenderTarget>> renderTargets = { sRendererData->GPassPositionRT, sRendererData->GPassNormalRT, sRendererData->GPassAlbedoMetallicRT, sRendererData->GPassRoughnessAORT, sRendererData->GPassPickingRT };
			size_t numRenderTargets = renderTargets.size();

			TOAST_CORE_ASSERT(numRenderTargets <= D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, "Too many render targets");

			// Collect blend descriptions from each render target
			for (size_t i = 0; i < numRenderTargets; ++i)
			{
				const D3D11_RENDER_TARGET_BLEND_DESC& rtBlendDesc = renderTargets[i]->GetBlendDesc();
				blendDesc.RenderTarget[i] = rtBlendDesc;
			}

			result = device->CreateBlendState(&blendDesc, &sRendererData->GPassBlendState);
			TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create GPass blend state");
		}

		// Lightning Pass Blend State
		{
			D3D11_BLEND_DESC blendDesc = {};
			blendDesc.AlphaToCoverageEnable = FALSE;
			blendDesc.IndependentBlendEnable = TRUE;

			const D3D11_RENDER_TARGET_BLEND_DESC& rtBlendDesc = sRendererData->LPassRT->GetBlendDesc();
			blendDesc.RenderTarget[0] = rtBlendDesc;

			result = device->CreateBlendState(&blendDesc, &sRendererData->LPassBlendState);
			TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create LPass blend state");
		}

		// Particle Pass Blend State
		{
			D3D11_BLEND_DESC blendDesc = {};
			blendDesc.AlphaToCoverageEnable = FALSE;
			blendDesc.IndependentBlendEnable = FALSE;

			blendDesc.RenderTarget[0].BlendEnable = TRUE;
			blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
			blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
			blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
			blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			//blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
			//blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
			//blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			//blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			//blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
			//blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

			HRESULT hr = device->CreateBlendState(&blendDesc, &sRendererData->ParticleBlendState);
			TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create Particle Pass blend state");
		}

		// Atmosphere Pass Blend State
		{
			D3D11_BLEND_DESC blendDesc = {};
			blendDesc.AlphaToCoverageEnable = FALSE;
			blendDesc.IndependentBlendEnable = FALSE;

			blendDesc.RenderTarget[0].BlendEnable = TRUE;
			blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
			blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
			blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_INV_DEST_ALPHA;
			blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
			blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

			result = device->CreateBlendState(&blendDesc, &sRendererData->AtmospherePassBlendState);
			TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create Atmosphere Pass blend state");
		}

		// UI Pass Blend State
		{
			D3D11_BLEND_DESC blendDesc = {};
			blendDesc.AlphaToCoverageEnable = FALSE;
			blendDesc.IndependentBlendEnable = TRUE;

			blendDesc.RenderTarget[0].BlendEnable = TRUE;
			blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
			blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
			blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_INV_DEST_ALPHA;
			blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
			blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

			blendDesc.RenderTarget[1].BlendEnable = FALSE; // Disable blending for slot 1
			blendDesc.RenderTarget[1].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

			result = device->CreateBlendState(&blendDesc, &sRendererData->UIBlendState);
			TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create Atmosphere Pass blend state");
		}
	}

	void Renderer::CreateRasterizerStates()
	{
		HRESULT result;
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();
		D3D11_RASTERIZER_DESC rasterDesc{};

		memset(&rasterDesc, 0, sizeof(D3D11_RASTERIZER_DESC));
		rasterDesc.CullMode = D3D11_CULL_NONE;
		rasterDesc.FillMode = D3D11_FILL_SOLID;
		rasterDesc.DepthClipEnable = true;

		result = device->CreateRasterizerState(&rasterDesc, &sRendererData->NormalRasterizerState);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create normal rasterizer state");

		rasterDesc.FillMode = D3D11_FILL_WIREFRAME;

		result = device->CreateRasterizerState(&rasterDesc, &sRendererData->WireframeRasterizerState);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create wireframe rasterizer state");

		rasterDesc.CullMode = D3D11_CULL_FRONT;
		rasterDesc.FillMode = D3D11_FILL_SOLID;
		rasterDesc.DepthClipEnable = true;

		result = device->CreateRasterizerState(&rasterDesc, &sRendererData->ShadowMapRasterizerState);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create shadow pass rasterizer state");
	}

	void Renderer::GenerateSampleKernel()
	{
		// Random engine & distribution setup
		std::random_device rd;
		std::mt19937 gen(rd());

		// For sampling -1..1 on x/y, and 0..1 on z
		// We only allow z >= 0 to constrain to the hemisphere pointing +Z
		std::uniform_real_distribution<float> randomX(-1.0f, 1.0f);
		std::uniform_real_distribution<float> randomY(-1.0f, 1.0f);
		std::uniform_real_distribution<float> randomZ(0.0f, 1.0f);

		// The resulting kernel
		const int KERNEL_SIZE = 64;
		sRendererData->SSAOKernel.reserve(KERNEL_SIZE);

		for (int i = 0; i < KERNEL_SIZE; ++i)
		{
			float x = randomX(gen);
			float y = randomY(gen);
			float z = randomZ(gen);

			Vector3 sample = { x, y, z };
			sample = Vector3::Normalize(sample);

			// Scale samples so more are close to the origin
			float scale = static_cast<float>(i) / static_cast<float>(KERNEL_SIZE);
			// You can tweak how fast the scale ramps up
			// e.g. "scale = lerp(0.1f, 1.0f, scale * scale)" is a common approach
			scale = 0.1f + scale * (1.0f - 0.1f); 

			sample *= scale;

			sRendererData->SSAOKernel.emplace_back(DirectX::XMFLOAT4(sample.x, sample.y, sample.z, 0.0f));
		}

		float radius = 7.5f;

		sRendererData->SSAOBuffer.Write((uint8_t*)&sRendererData->SSAOKernel[0], 1024, 0);
		sRendererData->SSAOBuffer.Write((uint8_t*)&radius, 4, 1024);
	}

	void Renderer::GenerateNoiseTexture()
	{
		const int NOISE_DIM = 4;
		std::vector<DirectX::XMFLOAT3> SSAONoise;
		SSAONoise.reserve(NOISE_DIM * NOISE_DIM);
		for (int i = 0; i < NOISE_DIM * NOISE_DIM; i++) {
			float x = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
			float y = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
			// z=0 so the noise vectors lie in the tangent plane
			SSAONoise.push_back({ x, y, 0.0f });
		}

		uint32_t width = NOISE_DIM;
		uint32_t height = NOISE_DIM;
		uint32_t rowPitch = static_cast<uint32_t>(width * sizeof(DirectX::XMFLOAT3));

		sRendererData->SSAONoiseTexture = CreateScope<Texture2D>(
			DXGI_FORMAT_R32G32B32A32_FLOAT,  // Texture format
			DXGI_FORMAT_R32G32B32A32_FLOAT,  // SRV format (same as texture in this case)
			width,
			height,
			D3D11_USAGE_IMMUTABLE,
			D3D11_BIND_SHADER_RESOURCE,
			1,                 // samples
			0,                 // CPU access flags
			SSAONoise.data(),   // initial data
			rowPitch
		);
	}

	void Renderer::SetUpAtmosphericScatteringMatrices()
	{
		// Define the directions and up vectors for each cube face
		static const DirectX::XMVECTOR directions[6] = {
			DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f),  // +X
			DirectX::XMVectorSet(-1.0f, 0.0f, 0.0f, 0.0f), // -X
			DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),  // +Y
			DirectX::XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f), // -Y
			DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),  // +Z
			DirectX::XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f)  // -Z
		};

		static const DirectX::XMVECTOR upVectors[6] = {
			DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), // Up for +X
			DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), // Up for -X
			DirectX::XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f),  // Up for +Y
			DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), // Up for -Y
			DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), // Up for +Z
			DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)  // Up for -Z
		};

		// Cube center
		DirectX::XMVECTOR cubeCenter = DirectX::XMVectorZero();

		// Calculate view matrices for each face
		for (int i = 0; i < 6; ++i)
		{
			// Compute the view matrix for this face
			sRendererData->AtmosphericScatteringViewMatrices[i] = DirectX::XMMatrixLookToLH(cubeCenter, directions[i], upVectors[i]);
			sRendererData->AtmosphericScatteringInvViewMatrices[i] = DirectX::XMMatrixInverse(nullptr, sRendererData->AtmosphericScatteringViewMatrices[i]);
		}
	}

	void Renderer::Submit(const Ref<IndexBuffer>& indexBuffer, const Ref<Shader> shader, const Ref<ShaderLayout> bufferLayout, const Ref<VertexBuffer> vertexBuffer, const DirectX::XMMATRIX& transform)
	{
		bufferLayout->Bind();
		vertexBuffer->Bind();
		indexBuffer->Bind();
		shader->Bind();
	}

	//Todo should be integrated into SubmitMesh later on
	void Renderer::SubmitSkybox(const DirectX::XMFLOAT4& cameraPos, const DirectX::XMFLOAT4X4& viewMatrix, const DirectX::XMFLOAT4X4& projectionMatrix, float intensity, float LOD)
	{
		sRendererData->CameraPos = cameraPos;
		sRendererData->ViewMatrix = viewMatrix;
		sRendererData->ProjectionMatrix = projectionMatrix;
		sRendererData->SceneData.SkyboxData.Intensity = intensity;
		sRendererData->SceneData.SkyboxData.LOD = LOD;
	}

	void Renderer::SubmitMesh(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, const int entityID, bool wireframe, int noWorldTransform, PlanetComponent::GPUData* planetData, bool atmosphere)
	{
		sRendererData->PlanetData.Atmosphere = atmosphere;
;		sRendererData->MeshDrawList.emplace_back(mesh, transform, wireframe, noWorldTransform, entityID, planetData);
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
		sRendererData->MeshWireframeDrawList.clear();
		sRendererData->MeshNoWireframeDrawList.clear();
	}

	static Scope<Shader> equirectangularConversionShader, envFilteringShader, envIrradianceShader;

	Ref<TextureCube> Renderer::CreateEnvironmentMap(const std::string& filepath)
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		const uint32_t cubemapSize = 2048;
		const uint32_t irradianceMapSize = 64;

		Ref<ConstantBuffer> specularMapFilterSettingsCB = CreateRef<ConstantBuffer>("SpecularMapFilterSettings", 16, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_COMPUTE_SHADER, CBufferBindSlot::SpecularLightEnvironmental) }, D3D11_USAGE_DEFAULT);

		Ref<Texture2D> starMap = CreateRef<Texture2D>(filepath);
		TextureSampler* defaultSampler = TextureLibrary::GetSampler("Default");
		Ref<TextureCube> envMapUnfiltered = CreateRef<TextureCube>("EnvMapUnfiltered", cubemapSize, cubemapSize);
		//Ref<TextureCube> envMapFiltered = CreateRef<TextureCube>("EnvMapFiltered", cubemapSize, cubemapSize);

		envMapUnfiltered->CreateUAV(0);

		if (!equirectangularConversionShader)
			equirectangularConversionShader = CreateScope<Shader>("assets/shaders/Environment/EquirectangularToCubeMap.hlsl");

		equirectangularConversionShader->Bind();
		starMap->Bind();
		defaultSampler->Bind(0, D3D11_COMPUTE_SHADER);
		envMapUnfiltered->BindForReadWrite(0, D3D11_COMPUTE_SHADER);
		RenderCommand::DispatchCompute(cubemapSize / 32, cubemapSize / 32, 6);
		envMapUnfiltered->UnbindUAV();

		//envMapUnfiltered->GenerateMips();

		//for (int arraySlice = 0; arraySlice < 6; ++arraySlice) {
		//	const uint32_t subresourceIndex = D3D11CalcSubresource(0, arraySlice, envMapFiltered->GetMipLevelCount());
		//	deviceContext->CopySubresourceRegion(envMapFiltered->GetResource(), subresourceIndex, 0, 0, 0, envMapUnfiltered->GetResource(), subresourceIndex, nullptr);
		//}

		//struct SpecularMapFilterSettingsCB
		//{
		//	float roughness;
		//	float padding[3];
		//};

		//if (!envFilteringShader)
		//	envFilteringShader = CreateScope<Shader>("assets/shaders/Environment/EnvironmentMipFilter.hlsl");

		//envFilteringShader->Bind();
		//envMapUnfiltered->Bind(0, D3D11_COMPUTE_SHADER);
		//defaultSampler->Bind(0, D3D11_COMPUTE_SHADER);

		//// Pre-filter rest of the mip chain.
		//const float deltaRoughness = 1.0f / std::max(float(envMapFiltered->GetMipLevelCount() - 1.0f), 1.0f);
		//for (int level = 1, size = cubemapSize / 2; level < envMapFiltered->GetMipLevelCount(); ++level, size /= 2) {
		//	const int numGroups = std::max(1, size / 32);

		//	envMapFiltered->CreateUAV(level);
		//	
		//	const SpecularMapFilterSettingsCB spmapConstants = { level * deltaRoughness };
		//	deviceContext->UpdateSubresource(specularMapFilterSettingsCB->GetBuffer(), 0, nullptr, &spmapConstants, 0, 0);

		//	specularMapFilterSettingsCB->Bind();
		//	envMapFiltered->BindForReadWrite(0, D3D11_COMPUTE_SHADER);
		//	RenderCommand::DispatchCompute(numGroups, numGroups, 6);
		//}
		//envMapFiltered->UnbindUAV();

		//Ref<TextureCube> irradianceMap = CreateRef<TextureCube>("IrradianceMap", irradianceMapSize, irradianceMapSize, 1);

		//if (!envIrradianceShader)
		//	envIrradianceShader = CreateScope<Shader>("assets/shaders/Environment/EnvironmentIrradiance.hlsl");

		//irradianceMap->CreateUAV(0);

		//envMapFiltered->Bind(0, D3D11_COMPUTE_SHADER);
		//irradianceMap->BindForReadWrite(0, D3D11_COMPUTE_SHADER);
		//defaultSampler->Bind(0, D3D11_COMPUTE_SHADER);
		//envIrradianceShader->Bind();
		//RenderCommand::DispatchCompute(irradianceMap->GetWidth() / 32, irradianceMap->GetHeight() / 32, 6);
		//irradianceMap->UnbindUAV();

		return envMapUnfiltered;
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
		RenderCommand::SetViewport(sRendererData->Viewport);
		RenderCommand::SetRenderTargets({ sRendererData->GPassPositionRT->GetRTV().Get(), sRendererData->GPassNormalRT->GetRTV().Get(), sRendererData->GPassAlbedoMetallicRT->GetRTV().Get(), sRendererData->GPassRoughnessAORT->GetRTV().Get(), sRendererData->GPassPickingRT->GetRTV().Get() }, sRendererData->DepthStencilView);
		RenderCommand::SetDepthStencilState(sRendererData->DepthEnabledStencilState);
		RenderCommand::SetBlendState(sRendererData->GPassBlendState, { 0.0f, 0.0f, 0.0f, 0.0f });
		RenderCommand::ClearDepthStencilView(sRendererData->DepthStencilView);
		RenderCommand::ClearRenderTargets({ sRendererData->GPassPositionRT->GetRTV().Get(), sRendererData->GPassNormalRT->GetRTV().Get(), sRendererData->GPassAlbedoMetallicRT->GetRTV().Get(), sRendererData->GPassRoughnessAORT->GetRTV().Get(), sRendererData->GPassPickingRT->GetRTV().Get() }, { 0.0f, 0.0f, 0.0f, 1.0f });
		RenderCommand::SetPrimitiveTopology(Topology::TRIANGLELIST);

		ShaderLibrary::Get("assets/shaders/Rendering/GeometryPass.hlsl")->Bind();

		for (const auto& meshCommand : sRendererData->MeshDrawList)
		{
			if (meshCommand.Wireframe)
				RenderCommand::SetRasterizerState(sRendererData->WireframeRasterizerState);
			else
				RenderCommand::SetRasterizerState(sRendererData->NormalRasterizerState);

			RenderCommand::SetPrimitiveTopology(meshCommand.Mesh->mTopology);

			int isInstanced = meshCommand.Mesh->IsInstanced() ? 1 : 0;

			// Model data
			sRendererData->ModelBuffer.Write((uint8_t*)&meshCommand.Transform, 64, 0);
			sRendererData->ModelBuffer.Write((uint8_t*)&meshCommand.EntityID, 4, 64);
			sRendererData->ModelBuffer.Write((uint8_t*)&meshCommand.NoWorldTransform, 4, 68);
			sRendererData->ModelBuffer.Write((uint8_t*)&isInstanced, 4, 72);
			sRendererData->ModelCBuffer->Map(sRendererData->ModelBuffer);

			for (Submesh& submesh : meshCommand.Mesh->mLODGroups[meshCommand.Mesh->mActiveLODGroup]->Submeshes)
			{
				// Material data
				auto& material = meshCommand.Mesh->GetMaterial(submesh.MaterialName);
				sRendererData->MaterialBuffer.Write((uint8_t*)&material->GetAlbedo(), 16, 0);
				sRendererData->MaterialBuffer.Write((uint8_t*)&material->GetEmission(), 4, 16);
				sRendererData->MaterialBuffer.Write((uint8_t*)&material->GetMetalness(), 4, 20);
				sRendererData->MaterialBuffer.Write((uint8_t*)&material->GetRoughness(), 4, 24);
				int useAlbedo = static_cast<int>(material->GetUseAlbedo());
				sRendererData->MaterialBuffer.Write((uint8_t*)&useAlbedo, 4, 28);
				int useNormal = static_cast<int>(material->GetUseNormal());
				sRendererData->MaterialBuffer.Write((uint8_t*)&useNormal, 4, 32);
				int useMetalRough = static_cast<int>(material->GetUseMetalRough());
				sRendererData->MaterialBuffer.Write((uint8_t*)&useMetalRough, 4, 36);
				sRendererData->MaterialCBuffer->Map(sRendererData->MaterialBuffer);

				if(material->GetUseAlbedo())
					RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 3, material->GetAlbedoTexture()->GetSRV());
				if (material->GetUseNormal())
					RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 4, material->GetNormalTexture()->GetSRV());
				if (material->GetUseMetalRough())
					RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 5, material->GetMetalRoughTexture()->GetSRV());

				meshCommand.Mesh->Bind();

				if (isInstanced == 0) 
				{
					//TOAST_CORE_CRITICAL("Drawing Submesh '%s'", submesh.MeshName.c_str());
					RenderCommand::DrawIndexed(0, submesh.BaseIndex, submesh.IndexCount);
				}
				else 
				{
					uint32_t bufferElements = meshCommand.Mesh->mLODGroups[0]->InstancedVBuffer->GetBufferSize() / sizeof(DirectX::XMFLOAT3);
					RenderCommand::DrawIndexedInstanced(meshCommand.Mesh->mLODGroups[0]->Submeshes[0].IndexCount, meshCommand.Mesh->GetNumberOfInstances(0), 0, 0, 0);
				}
			}
		}

		std::vector<ID3D11RenderTargetView*> nullRTVs(6, nullptr);

		RenderCommand::SetRenderTargets(nullRTVs, nullptr);
		RenderCommand::SetDepthStencilState(nullptr);
		RenderCommand::SetBlendState(nullptr);
		RenderCommand::ClearShaderResources();

#ifdef TOAST_DEBUG
		if (annotation)
			annotation->EndEvent();
#endif
	}

	void Renderer::ShadowPass()
	{
		TOAST_PROFILE_FUNCTION();

#ifdef TOAST_DEBUG
		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation)
			annotation->BeginEvent(L"Shadow Pass");
#endif

		RenderCommand::SetViewport(sRendererData->ShadowMapViewport);
		RenderCommand::SetRasterizerState(sRendererData->ShadowMapRasterizerState);
		RenderCommand::SetRenderTargets({ sRendererData->ShadowMapRT->GetRTV().Get() }, sRendererData->ShadowPassStencilView);
		RenderCommand::SetDepthStencilState(sRendererData->DepthEnabledStencilState);
		RenderCommand::SetBlendState(sRendererData->GPassBlendState, { 0.0f, 0.0f, 0.0f, 0.0f });
		RenderCommand::ClearDepthStencilView(sRendererData->ShadowPassStencilView);
		RenderCommand::ClearRenderTargets({ sRendererData->ShadowMapRT->GetRTV().Get() }, { 0.0f, 0.0f, 0.0f, 1.0f });
		RenderCommand::SetPrimitiveTopology(Topology::TRIANGLELIST);

		ShaderLibrary::Get("assets/shaders/Rendering/ShadowPass.hlsl")->Bind();

		for (const auto& meshCommand : sRendererData->MeshDrawList)
		{
			meshCommand.Mesh->Bind();

			int isInstanced = meshCommand.Mesh->IsInstanced() ? 1 : 0;

			// Model data
			sRendererData->ModelBuffer.Write((uint8_t*)&meshCommand.Transform, 64, 0);
			sRendererData->ModelBuffer.Write((uint8_t*)&meshCommand.EntityID, 4, 64);
			sRendererData->ModelBuffer.Write((uint8_t*)&meshCommand.NoWorldTransform, 4, 68);
			sRendererData->ModelBuffer.Write((uint8_t*)&isInstanced, 4, 72);
			sRendererData->ModelCBuffer->Map(sRendererData->ModelBuffer);

			RenderCommand::DrawIndexed(0, 0, meshCommand.Mesh->GetIndices().size());
		}

		ID3D11RenderTargetView* nullRTV = nullptr;
		RenderCommand::SetRenderTargets({ nullRTV }, nullptr);
		RenderCommand::SetDepthStencilState(nullptr);
		RenderCommand::SetBlendState(nullptr);
		RenderCommand::ClearShaderResources();

#ifdef TOAST_DEBUG
		if (annotation)
			annotation->EndEvent();
#endif
	}

	void Renderer::SSAOPass()
	{
		TOAST_PROFILE_FUNCTION();

#ifdef TOAST_DEBUG
		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation)
			annotation->BeginEvent(L"SSAO Pass");
#endif

		RenderCommand::SetViewport(sRendererData->Viewport);
		RenderCommand::SetRasterizerState(sRendererData->NormalRasterizerState);
		RenderCommand::SetDepthStencilState(sRendererData->DepthDisabledStencilState);
		RenderCommand::SetRenderTargets({ sRendererData->SSAORT->GetRTV().Get() }, nullptr);
		RenderCommand::ClearRenderTargets({ sRendererData->SSAORT->GetRTV().Get() }, { 0.0f, 0.0f, 0.0f, 1.0f });

		ShaderLibrary::Get("assets/shaders/Rendering/SSAOPass.hlsl")->Bind();

		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 0, sRendererData->GPassPositionRT->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 1, sRendererData->GPassNormalRT->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 2, sRendererData->SSAONoiseTexture->GetSRV());

		TextureLibrary::GetSampler("LinearSampler")->Bind(4, D3D11_PIXEL_SHADER);

		sRendererData->SSAOCBuffer->Map(sRendererData->SSAOBuffer);

		DrawFullscreenQuad();

		ID3D11RenderTargetView* nullRTV = nullptr;
		RenderCommand::SetRenderTargets({ nullRTV }, nullptr);
		RenderCommand::SetDepthStencilState(nullptr);
		RenderCommand::SetBlendState(nullptr);
		RenderCommand::ClearShaderResources();

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

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> defaultWhite2DSRV = TextureLibrary::Get("assets/textures/White.png")->GetSRV();

		RenderCommand::SetViewport(sRendererData->Viewport);
		RenderCommand::SetRasterizerState(sRendererData->NormalRasterizerState);
		RenderCommand::SetRenderTargets({ sRendererData->LPassRT->GetRTV().Get() }, nullptr);
		RenderCommand::SetDepthStencilState(sRendererData->DepthDisabledStencilState);
		RenderCommand::SetBlendState(sRendererData->LPassBlendState, { 0.0f, 0.0f, 0.0f, 0.0f });
		RenderCommand::ClearRenderTargets({ sRendererData->LPassRT->GetRTV().Get() }, { 0.0f, 0.0f, 0.0f, 1.0f });

		ShaderLibrary::Get("assets/shaders/Rendering/LightningPass.hlsl")->Bind();

		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 0, sRendererData->GPassPositionRT->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 1, sRendererData->GPassNormalRT->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 2, sRendererData->GPassAlbedoMetallicRT->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 3, sRendererData->GPassRoughnessAORT->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 10, sRendererData->SSAORT->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 12, sRendererData->ShadowPassDepth->GetSRV());

		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 4, sRendererData->IrradianceCubeMap->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 5, sRendererData->EnvMapFiltered->GetSRV());

		if (sRendererData->SceneData.SceneEnvironment.SpecularBRDFLUT)
			RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 6, sRendererData->SceneData.SceneEnvironment.SpecularBRDFLUT->GetSRV());
		else
			RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 6, defaultWhite2DSRV);

		TextureLibrary::GetSampler("Default")->Bind(0, D3D11_PIXEL_SHADER);
		TextureLibrary::GetSampler("BRDFSampler")->Bind(1, D3D11_PIXEL_SHADER);

		DrawFullscreenQuad();

		ID3D11RenderTargetView* nullRTV = nullptr;
		RenderCommand::SetRenderTargets({ nullRTV }, nullptr);
		RenderCommand::SetDepthStencilState(nullptr);
		RenderCommand::SetBlendState(nullptr);
		RenderCommand::ClearShaderResources();

#ifdef TOAST_DEBUG
		if (annotation)
			annotation->EndEvent();
#endif
	}

	void Renderer::SkyboxPass()
	{
#ifdef TOAST_DEBUG
		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation)
			annotation->BeginEvent(L"Skybox Pass");
#endif

		if (sRendererData->SceneData.SceneEnvironment.RadianceMap)
		{
			RenderCommand::SetRenderTargets({ sRendererData->LPassRT->GetRTV().Get() }, sRendererData->DepthStencilView);
			RenderCommand::SetDepthStencilState(sRendererData->DepthSkyboxPassStencilState);
			RenderCommand::SetBlendState(sRendererData->LPassBlendState, { 0.0f, 0.0f, 0.0f, 0.0f });

			RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 5, sRendererData->SceneData.SceneEnvironment.RadianceMap->GetSRV());

			ShaderLibrary::Get("assets/shaders/Post Process/Skybox.hlsl")->Bind();

			sRendererData->EnvironmentBuffer.Write((uint8_t*)&sRendererData->SceneData.SkyboxData.Intensity, 4, 0);
			sRendererData->EnvironmentBuffer.Write((uint8_t*)&sRendererData->SceneData.SkyboxData.LOD, 4, 4);
			sRendererData->EnvironmentCBuffer->Map(sRendererData->EnvironmentBuffer);

			DrawFullscreenQuad();
		}

		ID3D11RenderTargetView* nullRTV = nullptr;
		RenderCommand::SetRenderTargets({ nullRTV }, nullptr);

#ifdef TOAST_DEBUG
		if (annotation)
			annotation->EndEvent();
#endif
	}

	void Renderer::AtmospherePass(const bool dynamicIBL)
	{
#ifdef TOAST_DEBUG
		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation)
			annotation->BeginEvent(L"Atmosphere Pass");
#endif

		RenderCommand::SetRenderTargets({ sRendererData->AtmospherePassRT->GetRTV().Get() }, nullptr);
		RenderCommand::SetDepthStencilState(sRendererData->DepthEnabledStencilState);
		RenderCommand::SetBlendState(sRendererData->AtmospherePassBlendState, { 0.0f, 0.0f, 0.0f, 0.0f });

		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 10, sRendererData->LPassRT->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 9, sRendererData->DepthBuffer->GetSRV());

		int useDepth = 1;

		for (const auto& meshCommand : sRendererData->MeshDrawList)
		{
			if (meshCommand.PlanetData)
			{
				int atmosphereToggle = meshCommand.PlanetData->atmosphereToggle ? 1 : 0;
				int sunDiscToggle = meshCommand.PlanetData->SunDisc ? 1 : 0;

				sRendererData->AtmosphereBuffer.Write((uint8_t*)&meshCommand.PlanetData->radius, 4, 0);
				sRendererData->AtmosphereBuffer.Write((uint8_t*)&meshCommand.PlanetData->minAltitude, 4, 4);
				sRendererData->AtmosphereBuffer.Write((uint8_t*)&meshCommand.PlanetData->maxAltitude, 4, 8);
				sRendererData->AtmosphereBuffer.Write((uint8_t*)&meshCommand.PlanetData->atmosphereHeight, 4, 12);
				sRendererData->AtmosphereBuffer.Write((uint8_t*)&meshCommand.PlanetData->mieAnisotropy, 4, 16);
				sRendererData->AtmosphereBuffer.Write((uint8_t*)&meshCommand.PlanetData->rayScaleHeight, 4, 20);
				sRendererData->AtmosphereBuffer.Write((uint8_t*)&meshCommand.PlanetData->mieScaleHeight, 4, 24);
				sRendererData->AtmosphereBuffer.Write((uint8_t*)&meshCommand.PlanetData->rayBaseScatteringCoefficient, 12, 32);
				sRendererData->AtmosphereBuffer.Write((uint8_t*)&meshCommand.PlanetData->mieBaseScatteringCoefficient, 4, 44);
				sRendererData->AtmosphereBuffer.Write((uint8_t*)&meshCommand.PlanetData->planetCenter, 16, 48);
				sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphereToggle, 4, 60);
				sRendererData->AtmosphereBuffer.Write((uint8_t*)&meshCommand.PlanetData->inScatteringPoints, 4, 64);
				sRendererData->AtmosphereBuffer.Write((uint8_t*)&meshCommand.PlanetData->opticalDepthPoints, 4, 68);
				sRendererData->AtmosphereBuffer.Write((uint8_t*)&sunDiscToggle, 4, 72);
				sRendererData->AtmosphereBuffer.Write((uint8_t*)&meshCommand.PlanetData->SunDiscRadius, 4, 76);
				sRendererData->AtmosphereBuffer.Write((uint8_t*)&meshCommand.PlanetData->SunGlowIntensity, 4, 80);
				sRendererData->AtmosphereBuffer.Write((uint8_t*)&meshCommand.PlanetData->SunEdgeSoftness, 4, 84);
				sRendererData->AtmosphereBuffer.Write((uint8_t*)&meshCommand.PlanetData->SunGlowSize, 4, 88);
				sRendererData->AtmosphereBuffer.Write((uint8_t*)&useDepth, 4, 92);

				sRendererData->AtmosphereCBuffer->Map(sRendererData->AtmosphereBuffer);
			}
		}	

		ShaderLibrary::Get("assets/shaders/Post Process/Atmosphere.hlsl")->Bind();

		DrawFullscreenQuad();

		static int currentFace = 0;// Tracks which face of the cube to render

		if (sRendererData->SceneData.SceneEnvironment.RadianceMap && dynamicIBL)
		{
			const DirectX::XMMATRIX& viewMatrix = sRendererData->AtmosphericScatteringViewMatrices[currentFace];
			const DirectX::XMMATRIX& invViewMatrix = sRendererData->AtmosphericScatteringInvViewMatrices[currentFace];
			DirectX::XMFLOAT4 cameraPos = { 0.0f, 0.0f, 0.0f, 0.0f };

			sRendererData->CameraBuffer.Write((uint8_t*)&viewMatrix, sizeof(viewMatrix), 0);
			sRendererData->CameraBuffer.Write((uint8_t*)&invViewMatrix, sizeof(invViewMatrix), 128);
			sRendererData->CameraBuffer.Write((uint8_t*)&cameraPos, sizeof(cameraPos), 256);
			sRendererData->CameraCBuffer->Map(sRendererData->CameraBuffer);

			useDepth = 0;

			sRendererData->AtmosphereBuffer.Write((uint8_t*)&useDepth, 4, 92);
			sRendererData->AtmosphereCBuffer->Map(sRendererData->AtmosphereBuffer);

			RenderCommand::SetRenderTargets({ sRendererData->AtmosphereCubeRT->GetRTVFace(currentFace).Get() }, nullptr);

			DrawFullscreenQuad();

			RenderCommand::SetRenderTargets({ nullptr }, nullptr);
			RenderCommand::ClearShaderResources();

			GeneratePrefilteredEnvMap(currentFace);

			GenerateIrradianceCubemap(currentFace);
		}
		else 
		{
			RenderCommand::ClearRenderTargets({ sRendererData->AtmosphereCubeRT->GetRTVFace(currentFace).Get() }, { 0.0f, 0.0f, 0.0f, 1.0f });

			GeneratePrefilteredEnvMap(currentFace);

			GenerateIrradianceCubemap(currentFace);
		}

		currentFace = (currentFace + 1) % 6;

		ID3D11RenderTargetView* nullRTV = nullptr;
		RenderCommand::SetRenderTargets({ nullRTV }, nullptr);
		RenderCommand::SetDepthStencilState(nullptr);
		RenderCommand::SetBlendState(nullptr);

		RenderCommand::ClearShaderResources();

#ifdef TOAST_DEBUG
		if (annotation)
			annotation->EndEvent();
#endif
	}

	void Renderer::ParticlesPass(Camera& camera, const DirectX::XMFLOAT4 cameraPos)
	{
		TOAST_PROFILE_FUNCTION();

#ifdef TOAST_DEBUG
		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation)
			annotation->BeginEvent(L"Particle Pass");
#endif

		// Camera needs rebinding at this stage
		sRendererData->CameraBuffer.Write((uint8_t*)&camera.GetViewMatrix(), 64, 0);
		sRendererData->CameraBuffer.Write((uint8_t*)&camera.GetProjection(), 64, 64);
		sRendererData->CameraBuffer.Write((uint8_t*)&camera.GetInvViewMatrix(), 64, 128);
		sRendererData->CameraBuffer.Write((uint8_t*)&camera.GetInvProjection(), 64, 192);
		sRendererData->CameraBuffer.Write((uint8_t*)&cameraPos.x, 16, 256);
		sRendererData->CameraBuffer.Write((uint8_t*)&camera.GetFarClip(), 4, 272);
		sRendererData->CameraBuffer.Write((uint8_t*)&camera.GetNearClip(), 4, 276);
		sRendererData->CameraBuffer.Write((uint8_t*)&sRendererData->Viewport.Width, 4, 280);
		sRendererData->CameraBuffer.Write((uint8_t*)&sRendererData->Viewport.Height, 4, 284);
		sRendererData->CameraCBuffer->Map(sRendererData->CameraBuffer);

		RenderCommand::SetViewport(sRendererData->Viewport);
		RenderCommand::SetRasterizerState(sRendererData->NormalRasterizerState);
		RenderCommand::SetRenderTargets({ sRendererData->AtmospherePassRT->GetRTV().Get() }, sRendererData->DepthStencilView);
		RenderCommand::SetDepthStencilState(sRendererData->ParticleDepthStencilState);
		RenderCommand::SetBlendState(sRendererData->ParticleBlendState, { 0.0f, 0.0f, 0.0f, 0.0f });
		RenderCommand::SetShaderResource(D3D11_VERTEX_SHADER, 0, sRendererData->ParticlesSRV);
		if(sRendererData->ParticleMaskTexture)
			RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 0, sRendererData->ParticleMaskTexture->GetSRV());

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		deviceContext->IASetIndexBuffer(sRendererData->ParticleIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

		ShaderLibrary::Get("assets/shaders/Rendering/Particles.hlsl")->Bind();

		RenderCommand::DrawIndexedInstanced(6, sRendererData->NrOfParticlesToRender, 0, 0, 0);

		ID3D11RenderTargetView* nullRTV = nullptr;
		RenderCommand::SetRenderTargets({ nullRTV }, nullptr);
		RenderCommand::SetDepthStencilState(nullptr);
		RenderCommand::SetBlendState(nullptr);
		RenderCommand::ClearShaderResources();

#ifdef TOAST_DEBUG
		if (annotation)
			annotation->EndEvent();
#endif
	}

	void Renderer::PostProcessPass()
	{
		TOAST_PROFILE_FUNCTION();
#ifdef TOAST_DEBUG
		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation)
			annotation->BeginEvent(L"Tonemapping Pass");
#endif

		RenderCommand::SetDepthStencilState(sRendererData->DepthDisabledStencilState);

		//Tonemapping
		RenderCommand::SetRenderTargets({ sRendererData->FinalRT->GetRTV().Get() }, nullptr);
		RenderCommand::ClearRenderTargets(sRendererData->FinalRT->GetRTV().Get(), {0.0f, 0.0f, 0.0f, 1.0f});
		RenderCommand::SetBlendState(sRendererData->LPassBlendState, { 0.0f, 0.0f, 0.0f, 0.0f });

		TextureLibrary::GetSampler("Default")->Bind(0, D3D11_PIXEL_SHADER);

		ShaderLibrary::Get("assets/shaders/Post Process/ToneMapping.hlsl")->Bind();

		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 10, sRendererData->AtmospherePassRT->GetSRV());

		DrawFullscreenQuad();
#ifdef TOAST_DEBUG
		if (annotation)
			annotation->EndEvent();
#endif
	}

	void Renderer::ResetStats()
	{
		memset(&sData.Stats, 0, sizeof(Statistics));
	}

	Renderer::Statistics Renderer::GetStats()
	{
		return sData.Stats;
	}

	void Renderer::GeneratePrefilteredEnvMap(int faceIndex)
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		ShaderLibrary::Get("assets/shaders/Environment/EnvironmentMipFilter.hlsl")->Bind();

		// Bind the atmospheric scattering cube map as input (unfiltered environment map)
		RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 14, sRendererData->AtmosphereCubeRT->GetSRV());

		// Bind the sampler
		TextureLibrary::GetSampler("Default")->Bind(0, D3D11_COMPUTE_SHADER);

		// Calculate source and destination sub resource indices
		const uint32_t srcMipLevels = sRendererData->AtmosphereCubeRT->GetTextureOriginal()->GetMipLevelCount(); // 1
		const uint32_t srcSubresourceIndex = D3D11CalcSubresource(0, faceIndex, srcMipLevels); // faceIndex

		const uint32_t destMipLevels = sRendererData->EnvMapFiltered->GetMipLevelCount(); // 9
		const uint32_t destSubresourceIndex = D3D11CalcSubresource(0, faceIndex, destMipLevels); // faceIndex * 9
		
		const uint32_t subresourceIndex = D3D11CalcSubresource(0, faceIndex, srcMipLevels);
		// Perform the copy operation from AtmosphereCubeRT to EnvMapFiltered
		deviceContext->CopySubresourceRegion(
			sRendererData->EnvMapFiltered->GetResource(), destSubresourceIndex, // Destination sub resource
			0, 0, 0, // Destination X, Y, Z
			sRendererData->AtmosphereCubeRT->GetTextureOriginal()->GetResource(), // Source resource
			srcSubresourceIndex, // Source sub resource index
			nullptr // Source box
		);

		// Pre-filter the rest of the mip chain for the current face
		const float deltaRoughness = 1.0f / std::max(float(sRendererData->EnvMapFiltered->GetMipLevelCount() - 1.0f), 1.0f);
		const uint32_t cubemapSize = sRendererData->EnvMapFiltered->GetWidth();
		int size = cubemapSize / 2;

		for (int mipLevel = 1; mipLevel < sRendererData->EnvMapFiltered->GetMipLevelCount(); ++mipLevel, size /= 2)
		{
			uint32_t size = sRendererData->EnvMapFiltered->GetWidth() / (1 << mipLevel);
			int numGroups = (std::max)(1, static_cast<int>(size / 8));

			sRendererData->EnvMapFiltered->CreateUAVUpdated(mipLevel, faceIndex);

			const float roughness = { mipLevel * deltaRoughness };
			sRendererData->SpecularMapFilterSettingsBuffer.Write((uint8_t*)&roughness, sizeof(float), 0);
			sRendererData->SpecularMapFilterSettingsBuffer.Write((uint8_t*)&faceIndex, 4, 4);
			sRendererData->SpecularMapFilterSettingsCBuffer->Map(sRendererData->SpecularMapFilterSettingsBuffer);

			// Bind the filtered environment map for writing (current face and mip level)
			sRendererData->EnvMapFiltered->BindForReadWriteUpdated(0, D3D11_COMPUTE_SHADER, mipLevel, faceIndex);

			// Dispatch compute shader for the current face and mip level
			RenderCommand::DispatchCompute(numGroups, numGroups, 1); // Process one face at a time

			// Unbind UAV for this mip level
			sRendererData->EnvMapFiltered->UnbindUAV(0, D3D11_COMPUTE_SHADER);
		}

		// Unbind resources
		RenderCommand::ClearShaderResources();
	}

	void Renderer::GenerateIrradianceCubemap(int faceIndex)
	{
		ShaderLibrary::Get("assets/shaders/Environment/EnvironmentIrradiance.hlsl")->Bind();

		RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 15, sRendererData->EnvMapFiltered->GetSRV());

		sRendererData->IrradianceCubeMap->CreateUAVUpdated(0, faceIndex);

		sRendererData->IrradianceCubeMap->BindForReadWriteUpdated(0, D3D11_COMPUTE_SHADER, 0, faceIndex);

		// Determine dispatch dimensions
		uint32_t textureDepth;
		uint32_t textureWidth = sRendererData->IrradianceCubeMap->GetWidth(); 
		uint32_t textureHeight = sRendererData->IrradianceCubeMap->GetHeight();

		uint32_t dispatchX = (textureWidth + 31) / 32;
		uint32_t dispatchY = (textureHeight + 31) / 32;

		RenderCommand::DispatchCompute(dispatchX, dispatchY, 1); 

		RenderCommand::ClearShaderResources();

		sRendererData->IrradianceCubeMap->UnbindUAVUpdated(0, D3D11_COMPUTE_SHADER);
	}

	void Renderer::GenerateParticleBuffers()
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		bufferDesc.ByteWidth = sizeof(Particle) * 1000;
		bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bufferDesc.StructureByteStride = sizeof(Particle);

		device->CreateBuffer(&bufferDesc, nullptr, &sRendererData->ParticleBuffer);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN; // Structured buffers don’t have a format
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.NumElements = 1000;

		device->CreateShaderResourceView(sRendererData->ParticleBuffer.Get(), &srvDesc, &sRendererData->ParticlesSRV);

		uint16_t indices[] = { 0, 1, 2, 2, 1, 3 };

		D3D11_BUFFER_DESC indexBufferDesc = {};
		indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		indexBufferDesc.ByteWidth = sizeof(indices);
		indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

		D3D11_SUBRESOURCE_DATA indexData = { indices, 0, 0 };
		device->CreateBuffer(&indexBufferDesc, &indexData, &sRendererData->ParticleIndexBuffer);
	}

	void Renderer::InvalidateParticleBuffers(size_t nrOfParticles, size_t maxNrOfParticles)
	{
		size_t newSize;
		if (maxNrOfParticles != sRendererData->NrOfParticlesToRender && maxNrOfParticles > 0 && nrOfParticles <= maxNrOfParticles)
			newSize = maxNrOfParticles;
		else if (nrOfParticles > maxNrOfParticles)
			newSize = nrOfParticles + maxNrOfParticles;
		else
			return;

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		bufferDesc.ByteWidth = sizeof(Particle) * newSize;
		bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bufferDesc.StructureByteStride = sizeof(Particle);

		device->CreateBuffer(&bufferDesc, nullptr, &sRendererData->ParticleBuffer);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN; // Structured buffers don’t have a format
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.NumElements = newSize;

		device->CreateShaderResourceView(sRendererData->ParticleBuffer.Get(), &srvDesc, &sRendererData->ParticlesSRV);
	}

	void Renderer::FillParticleBuffer(std::vector<Particle>& particles)
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		D3D11_MAPPED_SUBRESOURCE mappedResource;
		deviceContext->Map(sRendererData->ParticleBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		Particle* instances = reinterpret_cast<Particle*>(mappedResource.pData);

		for (size_t i = 0; i < particles.size(); ++i)
			instances[i] = particles[i];

		deviceContext->Unmap(sRendererData->ParticleBuffer.Get(), 0);

		sRendererData->NrOfParticlesToRender = particles.size();
	}

}