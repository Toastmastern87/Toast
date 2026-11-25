#include "tpch.h"
#include "Renderer.h"

#include "Toast/Assets/AssetManager.h"

#include "Toast/Debug/FrameProfiler.h"

#include "Toast/Renderer/AtmosphereCommon.h"
#include "Toast/Renderer/Renderer2D.h"
#include "Toast/Renderer/RendererDebug.h"
#include "Toast/Renderer/PlanetSystem.h"
#include "Toast/Renderer/SamplerStates.h"
#include "Toast/Renderer/TerrainObjectSystem.h"

namespace Toast {

	Scope<Renderer::RendererData> Renderer::sRendererData = CreateScope<Renderer::RendererData>();

	void Renderer::Init(uint32_t width, uint32_t height)
	{
		TOAST_PROFILE_FUNCTION();

		RenderCommand::Init();
		Renderer2D::Init();
		RendererDebug::Init(width, height);

		SamplerStates::Init();

		sRendererData->Particles = CreateRef<ParticleSystem>();
		if (!sRendererData->Particles->Init())
			TOAST_CORE_ASSERT(false, "Failed to initialize the GPU particle system!");

		sRendererData->TerrainObjects = CreateRef<TerrainObjectSystem>();
		if (!sRendererData->TerrainObjects->Init())
			TOAST_CORE_ASSERT(false, "Failed to initialize the terrain object scatter system!");

#if TOAST_PROFILE_ENABLED
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		sRendererData->FrameProfiler = CreateScope<FrameProfiler>();
		sRendererData->FrameProfiler->Init(API->GetDevice(), API->GetDeviceContext());
#endif

		sRendererData->EditorViewport.TopLeftX = 0.0f;
		sRendererData->EditorViewport.TopLeftY = 0.0f;
		sRendererData->EditorViewport.Width = static_cast<float>(width);
		sRendererData->EditorViewport.Height = static_cast<float>(height);
		sRendererData->EditorViewport.MinDepth = 0.0f;
		sRendererData->EditorViewport.MaxDepth = 1.0f;

		// Setting viewport for shadow mapping
		sRendererData->ShadowMapViewport.TopLeftX = 0.0f;
		sRendererData->ShadowMapViewport.TopLeftY = 0.0f;
		sRendererData->ShadowMapViewport.Width = 4096.0f;
		sRendererData->ShadowMapViewport.Height = 4096.0f;
		sRendererData->ShadowMapViewport.MinDepth = 0.0f;
		sRendererData->ShadowMapViewport.MaxDepth = 1.0f;

		// Setting up the constant buffer and data buffer for the camera rendering
		sRendererData->CameraCBuffer = ConstantBufferLibrary::Load("Camera", 352, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_VERTEX_SHADER, CBufferBindSlot::Camera), CBufferBindInfo(D3D11_PIXEL_SHADER, CBufferBindSlot::Camera), CBufferBindInfo(D3D11_COMPUTE_SHADER, CBufferBindSlot::Camera) });
		sRendererData->CameraCBuffer->Bind();
		sRendererData->CameraBuffer.Allocate(sRendererData->CameraCBuffer->GetSize());
		sRendererData->CameraBuffer.ZeroInitialize();

		// Setting up the constant buffer and data buffer for the Model
		sRendererData->ModelCBuffer = ConstantBufferLibrary::Load("Model", 80, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_VERTEX_SHADER, CBufferBindSlot::Model), CBufferBindInfo(D3D11_PIXEL_SHADER, CBufferBindSlot::Model) });
		sRendererData->ModelCBuffer->Bind();
		sRendererData->ModelBuffer.Allocate(sRendererData->ModelCBuffer->GetSize());
		sRendererData->ModelBuffer.ZeroInitialize();

		// Setting up the constant buffer and data buffer for the PBR Material
		sRendererData->MaterialCBuffer = ConstantBufferLibrary::Load("Material", 48, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_PIXEL_SHADER, CBufferBindSlot::Material) });
		sRendererData->MaterialCBuffer->Bind();
		sRendererData->MaterialBuffer.Allocate(sRendererData->MaterialCBuffer->GetSize());
		sRendererData->MaterialBuffer.ZeroInitialize();

		// Setting up the constant buffer and data buffer for lightning rendering
		sRendererData->LightningCBuffer = ConstantBufferLibrary::Load("DirectionalLight", 336, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_VERTEX_SHADER, CBufferBindSlot::DirectionalLight), CBufferBindInfo(D3D11_PIXEL_SHADER, CBufferBindSlot::DirectionalLight), CBufferBindInfo(D3D11_COMPUTE_SHADER, CBufferBindSlot::DirectionalLight) });
		sRendererData->LightningCBuffer->Bind();
		sRendererData->LightningBuffer.Allocate(sRendererData->LightningCBuffer->GetSize());
		sRendererData->LightningBuffer.ZeroInitialize();

		// Setting up the constant buffer and data buffer for environmental rendering
		sRendererData->SunDiscSettingsCBuffer = ConstantBufferLibrary::Load("SunDiscSettings", sizeof(SunDiscCBufferGPU), std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_PIXEL_SHADER, CBufferBindSlot::SunDiscSettings), CBufferBindInfo(D3D11_COMPUTE_SHADER, CBufferBindSlot::SunDiscSettings)  });
		sRendererData->SunDiscSettingsCBuffer->Bind();
		sRendererData->SunDiscSettingsBuffer.Allocate(sRendererData->SunDiscSettingsCBuffer->GetSize());
		sRendererData->SunDiscSettingsBuffer.ZeroInitialize();

		// Setting up the constant buffer and data buffer for the render settings
		sRendererData->RenderSettingsCBuffer = ConstantBufferLibrary::Load("RenderSettings", 16, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_PIXEL_SHADER, CBufferBindSlot::RenderSettings) });
		sRendererData->RenderSettingsCBuffer->Bind();
		sRendererData->RenderSettingsBuffer.Allocate(sRendererData->RenderSettingsCBuffer->GetSize());
		sRendererData->RenderSettingsBuffer.ZeroInitialize();

		// Setting up the constant buffer for stars rendering
		sRendererData->StarsCBuffer = ConstantBufferLibrary::Load("StarsParams", 32, std::vector<CBufferBindInfo>{  CBufferBindInfo(D3D11_PIXEL_SHADER, (CBufferBindSlot)7) });
		sRendererData->StarsCBuffer->Bind();
		sRendererData->StarsBuffer.Allocate(sRendererData->StarsCBuffer->GetSize());
		sRendererData->StarsBuffer.ZeroInitialize();

		// Setting up the constant buffer for atmosphere rendering
		sRendererData->AtmosphereCBuffer = ConstantBufferLibrary::Load("Atmosphere", sizeof(AtmosphereCBufferGPU), std::vector<CBufferBindInfo>{  CBufferBindInfo(D3D11_PIXEL_SHADER, (CBufferBindSlot)5), CBufferBindInfo(D3D11_COMPUTE_SHADER, (CBufferBindSlot)5) });
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

		// Setting up the constant buffer for hover tint
		sRendererData->HoverTintCBuffer = CreateRef<ConstantBuffer>("Hovering", 16, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_PIXEL_SHADER, (CBufferBindSlot)8) });
		sRendererData->HoverTintCBuffer->Bind();
		sRendererData->HoverTintBuffer.Allocate(sRendererData->HoverTintCBuffer->GetSize());
		sRendererData->HoverTintBuffer.ZeroInitialize();

		// Setting up the constant buffer for bloom rendering
		sRendererData->BloomMipCBuffer = ConstantBufferLibrary::Load("BloomMipParams", 16, std::vector<CBufferBindInfo>{  CBufferBindInfo(D3D11_PIXEL_SHADER, CBufferBindSlot::Bloom) });
		sRendererData->BloomMipBuffer.Allocate(sRendererData->BloomMipCBuffer->GetSize());
		sRendererData->BloomMipBuffer.ZeroInitialize();
		sRendererData->BloomBlendCBuffer = ConstantBufferLibrary::Load("BloomBlendParams", 16, std::vector<CBufferBindInfo>{  CBufferBindInfo(D3D11_PIXEL_SHADER, CBufferBindSlot::Bloom) });
		sRendererData->BloomBlendBuffer.Allocate(sRendererData->BloomBlendCBuffer->GetSize());
		sRendererData->BloomBlendBuffer.ZeroInitialize();

		// Auto-exposure: luminance histogram + its parameters
		sRendererData->ExposureEVBuffer = CreateRef<StructuredBuffer>((uint32_t)sizeof(float), 2u, D3D11_USAGE_DEFAULT, true, false);
		sRendererData->LocalExposureGridRaw = CreateRef<Texture3D>(DXGI_FORMAT_R32G32_FLOAT, DXGI_FORMAT_R32G32_FLOAT, 64, 32, 32);
		sRendererData->LocalExposureGrid = CreateRef<Texture3D>(DXGI_FORMAT_R32G32_FLOAT, DXGI_FORMAT_R32G32_FLOAT, 64, 32, 32);
		sRendererData->AutoExposureCBuffer = ConstantBufferLibrary::Load("AutoExposure", 64, std::vector<CBufferBindInfo>{  CBufferBindInfo(D3D11_COMPUTE_SHADER, (CBufferBindSlot)8)});
		sRendererData->AutoExposureBuffer.Allocate(sRendererData->AutoExposureCBuffer->GetSize());
		sRendererData->AutoExposureBuffer.ZeroInitialize();

		// Sun Visibility
		sRendererData->SunVisibilityBuffer = CreateRef<StructuredBuffer>((uint32_t)sizeof(DirectX::XMFLOAT4), 2u, D3D11_USAGE_DEFAULT, true, false);

		for (auto& vis : sRendererData->APVisibility)
			vis = CreateRef<Texture3D>(DXGI_FORMAT_R16_FLOAT, DXGI_FORMAT_R16_FLOAT, 192, 108, 128);
		sRendererData->APTemporalCBuffer = ConstantBufferLibrary::Load("APTemporal", 96, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_COMPUTE_SHADER, (CBufferBindSlot)7) });
		sRendererData->APTemporalBuffer.Allocate(sRendererData->APTemporalCBuffer->GetSize());
		sRendererData->APTemporalBuffer.ZeroInitialize();
		sRendererData->APSkyShadow = CreateRef<Texture2D>(DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT, 192, 108, D3D11_USAGE_DEFAULT, (D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS), 1, 0);
		sRendererData->ShadowHeightMap = CreateRef<Texture2D>(DXGI_FORMAT_R32G32_FLOAT, DXGI_FORMAT_R32G32_FLOAT, 512, 512, D3D11_USAGE_DEFAULT, (D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS), 1, 0);

		// Setting up the constant buffer for Tonemapping
		sRendererData->TonemappingCBuffer = ConstantBufferLibrary::Load("Tonemapping", 32, std::vector<CBufferBindInfo>{  CBufferBindInfo(D3D11_PIXEL_SHADER, (CBufferBindSlot)10) });
		sRendererData->TonemappingCBuffer->Bind();
		sRendererData->TonemappingBuffer.Allocate(sRendererData->TonemappingCBuffer->GetSize());
		sRendererData->TonemappingBuffer.ZeroInitialize();

		// Setting up the constant buffer and data buffer for the lightning pass settings
		sRendererData->LightningPassCBuffer = ConstantBufferLibrary::Load("LightningPassSettings", 16, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_PIXEL_SHADER, (CBufferBindSlot)8) });
		sRendererData->LightningPassCBuffer->Bind();
		sRendererData->LightningPassBuffer.Allocate(sRendererData->LightningPassCBuffer->GetSize());
		sRendererData->LightningPassBuffer.ZeroInitialize();

		// Setting up the constant buffer for outline selection
		sRendererData->OutlineCBuffer = ConstantBufferLibrary::Load("Outline", 32, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_PIXEL_SHADER, (CBufferBindSlot)8) });
		sRendererData->OutlineBuffer.Allocate(sRendererData->OutlineCBuffer->GetSize());
		sRendererData->OutlineBuffer.ZeroInitialize();

		// Setting up the constant buffer for Markers
		sRendererData->MarkerCBuffer = ConstantBufferLibrary::Load("MarkerCBuffer", 16, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_PIXEL_SHADER, (CBufferBindSlot)8) });
		sRendererData->MarkerBuffer.Allocate(sRendererData->MarkerCBuffer->GetSize());
		sRendererData->MarkerBuffer.ZeroInitialize();

		// Setting up the render targets for the Geometry Pass
		sRendererData->GPassPositionRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R32G32B32A32_FLOAT);
		sRendererData->GPassNormalRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R16G16B16A16_FLOAT);
		sRendererData->GPassAlbedoMetallicRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R8G8B8A8_UNORM_SRGB);
		sRendererData->GPassRoughnessAORT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R8G8B8A8_UNORM);
		sRendererData->GPassPickingRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R32_SINT);

		// Setting up the render target for Planet Material debug
		sRendererData->PlanetMaterialDebugRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R8G8B8A8_UNORM);

		// Setting up the render target for SSAO Pass
		sRendererData->SSAORT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R8G8B8A8_UNORM);
		sRendererData->SSAOBlurRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R8G8B8A8_UNORM);

		// Setting up the render target for the Lightning Pass
		sRendererData->LPassRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R16G16B16A16_FLOAT);

		// Setting up the render target for Bloom Pass
		sRendererData->FinalBloomRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R16G16B16A16_FLOAT);
		for(uint32_t i = 0; i < RendererData::BloomMipCount; ++i)
			sRendererData->BloomMips[i] = CreateRef<RenderTarget>(RenderTargetType::Color, std::max(1u, width >> (i + 1)), std::max(1u, height >> (i + 1)), 1, TextureFormat::R16G16B16A16_FLOAT);

		// Setting up the render targets for the Post Process pass
		sRendererData->FinalRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R16G16B16A16_FLOAT, false, true);
		sRendererData->FinalEditorRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R8G8B8A8_UNORM_SRGB, false, true);

		// Setting up the render target for the back buffer
		sRendererData->BackbufferRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R16G16B16A16_FLOAT, true);

		// Setting up the render target for the Atmosphere Pass
		sRendererData->AtmospherePassRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R16G16B16A16_FLOAT);
		sRendererData->StarsRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R16G16B16A16_FLOAT);
		sRendererData->AtmosphereCubeRT = CreateRef<RenderTarget>(RenderTargetType::ColorCube, 256, 256, 1, TextureFormat::R16G16B16A16_FLOAT);
		sRendererData->SunDiscMaskRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R8_UNORM);
		sRendererData->SunHaloMaskRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R8_UNORM);
		sRendererData->Dummy1RT = CreateRef<RenderTarget>(RenderTargetType::ColorCube, 256, 256, 1, TextureFormat::R8G8B8A8_UNORM);
		sRendererData->Dummy2RT = CreateRef<RenderTarget>(RenderTargetType::ColorCube, 256, 256, 1, TextureFormat::R8G8B8A8_UNORM);

		// Setting up the Render Target for the Selection System
		sRendererData->SelectedMeshMaskRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R8G8B8A8_UNORM);

		// Setting up the Render Target for the Hover Tint
		sRendererData->HoveredMeshMaskRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R8G8B8A8_UNORM);

		// Setting -Y led to the black since nothing should reflect. 
		// TODO this should most likely be dynamic in the future depending on which color the surface is. It is gray during the night but orange during the day.
		RenderCommand::ClearRenderTargets({ sRendererData->AtmosphereCubeRT->GetRTVFace(0).Get(), sRendererData->Dummy1RT->GetRTVFace(0).Get(), sRendererData->Dummy2RT->GetRTVFace(0).Get() }, { 0.0f, 0.0f, 0.0f, 0.0f });
		RenderCommand::ClearRenderTargets({ sRendererData->AtmosphereCubeRT->GetRTVFace(1).Get(), sRendererData->Dummy1RT->GetRTVFace(1).Get(), sRendererData->Dummy2RT->GetRTVFace(1).Get() }, { 0.0f, 0.0f, 0.0f, 0.0f });
		RenderCommand::ClearRenderTargets({ sRendererData->AtmosphereCubeRT->GetRTVFace(2).Get(), sRendererData->Dummy1RT->GetRTVFace(2).Get(), sRendererData->Dummy2RT->GetRTVFace(2).Get() }, { 0.0f, 0.0f, 0.0f, 0.0f });
		RenderCommand::ClearRenderTargets({ sRendererData->AtmosphereCubeRT->GetRTVFace(3).Get(), sRendererData->Dummy1RT->GetRTVFace(3).Get(), sRendererData->Dummy2RT->GetRTVFace(3).Get() }, { 0.0f, 0.0f, 0.0f, 0.0f });
		RenderCommand::ClearRenderTargets({ sRendererData->AtmosphereCubeRT->GetRTVFace(4).Get(), sRendererData->Dummy1RT->GetRTVFace(4).Get(), sRendererData->Dummy2RT->GetRTVFace(4).Get() }, { 0.0f, 0.0f, 0.0f, 0.0f });
		RenderCommand::ClearRenderTargets({ sRendererData->AtmosphereCubeRT->GetRTVFace(5).Get(), sRendererData->Dummy1RT->GetRTVFace(5).Get(), sRendererData->Dummy2RT->GetRTVFace(5).Get() }, { 0.0f, 0.0f, 0.0f, 0.0f });

		// Setting up dynamic environmental maps 
		sRendererData->EnvMapSource = CreateRef<TextureCube>("EnvMapSource", 256, 256, 9, true);
		sRendererData->EnvMapFiltered = CreateRef<TextureCube>("EnvMapFiltered", 256, 256, 9);
		sRendererData->IrradianceCubeMap = CreateRef<TextureCube>("IrradianceCubemap", 32, 32, 1);

		auto& atmospherCubeViewport = sRendererData->AtmosphereCubeViewport;
		auto atmospherCubeSize = sRendererData->AtmosphereCubeRT->GetSize();
		atmospherCubeViewport.TopLeftX = atmospherCubeViewport.TopLeftY = 0.0f;
		atmospherCubeViewport.Width = (float)std::get<0>(atmospherCubeSize);
		atmospherCubeViewport.Height = (float)std::get<1>(atmospherCubeSize);
		atmospherCubeViewport.MinDepth = 0.0f;
		atmospherCubeViewport.MaxDepth = 1.0f;

		CreateRasterizerStates();
		CreateDepthBuffer(width, height);
		CreateDepthStencilView();
		CreateDepthStencilStates();

		CreateBlendStates();

		SetUpAtmosphericScatteringMatrices();

		GenerateSampleKernel();
		GenerateNoiseTexture();

		CreateParticleIndexBuffer();
	}

	void Renderer::Shutdown()
	{
		SamplerStates::Shutdown();

		Renderer2D::Shutdown();
		RendererDebug::Shutdown();

#if TOAST_PROFILE_ENABLED
		sRendererData->FrameProfiler->Shutdown();
#endif
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

		sRendererData->PlanetMaterialDebugRT->Resize(width, height);

		sRendererData->SSAORT->Resize(width, height);
		sRendererData->SSAOBlurRT->Resize(width, height);

		sRendererData->FinalBloomRT->Resize(width, height);
		for (uint32_t i = 0; i < RendererData::BloomMipCount; ++i)
			sRendererData->BloomMips[i]->Resize(std::max(1u, width >> (i + 1)), std::max(1u, height >> (i + 1)));


		sRendererData->LPassRT->Resize(width, height);

		sRendererData->AtmospherePassRT->Resize(width, height);
		sRendererData->StarsRT->Resize(width, height);
		sRendererData->SunDiscMaskRT->Resize(width, height);
		sRendererData->SunHaloMaskRT->Resize(width, height);

		sRendererData->SelectedMeshMaskRT->Resize(width, height);
		sRendererData->HoveredMeshMaskRT->Resize(width, height);

		sRendererData->FinalRT->Resize(width, height);
		sRendererData->FinalEditorRT->Resize(width, height);

		sRendererData->DepthStencilView.Reset();
		sRendererData->ShadowPassDepthStencilView[0].Reset();

		CreateDepthBuffer(width, height);
		CreateDepthStencilView();

		RendererDebug::OnWindowResize(width, height);
	}

	void Renderer::BeginScene(const Scene* scene, Camera& camera, const DirectX::XMFLOAT4 cameraPos, Scene::Environment& environment, int wireFrame)
	{
		TOAST_PROFILE_FUNCTION();

#if TOAST_PROFILE_ENABLED
		sRendererData->FrameProfiler->BeginFrame();
		sRendererData->FrameProfiler->SetPresentMS(RenderCommand::sRendererAPI->GetLastPresentMS());

		const RendererStatistics& statistics = RenderCommand::sRendererAPI->GetStatistics();
		sRendererData->FrameProfiler->SetStatisticInfo(statistics.DrawCalls, statistics.ConstantBuffersMaps, (uint32_t)sRendererData->MeshDrawList.size());
#endif
		RenderCommand::sRendererAPI->ResetStatistics();

		ClearDrawList();

		sRendererData->Wireframe = wireFrame;

		// Updating the camera data in the buffer and mapping it to the GPU
		UploadCameraCBuffer(camera, cameraPos);

		DirectX::XMStoreFloat4x4(&sRendererData->APViewProj, DirectX::XMMatrixMultiply(DirectX::XMLoadFloat4x4(&camera.GetViewMatrix()), DirectX::XMLoadFloat4x4(&camera.GetProjection())));
		sRendererData->APCameraPos = cameraPos;

		DirectX::XMFLOAT4 cascadeEnds = {
				scene->mLightEnvironment.DirectionalLights[0].CascadeEnds[0],
				scene->mLightEnvironment.DirectionalLights[0].CascadeEnds[1],
				scene->mLightEnvironment.DirectionalLights[0].CascadeEnds[2],
				scene->mLightEnvironment.DirectionalLights[0].CascadeEnds[3],
		};

		// Updating the lightning data in the buffer and mapping it to the GPU
		sRendererData->LightningBuffer.Write((uint8_t*)&scene->mLightEnvironment.DirectionalLights[0].LightViewProj[0], 256, 0);
		sRendererData->LightningBuffer.Write((uint8_t*)&scene->mLightEnvironment.DirectionalLights[0].Direction, 16, 256);
		sRendererData->LightningBuffer.Write((uint8_t*)&scene->mLightEnvironment.DirectionalLights[0].Radiance, 16, 272);
		sRendererData->LightningBuffer.Write((uint8_t*)&environment.SunIntensity, 4, 288);
		sRendererData->LightningBuffer.Write((uint8_t*)&scene->mSettings.DirectionalLightningGain, 4, 292);
		sRendererData->LightningBuffer.Write((uint8_t*)&scene->mLightEnvironment.DirectionalLights[0].CascadeCount, 4, 296);
		sRendererData->LightningBuffer.Write((uint8_t*)&scene->mLightEnvironment.DirectionalLights[0].ShadowDistance, 4, 300);
		sRendererData->LightningBuffer.Write((uint8_t*)&cascadeEnds, sizeof(cascadeEnds), 304);
		sRendererData->LightningBuffer.Write((uint8_t*)&scene->mLightEnvironment.DirectionalLights[0].ConstantBias, 4, 324);
		sRendererData->LightningBuffer.Write((uint8_t*)&scene->mLightEnvironment.DirectionalLights[0].SlopeBias, 4, 328);
		sRendererData->LightningCBuffer->Map(sRendererData->LightningBuffer);

		sRendererData->SpecularBRDFLUT->Bind(2, D3D11_PIXEL_SHADER);

		RenderCommand::BindSampler(D3D11_PIXEL_SHADER, 0, SamplerStates::Get(SamplerType::Default));
		RenderCommand::BindSampler(D3D11_PIXEL_SHADER, 1, SamplerStates::Get(SamplerType::BRDF));

		// Updating the render settings data in the buffer and mapping it to the GPU
		float renderOverlay = (float)scene->mSettings.RenderOverlaySetting;
		sRendererData->RenderSettingsBuffer.Write((uint8_t*)&renderOverlay, 4, 0);
		sRendererData->RenderSettingsCBuffer->Map(sRendererData->RenderSettingsBuffer);
	}

	void Renderer::EndScene(Ref<Planet>& planet, Scene::Environment& environment, Scene::ExposureParams& exposureParams, Scene::BloomParams& bloomParams, const Scene::OutlineSettings& outlineSettings, const bool debugActivated, const bool shadows, const bool SSAO, Camera& camera, const DirectX::XMFLOAT4 cameraPos, float SSAORadius, float SSAObias, Scene::CascadedShadowMapParams& shadowParams, DirectX::XMFLOAT4& hoverTintColor, float dt, bool runtime)
	{
		sRendererData->ElapsedTime += (float)dt;

		RenderCommand::SetViewport(sRendererData->Viewport);

		{
			TOAST_PROFILE(*sRendererData->FrameProfiler, "Geometry Pass");
			GeometryPass(camera.GetWorldTranslation());
		}

		if (shadows)
		{
				TOAST_PROFILE(*sRendererData->FrameProfiler, "Shadow Pass");
				ShadowPass(shadowParams);
		}

		else
			RenderCommand::ClearDepthStencilView(sRendererData->ShadowPassDepthStencilView[0], 1.0f);

		if (SSAO)
		{
			TOAST_PROFILE(*sRendererData->FrameProfiler, "SSAO Pass");
			SSAOPass(SSAORadius, SSAObias);
		}
		else 
		{
			RenderCommand::ClearRenderTargets(sRendererData->SSAORT->GetRTV().Get(), { 1.0f, 1.0f, 1.0f, 1.0f });
			RenderCommand::ClearRenderTargets(sRendererData->SSAOBlurRT->GetRTV().Get(), { 1.0f, 1.0f, 1.0f, 1.0f });
		}

		{
			TOAST_PROFILE(*sRendererData->FrameProfiler, "Lightning Pass");
			LightningPass(planet, environment);
		}

		const bool hasPlanet = (sRendererData->PlanetDraw.Planet != nullptr);
		const bool atmoActive = hasPlanet && sRendererData->PlanetDraw.Planet->AtmosphereActivated();

		const bool hasSkyView = (planet->GetSkyViewLUT() != nullptr);       
		const bool hasAP3D = (planet->GetAerialPerspectiveLUT() != nullptr);

		// Post Processes
		if (hasPlanet && atmoActive && hasSkyView && hasAP3D) 
		{
			{
				TOAST_PROFILE(*sRendererData->FrameProfiler, "Starfield Pass");
				StarFieldPass(environment, planet, planet->GetAtmosphere().AtmosphereHeight);
			}

			{
				TOAST_PROFILE(*sRendererData->FrameProfiler, "Atmosphere Pass");
				AtmospherePass(planet, environment, cameraPos, camera.GetWorldTranslation());
			}

			UploadCameraCBuffer(camera, cameraPos);

			{
				TOAST_PROFILE(*sRendererData->FrameProfiler, "Sun Visibility");
				SunVisibilityPass(planet);
			}
		}
		else 
		{
			RenderCommand::ClearRenderTargets({ sRendererData->AtmospherePassRT->GetRTV().Get(), sRendererData->SunDiscMaskRT->GetRTV().Get(), sRendererData->SunHaloMaskRT->GetRTV().Get() }, { 0.0f, 0.0f, 0.0f, 0.0f });
		}

		// Particles only for now, but will most likely be renamed and handle more things in the future.
		// If there are no particles that needs to be rendered, this pass will be skipped.
		if (sRendererData->ParticleIndexBuffer.Get())
		{
			TOAST_PROFILE(*sRendererData->FrameProfiler, "Particle Pass");
			ParticlesPass();
		}		

		if (bloomParams.Enabled)
		{
			{
				TOAST_PROFILE(*sRendererData->FrameProfiler, "Bloom Mip Chain");
				BloomMipChainPass();
			}

			{
				TOAST_PROFILE(*sRendererData->FrameProfiler, "Bloom Up Sample");
				BloomUpSamplePass(bloomParams.Falloff);
			}

			{
				TOAST_PROFILE(*sRendererData->FrameProfiler, "Bloom Blend");
				BloomBlendPass(bloomParams.Intensity, bloomParams.Falloff);
			}
		}

		{
			TOAST_PROFILE(*sRendererData->FrameProfiler, "Exposure Adapt");
			const DirectX::XMFLOAT3 camPos3 = DirectX::XMFLOAT3(cameraPos.x, cameraPos.y, cameraPos.z);
			const float spaceFactor = planet ? planet->GetSpaceFactor(camPos3, camera.GetWorldTranslation()) : 0.0f;
			ExposureAdaptPass(dt, exposureParams, spaceFactor, planet);
		}

		{
			TOAST_PROFILE(*sRendererData->FrameProfiler, "Post Process Pass");
			PostProcessPass(bloomParams.Enabled, environment, exposureParams, planet, cameraPos, camera.GetWorldTranslation());
		}

		// Only run during runtime, otherwise the RendererDebug handles the outline
		if (runtime)
		{
			{
				TOAST_PROFILE(*sRendererData->FrameProfiler, "Guidance Pass");
				GuidancePass(camera.GetWorldTranslation()); // Here is where the move markers and in the future other player guidances are rendered, these are only rendered during play mode of the scene due to the fact that they are depending on play mode states.
			}

			{
				TOAST_PROFILE(*sRendererData->FrameProfiler, "Outline Pass");
				OutlinePass(outlineSettings, hoverTintColor);
			}
		}

		if (!debugActivated) 
		{
			RenderCommand::SetRenderTargets({ sRendererData->BackbufferRT->GetRTV().Get() }, nullptr);
			RenderCommand::ClearRenderTargets(sRendererData->BackbufferRT->GetRTV().Get() , { 0.0f, 0.0f, 0.0f, 1.0f });
		}

#if TOAST_PROFILE_ENABLED
		sRendererData->FrameProfiler->EndFrame();
#endif
	}

	void Renderer::CreateDepthBuffer(uint32_t width, uint32_t height)
	{
		sRendererData->DepthBuffer = CreateScope<Texture2D>((DXGI_FORMAT)TextureFormat::R32_TYPELESS, (DXGI_FORMAT)TextureFormat::R32_FLOAT, width, height, D3D11_USAGE_DEFAULT, (D3D11_BIND_FLAG)(D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE), 1);

		sRendererData->ShadowMapArray = CreateScope<Texture2DArray>(DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R32_FLOAT, 4096, 4096, MaxCascades, D3D11_USAGE_DEFAULT, (D3D11_BIND_FLAG)(D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE),	1,0);
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

		auto tex = sRendererData->ShadowMapArray->GetTexture().Get(); // you need an accessor

		for (uint32_t i = 0; i < MaxCascades; ++i)
		{
			D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
			dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
			dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
			dsvDesc.Texture2DArray.MipSlice = 0;
			dsvDesc.Texture2DArray.FirstArraySlice = i;
			dsvDesc.Texture2DArray.ArraySize = 1;

			HRESULT hr = device->CreateDepthStencilView(tex, &dsvDesc, sRendererData->ShadowPassDepthStencilView[i].GetAddressOf());
			TOAST_CORE_ASSERT(SUCCEEDED(hr), "Failed to create ShadowPassDSV slice!");
		}
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

		result = device->CreateDepthStencilState(&depthStencilDesc, &sRendererData->DepthStarFieldStencilState);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create Skybox pass depth stencil state");

		// --- Shadow map depth state (Normal-Z) ---
		{
			D3D11_DEPTH_STENCIL_DESC ds = {};
			ds.DepthEnable = TRUE;
			ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
			ds.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

			ds.StencilEnable = FALSE;
			ds.StencilReadMask = 0;
			ds.StencilWriteMask = 0;

			HRESULT hr2 = device->CreateDepthStencilState(&ds, &sRendererData->ShadowPassDepthStencilState);
			TOAST_CORE_ASSERT(SUCCEEDED(hr2), "Failed to create shadow depth stencil state");
		}
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

			const std::vector<Ref<RenderTarget>> renderTargets = { sRendererData->GPassPositionRT, sRendererData->GPassNormalRT, sRendererData->GPassAlbedoMetallicRT, sRendererData->GPassRoughnessAORT, sRendererData->GPassPickingRT, sRendererData->PlanetMaterialDebugRT };
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

		// Particle Pass Additive Blend State
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
			blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

			HRESULT hr = device->CreateBlendState(&blendDesc, &sRendererData->ParticleAdditiveBlendState);
			TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create Particle Pass blend state");
		}

		// Particle Pass Alpha Blend State
		{
			D3D11_BLEND_DESC blendDesc = {};
			blendDesc.AlphaToCoverageEnable = FALSE;
			blendDesc.IndependentBlendEnable = FALSE;

			blendDesc.RenderTarget[0].BlendEnable = TRUE;
			blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
			blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
			blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
			blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

			HRESULT hr = device->CreateBlendState(&blendDesc, &sRendererData->ParticleAlphaBlendState);
			TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create Particle Pass blend state");
		}

		// Particle Pass Alpha Blend State
		{
			D3D11_BLEND_DESC blendDesc = {};
			blendDesc.AlphaToCoverageEnable = FALSE;
			blendDesc.IndependentBlendEnable = FALSE;
			blendDesc.RenderTarget[0].BlendEnable = TRUE;
			blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_BLEND_FACTOR;
			blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_BLEND_FACTOR;
			blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
			blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

			HRESULT hr = device->CreateBlendState(&blendDesc, &sRendererData->BloomUpBlendState);
			TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create Bloom UP Pass blend state");
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

		// Tone Mapping Pass Blend State
		{
			D3D11_BLEND_DESC blendDesc = {};
			blendDesc.AlphaToCoverageEnable = FALSE;
			blendDesc.IndependentBlendEnable = TRUE;

			const D3D11_RENDER_TARGET_BLEND_DESC& rtBlendDesc = sRendererData->LPassRT->GetBlendDesc();
			blendDesc.RenderTarget[0] = rtBlendDesc;
			blendDesc.RenderTarget[1] = rtBlendDesc;

			result = device->CreateBlendState(&blendDesc, &sRendererData->PostProcessBlendState);
			TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create LPass blend state");
		}

		// UI Pass Blend State
		{
			D3D11_BLEND_DESC blendDesc = {};
			blendDesc.AlphaToCoverageEnable = FALSE;
			blendDesc.IndependentBlendEnable = TRUE;

			blendDesc.RenderTarget[0].BlendEnable = TRUE;
			blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
			blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
			blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
			blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

			blendDesc.RenderTarget[1].BlendEnable = TRUE;
			blendDesc.RenderTarget[1].SrcBlend = D3D11_BLEND_ONE;
			blendDesc.RenderTarget[1].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
			blendDesc.RenderTarget[1].BlendOp = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[1].SrcBlendAlpha = D3D11_BLEND_ONE;
			blendDesc.RenderTarget[1].DestBlendAlpha = D3D11_BLEND_ONE;
			blendDesc.RenderTarget[1].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[1].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

			blendDesc.RenderTarget[2].BlendEnable = FALSE; // Disable blending for slot 1
			blendDesc.RenderTarget[2].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

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
		rasterDesc.DepthClipEnable = false;
		rasterDesc.DepthBias = 2000;                    // start 500..5000
		rasterDesc.SlopeScaledDepthBias = 2.0f;         // start 1..4
		rasterDesc.DepthBiasClamp = 0.0f;               // often 0

		result = device->CreateRasterizerState(&rasterDesc, &sRendererData->ShadowMapRasterizerState);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create shadow pass rasterizer state");
	}

	float lerp(float a, float b, float f)
	{
		return a + f * (b - a);
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

		for (int i = 0; i < KERNEL_SIZE; i++)
		{
			float x = randomX(gen);
			float y = randomY(gen);
			float z = randomZ(gen);

			Vector3 sample = { x, y, z };
			sample = Vector3::Normalize(sample);

			float scale = (float)i / (float)KERNEL_SIZE;
			scale = lerp(0.1f, 1.0f, scale * scale);
			sample *= scale;

			sRendererData->SSAOKernel.emplace_back(DirectX::XMFLOAT4(sample.x, sample.y, sample.z, 0.0f));
		}
	}

	void Renderer::GenerateNoiseTexture()
	{
		const int NOISE_DIM = 16;
		std::vector<DirectX::XMFLOAT4> SSAONoise;
		SSAONoise.reserve(NOISE_DIM * NOISE_DIM);
		for (int i = 0; i < NOISE_DIM * NOISE_DIM; i++) {
			float x = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
			float y = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
			// y=0 so the noise vectors lie in the tangent plane
			SSAONoise.push_back({ x, y, 0.0f, 0.0f });
		}

		sRendererData->SSAONoiseCPU = SSAONoise;

		uint32_t width = NOISE_DIM;
		uint32_t height = NOISE_DIM;
		uint32_t rowPitch = static_cast<uint32_t>(width * sizeof(DirectX::XMFLOAT4));

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

	void Renderer::SubmitMesh(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, const int entityID, uint32_t submeshIndex, uint32_t lodIndex, bool wireframe, int noWorldTransform, bool atmosphere)
	{
		sRendererData->PlanetData.Atmosphere = atmosphere;
;		sRendererData->MeshDrawList.emplace_back(mesh, transform, wireframe, noWorldTransform, entityID, submeshIndex, lodIndex);
	}

	void Renderer::SubmitSelecetedMesh(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, bool wireframe, uint32_t submeshIndex, uint32_t lodIndex, bool runtime)
	{
		bool noWorldTransform = false;
		int entityID = 0;
		if (runtime)
			sRendererData->MeshSelectedDrawList.emplace_back(mesh, transform, wireframe, noWorldTransform, entityID, submeshIndex, lodIndex);
		else
			sRendererData->MeshEditorSelectedDrawList.emplace_back(mesh, transform, wireframe, noWorldTransform, entityID, submeshIndex, lodIndex);
	}

	void Renderer::SubmitPlanet(const Ref<Planet> planet, bool wireframe)
	{
		sRendererData->PlanetDraw = { planet, wireframe };
	}

	void Renderer::SubmitMoveMarker(const Vector3& target, const Vector3& normal, float size, float alpha, AssetHandle texture)
	{
		sRendererData->MoveMarkerDrawList.push_back({ target, normal, size, alpha, texture });
	}
	
	void Renderer::SubmitHoveredMesh(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, uint32_t submeshIndex, uint32_t lodIndex)
	{
		sRendererData->MeshHoveredDrawList.emplace_back(mesh, transform, false, false, 0, submeshIndex, lodIndex);
	}

	void Renderer::DrawFullscreenQuad()
	{
		RenderCommand::Draw(3);
	}

	void Renderer::DrawQuad()
	{
		RenderCommand::Draw(6);
	}

	void Renderer::ClearDrawList()
	{
		sRendererData->MeshDrawList.clear();
		sRendererData->MeshWireframeDrawList.clear();
		sRendererData->MeshNoWireframeDrawList.clear();
		sRendererData->MeshSelectedDrawList.clear();
		sRendererData->MoveMarkerDrawList.clear();
		sRendererData->MeshHoveredDrawList.clear();
	}

	static Scope<Shader> equirectangularConversionShader, envFilteringShader, envIrradianceShader;

	Ref<TextureCube> Renderer::CreateStarFieldTexture(const Texture2D* starFieldTexture)
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		const uint32_t cubemapSize = 4096;

		Ref<TextureCube> envMapUnfiltered = CreateRef<TextureCube>("EnvMapUnfiltered", cubemapSize, cubemapSize);
		Ref<TextureCube> envMapFiltered = CreateRef<TextureCube>("EnvMapFiltered", cubemapSize, cubemapSize);

		envMapUnfiltered->CreateUAV(0);

		if (!equirectangularConversionShader)
			equirectangularConversionShader = CreateScope<Shader>("assets/shaders/Environment/EquirectangularToCubeMap.hlsl");

		equirectangularConversionShader->Bind();
		starFieldTexture->Bind(0, D3D11_COMPUTE_SHADER);
		RenderCommand::BindSampler(D3D11_COMPUTE_SHADER, 0, SamplerStates::Get(SamplerType::Default));
		envMapUnfiltered->BindForReadWrite(0, D3D11_COMPUTE_SHADER);
		RenderCommand::DispatchCompute(cubemapSize / 32, cubemapSize / 32, 6);
		envMapUnfiltered->UnbindUAV();

		return envMapUnfiltered;
	}

	void Renderer::GeometryPass(Vector3 worldTranslation)
	{
		TOAST_PROFILE_FUNCTION();

#ifdef TOAST_DEBUG
		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation)
			annotation->BeginEvent(L"Geometry Pass");
#endif

		{	// ---- Setup ----
			TOAST_PROFILE(*sRendererData->FrameProfiler, "Setup");
			RenderCommand::SetViewport(sRendererData->Viewport);
			RenderCommand::SetRenderTargets({ sRendererData->GPassPositionRT->GetRTV().Get(), sRendererData->GPassNormalRT->GetRTV().Get(), sRendererData->GPassAlbedoMetallicRT->GetRTV().Get(), sRendererData->GPassRoughnessAORT->GetRTV().Get(), sRendererData->GPassPickingRT->GetRTV().Get(), sRendererData->PlanetMaterialDebugRT->GetRTV().Get() }, sRendererData->DepthStencilView);
			RenderCommand::SetDepthStencilState(sRendererData->DepthEnabledStencilState);
		}

		RenderCommand::SetBlendState(sRendererData->GPassBlendState, { 0.0f, 0.0f, 0.0f, 0.0f });
		RenderCommand::ClearDepthStencilView(sRendererData->DepthStencilView);
		RenderCommand::ClearRenderTargets({ sRendererData->GPassPositionRT->GetRTV().Get(), sRendererData->GPassNormalRT->GetRTV().Get(), sRendererData->GPassAlbedoMetallicRT->GetRTV().Get(), sRendererData->GPassRoughnessAORT->GetRTV().Get(), sRendererData->GPassPickingRT->GetRTV().Get(), sRendererData->PlanetMaterialDebugRT->GetRTV().Get() }, { 0.0f, 0.0f, 0.0f, 1.0f });
		RenderCommand::SetPrimitiveTopology(Topology::TRIANGLELIST);

		if (sRendererData->PlanetDraw.Planet)
		{
			TOAST_PROFILE(*sRendererData->FrameProfiler, "PlanetBake");

			sRendererData->PlanetDraw.Planet->MapRenderingSettings();
			BindPlanetTerrainResources(false, true);

			sRendererData->PlanetDraw.Planet->GetMesh()->Bake();

			RenderCommand::ClearShaderResources();
		}

		if (sRendererData->PlanetDraw.Planet)
		{
			TOAST_PROFILE(*sRendererData->FrameProfiler, "Planet");

			if (sRendererData->Wireframe == 1)
				RenderCommand::SetRasterizerState(sRendererData->WireframeRasterizerState);
			else
				RenderCommand::SetRasterizerState(sRendererData->NormalRasterizerState);

			auto shader = AssetManager::GetAsset<Shader>(sRendererData->PlanetGeometryPassShaderHandle);
			if (shader)
				shader->Bind();

			auto& planetMesh = sRendererData->PlanetDraw.Planet->GetMesh();

			planetMesh->GetShaderInputLayout()->Bind();

 			BindPlanetTerrainResources(true, false);

			sRendererData->PlanetDraw.Planet->MapRenderingSettings();
			sRendererData->PlanetDraw.Planet->GetPlanetRenderingSettingsCBuffer()->Bind();

			sRendererData->ModelBuffer.Write((uint8_t*)&sRendererData->PlanetDraw.Planet->GetTransformRotation(), 64, 0);
			sRendererData->ModelCBuffer->Map(sRendererData->ModelBuffer);

			RenderCommand::BindSampler(D3D11_PIXEL_SHADER, 0, SamplerStates::Get(SamplerType::UWrapVClamp)); 
			RenderCommand::BindSampler(D3D11_PIXEL_SHADER, 1, SamplerStates::Get(SamplerType::LinearWrap));

			sRendererData->MaterialBuffer.Write((uint8_t*)&sRendererData->PlanetDraw.Planet->GetAlbedoColor(), 16, 0);
			sRendererData->MaterialBuffer.Write((uint8_t*)&sRendererData->PlanetDraw.Planet->GetMetalness(), 4, 20);
			sRendererData->MaterialBuffer.Write((uint8_t*)&sRendererData->PlanetDraw.Planet->GetRoughness(), 4, 24);
			int useAlbedo = static_cast<int>(sRendererData->PlanetDraw.Planet->GetUseAlbedoMap());
			sRendererData->MaterialBuffer.Write((uint8_t*)&useAlbedo, 4, 28);
			sRendererData->MaterialCBuffer->Map(sRendererData->MaterialBuffer);

			auto& transform = sRendererData->PlanetDraw.Planet->GetTransformNoScale();
			sRendererData->ModelBuffer.Write((uint8_t*)&transform, 64, 0);
			sRendererData->ModelCBuffer->Map(sRendererData->ModelBuffer);
			sRendererData->ModelCBuffer->Bind();

			planetMesh->BindGPUData();
			RenderCommand::DrawIndexedInstanced(planetMesh->GetIndexCount(), planetMesh->GetPatchCount(), 0, 0, 0);
			planetMesh->UnbindGPUData();
		}

		RenderCommand::ClearShaderResources();

		if (sRendererData->PlanetDraw.Planet && !sRendererData->PlanetDraw.Planet->GetTerrainObjects().empty())
		{
			TOAST_PROFILE(*sRendererData->FrameProfiler, "TerrainScatter");
			BindPlanetTerrainResources(false, true);
			sRendererData->TerrainObjects->Scatter(sRendererData->PlanetDraw.Planet.get(), worldTranslation);
		}

		RenderCommand::BindSampler(D3D11_PIXEL_SHADER, 0, SamplerStates::Get(SamplerType::LinearWrap));

		auto shader = AssetManager::GetAsset<Shader>(sRendererData->GeometryPassShaderHandle);
		if (shader)
			shader->Bind();

		if (sRendererData->PlanetDraw.Planet)
		{
			Planet* planet = sRendererData->PlanetDraw.Planet.get();

			if (!planet->GetTerrainObjects().empty())
			{
				TOAST_PROFILE(*sRendererData->FrameProfiler, "TerrainObjects");

				DrawTerrainObjects(planet);
			}
		}

		sRendererData->CurrentMesh = nullptr;

		{	// ---- Meshes (new braces around the existing loop) ----
			TOAST_PROFILE(*sRendererData->FrameProfiler, "Meshes");

			for (const auto& meshCommand : sRendererData->MeshDrawList)
			{
				Microsoft::WRL::ComPtr<ID3D11RasterizerState> rs = meshCommand.Wireframe ? sRendererData->WireframeRasterizerState : sRendererData->NormalRasterizerState;

				if (sRendererData->CurrentRasterizerState != rs.Get())
				{
					RenderCommand::SetRasterizerState(rs);
					sRendererData->CurrentRasterizerState = rs.Get();
				}

				if (sRendererData->CurrentTopology != meshCommand.Mesh->mTopology)
				{
					RenderCommand::SetPrimitiveTopology(meshCommand.Mesh->mTopology);
					sRendererData->CurrentTopology = meshCommand.Mesh->mTopology;
				}

				int isInstanced = meshCommand.Mesh->IsInstanced() ? 1 : 0;

				float clickable = 1.0f;

				const Submesh& submesh = meshCommand.Mesh->mLODGroups[meshCommand.LODIndex]->Submeshes[meshCommand.SubmeshIndex];

				// Model data
				sRendererData->ModelBuffer.Write((uint8_t*)&meshCommand.Transform, 64, 0);
				sRendererData->ModelBuffer.Write((uint8_t*)&clickable, 4, 64);
				sRendererData->ModelBuffer.Write((uint8_t*)&meshCommand.EntityID, 4, 68);
				sRendererData->ModelBuffer.Write((uint8_t*)&meshCommand.NoWorldTransform, 4, 72);
				sRendererData->ModelBuffer.Write((uint8_t*)&isInstanced, 4, 76);
				sRendererData->ModelCBuffer->Map(sRendererData->ModelBuffer);

				//Material data
				Ref<Material> material = AssetManager::GetAsset<Material>(submesh.MaterialHandle);
				if (!material)
				{
					TOAST_CORE_WARN("Renderer: material handle %llu failed to resolve, skipping submesh", (uint64_t)submesh.MaterialHandle);
					continue;
				}
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

				if (material->GetUseAlbedo())
					RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 3, AssetManager::GetAsset<Texture2D>(material->GetAlbedoAssetHandle())->GetSRV());
				if (material->GetUseNormal())
					RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 4, AssetManager::GetAsset<Texture2D>(material->GetNormalAssetHandle())->GetSRV());
				if (material->GetUseMetalRough())
					RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 5, AssetManager::GetAsset<Texture2D>(material->GetMetalRoughAssetHandle())->GetSRV());

				if (sRendererData->CurrentMesh != meshCommand.Mesh.get())
				{
					meshCommand.Mesh->Bind(meshCommand.LODIndex);
					sRendererData->CurrentMesh = meshCommand.Mesh.get();
				}

				RenderCommand::DrawIndexed(0, submesh.BaseIndex, submesh.IndexCount);
			}			 
		}

		std::vector<ID3D11RenderTargetView*> nullRTVs(6, nullptr);

		shader = AssetManager::GetAsset<Shader>(sRendererData->GeometryPassShaderHandle);
		if (shader)
			shader->Bind();

		RenderCommand::SetRenderTargets(nullRTVs, nullptr);
		RenderCommand::SetDepthStencilState(nullptr);
		RenderCommand::SetBlendState(nullptr);
		RenderCommand::ClearShaderResources();

#ifdef TOAST_DEBUG
		if (annotation)
			annotation->EndEvent();
#endif
	}

	void Renderer::ShadowPass(Scene::CascadedShadowMapParams& shadowParams)
	{
		TOAST_PROFILE_FUNCTION();

#ifdef TOAST_DEBUG
		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation)
			annotation->BeginEvent(L"Shadow Pass (CSM)");
#endif

		{
			TOAST_PROFILE(*sRendererData->FrameProfiler, "Setup");
			RenderCommand::SetViewport(sRendererData->ShadowMapViewport);
			RenderCommand::SetRasterizerState(sRendererData->ShadowMapRasterizerState);
			RenderCommand::SetDepthStencilState(sRendererData->ShadowPassDepthStencilState);
			RenderCommand::SetPrimitiveTopology(Topology::TRIANGLELIST);
		}

		auto shader = AssetManager::GetAsset<Shader>(sRendererData->ShadowPassShaderHandle);
		if (shader)
			shader->Bind();

		// TEMPORARY FIX NEED TO FIX IN THE FUTURE; TODO
		RenderCommand::sRendererAPI.get()->GetDeviceContext()->PSSetShader(nullptr, nullptr, 0);

		ID3D11RenderTargetView* nullRTV = nullptr;

		sRendererData->CurrentMesh = nullptr;

		{
			TOAST_PROFILE(*sRendererData->FrameProfiler, "Cascades");

			for (uint32_t i = 0; i < shadowParams.CascadeCount; ++i)
			{
				// Bind the slice
				RenderCommand::SetRenderTargets({ nullRTV }, sRendererData->ShadowPassDepthStencilView[i]);
				RenderCommand::ClearDepthStencilView(sRendererData->ShadowPassDepthStencilView[i], 1.0f);

				sRendererData->LightningBuffer.Write((uint8_t*)&i, 4, 320);
				sRendererData->LightningCBuffer->Map(sRendererData->LightningBuffer);

				DrawPlanetShadowCaster();
				DrawTerrainObjectShadowCasters();

				if (shader)
					shader->Bind();
				RenderCommand::sRendererAPI.get()->GetDeviceContext()->PSSetShader(nullptr, nullptr, 0);
				sRendererData->CurrentMesh = nullptr;

				for (const auto& meshCommand : sRendererData->MeshDrawList)
				{
					const Submesh& submesh = meshCommand.Mesh->mLODGroups[meshCommand.LODIndex]->Submeshes[meshCommand.SubmeshIndex];

					if (sRendererData->CurrentMesh != meshCommand.Mesh.get())
					{
						meshCommand.Mesh->Bind(meshCommand.LODIndex);
						sRendererData->CurrentMesh = meshCommand.Mesh.get();
					}

					int isInstanced = meshCommand.Mesh->IsInstanced() ? 1 : 0;

					float clickable = 1.0f;

					// Model data
					sRendererData->ModelBuffer.Write((uint8_t*)&meshCommand.Transform, 64, 0);
					sRendererData->ModelBuffer.Write((uint8_t*)&meshCommand.NoWorldTransform, 4, 72);
					sRendererData->ModelBuffer.Write((uint8_t*)&isInstanced, 4, 76);
					sRendererData->ModelCBuffer->Map(sRendererData->ModelBuffer);

					RenderCommand::DrawIndexed(0, submesh.BaseIndex, submesh.IndexCount);
				}
			}
		}

		RenderCommand::SetRenderTargets({ nullRTV }, nullptr);
		RenderCommand::SetDepthStencilState(nullptr);
		RenderCommand::SetBlendState(nullptr);
		RenderCommand::ClearShaderResources();

#ifdef TOAST_DEBUG
		if (annotation)
			annotation->EndEvent();
#endif
	}

	void Renderer::DrawPlanetShadowCaster()
	{
		Planet* planet = sRendererData->PlanetDraw.Planet.get();
		if (!planet)
			return;

		auto shader = AssetManager::GetAsset<Shader>(sRendererData->PlanetShadowPassShaderHandle);
		if (!shader)
			return;
		shader->Bind();
		RenderCommand::sRendererAPI.get()->GetDeviceContext()->PSSetShader(nullptr, nullptr, 0);

		auto& planetMesh = planet->GetMesh();
		planetMesh->GetShaderInputLayout()->Bind();

		auto& transform = planet->GetTransformNoScale();
		sRendererData->ModelBuffer.Write((uint8_t*)&transform, 64, 0);
		sRendererData->ModelCBuffer->Map(sRendererData->ModelBuffer);
		sRendererData->ModelCBuffer->Bind();

		planetMesh->BindGPUData();
		RenderCommand::DrawIndexedInstanced(planetMesh->GetIndexCount(), planetMesh->GetPatchCount(), 0, 0, 0);
		planetMesh->UnbindGPUData();
	}

	void Renderer::DrawTerrainObjectShadowCasters()
	{
		Planet* planet = sRendererData->PlanetDraw.Planet.get();
		if (!planet)
			return;

		const auto& objects = planet->GetTerrainObjects();
		if (objects.empty())
			return;

		auto shader = AssetManager::GetAsset<Shader>(sRendererData->TerrainObjectShadowPassShaderHandle);
		if (!shader)
			return;
		shader->Bind();
		RenderCommand::sRendererAPI.get()->GetDeviceContext()->PSSetShader(nullptr, nullptr, 0);

		auto& objectSystem = sRendererData->TerrainObjects;
		const uint32_t typeCount = std::min((uint32_t)objects.size(), (uint32_t)MAX_TERRAIN_OBJECT_TYPES);

		for (uint32_t t = 0; t < typeCount; ++t)
		{
			const TerrainObject& object = objects[t];
			if (!object.MeshObject || object.CandidateGridSize == 0)
				continue;

			object.MeshObject->Bind(0);
			RenderCommand::SetShaderResource(D3D11_VERTEX_SHADER, 0, objectSystem->GetInstanceBuffer(t)->GetSRV());
			RenderCommand::DrawIndexedInstancedIndirect(objectSystem->GetIndirectArgs(), objectSystem->GetArgsOffset(t));
		}

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> nullSRV;
		RenderCommand::SetShaderResource(D3D11_VERTEX_SHADER, 0, nullSRV);
	}

	void Renderer::SSAOPass(float radius, float bias)
	{
		TOAST_PROFILE_FUNCTION();

#ifdef TOAST_DEBUG
		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation)
			annotation->BeginEvent(L"SSAO Pass");
#endif

		Ref<Shader> shader;

		{
			TOAST_PROFILE(*sRendererData->FrameProfiler, "SSAO Compute");

			RenderCommand::SetViewport(sRendererData->Viewport);
			RenderCommand::SetRasterizerState(sRendererData->NormalRasterizerState);
			RenderCommand::SetDepthStencilState(sRendererData->DepthDisabledStencilState);
			RenderCommand::SetRenderTargets({ sRendererData->SSAORT->GetRTV().Get() }, nullptr);
			RenderCommand::ClearRenderTargets({ sRendererData->SSAORT->GetRTV().Get() }, { 0.0f, 0.0f, 0.0f, 1.0f });

			shader = AssetManager::GetAsset<Shader>(sRendererData->SSAOPassShaderHandle);
			if (shader)
				shader->Bind();

			RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 0, sRendererData->GPassPositionRT->GetSRV());
			RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 1, sRendererData->GPassNormalRT->GetSRV());
			RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 2, sRendererData->SSAONoiseTexture->GetSRV());

			RenderCommand::BindSampler(D3D11_PIXEL_SHADER, 3, SamplerStates::Get(SamplerType::PointClamp));
			RenderCommand::BindSampler(D3D11_PIXEL_SHADER, 4, SamplerStates::Get(SamplerType::LinearClamp));

			sRendererData->SSAOBuffer.Write((uint8_t*)&sRendererData->SSAOKernel[0], 1024, 0);
			sRendererData->SSAOBuffer.Write((uint8_t*)&radius, 4, 1024);
			sRendererData->SSAOBuffer.Write((uint8_t*)&bias, 4, 1028);

			sRendererData->SSAOCBuffer->Map(sRendererData->SSAOBuffer);
			sRendererData->SSAOCBuffer->Bind();

			DrawFullscreenQuad();
		}

		{
			TOAST_PROFILE(*sRendererData->FrameProfiler, "SSAO Blur");

			RenderCommand::SetRenderTargets({ sRendererData->SSAOBlurRT->GetRTV().Get() }, nullptr);

			RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 0, sRendererData->SSAORT->GetSRV());

			shader = AssetManager::GetAsset<Shader>(sRendererData->SSAOBlurPassShaderHandle);
			if (shader)
				shader->Bind();

			DrawFullscreenQuad();
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
	
	void Renderer::LightningPass(Ref<Planet>& planet, Scene::Environment& environment)
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

		// Updating the atmospheric data in the buffer and mapping it to the GPU
		auto& atmosphere = planet->GetAtmosphere();
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.AtmosphereHeight, 4, 0);
		sRendererData->AtmosphereCBuffer->Map(sRendererData->AtmosphereBuffer);
		sRendererData->AtmosphereCBuffer->Bind();

		// Updating the lighting data in the buffer and mapping it to the GPU
		sRendererData->LightningPassBuffer.Write((uint8_t*)&environment.DiffuseIBLGain, 4, 0);
		sRendererData->LightningPassBuffer.Write((uint8_t*)&environment.SpecularIBLGain, 4, 4);
		sRendererData->LightningPassBuffer.Write((uint8_t*)&environment.NightSkyLight, 4, 8);
		sRendererData->LightningPassCBuffer->Map(sRendererData->LightningPassBuffer);
		sRendererData->LightningPassCBuffer->Bind();

		sRendererData->PlanetDraw.Planet->GetPlanetFrameCBuffer()->Bind();

		auto shader = AssetManager::GetAsset<Shader>(sRendererData->LightningPassShaderHandle);
		if (shader)
			shader->Bind();

		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 0, sRendererData->GPassPositionRT->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 1, sRendererData->GPassNormalRT->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 2, sRendererData->GPassAlbedoMetallicRT->GetSRV());

		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 3, sRendererData->GPassRoughnessAORT->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 10, sRendererData->SSAOBlurRT->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 12, sRendererData->ShadowMapArray->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 13, sRendererData->GPassPickingRT->GetSRV());

		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 4, sRendererData->IrradianceCubeMap->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 5, sRendererData->EnvMapFiltered->GetSRV());
		if (sRendererData->PlanetDraw.Planet->GetHeightMapCubeTexture())
			RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 16, sRendererData->PlanetDraw.Planet->GetHeightMapCubeTexture()->GetSRV());

		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 17, sRendererData->DepthBuffer->GetSRV());

		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 6, sRendererData->SpecularBRDFLUT->GetSRV());

		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 7, planet->GetTransmittanceLUT()->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 9, planet->GetGroundIrradianceLUT()->GetSRV());

		RenderCommand::BindSampler(D3D11_PIXEL_SHADER, 0, SamplerStates::Get(SamplerType::Default));
		RenderCommand::BindSampler(D3D11_PIXEL_SHADER, 1, SamplerStates::Get(SamplerType::BRDF));
		RenderCommand::BindSampler(D3D11_PIXEL_SHADER, 2, SamplerStates::Get(SamplerType::PointClamp));
		RenderCommand::BindSampler(D3D11_PIXEL_SHADER, 3, SamplerStates::Get(SamplerType::LinearClamp));
		RenderCommand::BindSampler(D3D11_PIXEL_SHADER, 4, SamplerStates::Get(SamplerType::ShadowCmp));

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

	void Renderer::StarFieldPass(Scene::Environment& environment, Ref<Planet>& planet, const float atmosphereHeight)
	{
		TOAST_PROFILE_FUNCTION();

#ifdef TOAST_DEBUG
		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation)
			annotation->BeginEvent(L"Star Field Pass");
#endif

		if (sRendererData->PlanetDraw.Planet)
		{
			if (sRendererData->PlanetDraw.Planet->GetStarFieldTextureCube())
			{
				RenderCommand::ClearRenderTargets({ sRendererData->StarsRT->GetRTV().Get() }, { 0.0f, 0.0f, 0.0f, 1.0f });

				sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphereHeight, 4, 0);
				sRendererData->AtmosphereCBuffer->Map(sRendererData->AtmosphereBuffer);
				sRendererData->AtmosphereCBuffer->Bind();

				sRendererData->StarsBuffer.Write((uint8_t*)&environment.StarNits, 4, 0);
				sRendererData->StarsBuffer.Write((uint8_t*)&environment.TwilightStartDeg, 4, 4);
				sRendererData->StarsBuffer.Write((uint8_t*)&environment.TwilightEndDeg, 4, 8);
				sRendererData->StarsBuffer.Write((uint8_t*)&environment.SpaceFadeStart, 4, 12);
				sRendererData->StarsBuffer.Write((uint8_t*)&environment.SpaceFadeEnd, 4, 16);
				sRendererData->StarsCBuffer->Map(sRendererData->StarsBuffer);
				sRendererData->StarsCBuffer->Bind();

				RenderCommand::SetRenderTargets({ sRendererData->StarsRT->GetRTV().Get() }, nullptr);
				RenderCommand::SetDepthStencilState(sRendererData->DepthDisabledStencilState);
				RenderCommand::SetBlendState(nullptr);
				RenderCommand::SetBlendState(sRendererData->LPassBlendState, { 0.0f, 0.0f, 0.0f, 0.0f });

				sRendererData->PlanetDraw.Planet->GetPlanetFrameCBuffer()->Bind();

				RenderCommand::BindSampler(D3D11_PIXEL_SHADER, 0, SamplerStates::Get(SamplerType::LinearWrap));
				RenderCommand::BindSampler(D3D11_PIXEL_SHADER, 1, SamplerStates::Get(SamplerType::PointClamp));

				RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 0, planet->GetTransmittanceLUT()->GetSRV());
				RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 5, sRendererData->PlanetDraw.Planet->GetStarFieldTextureCube()->GetSRV());
				RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 9, sRendererData->DepthBuffer->GetSRV());

				auto shader = AssetManager::GetAsset<Shader>(sRendererData->StarFieldShaderHandle);
				if (shader)
					shader->Bind();

				DrawFullscreenQuad();
			}
		}

		ID3D11RenderTargetView* nullRTV = nullptr;
		RenderCommand::SetRenderTargets({ nullRTV }, nullptr);

#ifdef TOAST_DEBUG
		if (annotation)
			annotation->EndEvent();
#endif
	}

	void Renderer::AtmospherePass(Ref<Planet>& planet, Scene::Environment& environment, DirectX::XMFLOAT4 camPosWS, DirectX::XMFLOAT3 worldOffsetWS)
	{
		TOAST_PROFILE_FUNCTION();

#ifdef TOAST_DEBUG
		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation)
			annotation->BeginEvent(L"Atmosphere Pass");
#endif

		Ref<Shader> shader;
		Ref<Texture2D> APFar;
		Ref<Texture2D> skyview;
		Ref<Texture3D> aerialPerspective;
		float bakeIBL = 0.0f;

		{
			TOAST_PROFILE(*sRendererData->FrameProfiler, "Setup");

			RenderCommand::BindSampler(D3D11_COMPUTE_SHADER, 0, SamplerStates::Get(SamplerType::LinearClamp));
			RenderCommand::BindSampler(D3D11_COMPUTE_SHADER, 1, SamplerStates::Get(SamplerType::PointClamp));
			RenderCommand::BindSampler(D3D11_PIXEL_SHADER, 0, SamplerStates::Get(SamplerType::LinearClamp));
			RenderCommand::BindSampler(D3D11_PIXEL_SHADER, 1, SamplerStates::Get(SamplerType::PointClamp));

			// Updating the atmospheric data in the buffer and mapping it to the GPU
			UploadAtmosphereCBuffer(planet.get(), bakeIBL);

			{
				auto& buf = sRendererData->SunDiscSettingsBuffer;

				int32_t sunDiscToggle = environment.SunDiscToggle ? 1 : 0;

				buf.Write((uint8_t*)&environment.SunDiscRadius, 4, offsetof(SunDiscCBufferGPU, SunDiscRadius));
				buf.Write((uint8_t*)&sunDiscToggle, 4, offsetof(SunDiscCBufferGPU, SunDiscToggle));

				sRendererData->SunDiscSettingsCBuffer->Map(sRendererData->SunDiscSettingsBuffer);
				sRendererData->SunDiscSettingsCBuffer->Bind();
			}

			sRendererData->PlanetDraw.Planet->GetPlanetFrameCBuffer()->Bind();
		}

		{
			TOAST_PROFILE(*sRendererData->FrameProfiler, "AP Far");
			UINT zero[4] = { 0,0,0,0 };

			aerialPerspective = planet->GetAerialPerspectiveLUT();
			APFar = planet->GetAPFar();
			RenderCommand::ClearUAV(APFar->GetUAV().Get(), zero);
			RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 0, sRendererData->DepthBuffer->GetSRV());
			APFar->BindForReadWrite(0, D3D11_COMPUTE_SHADER);

			shader = AssetManager::GetAsset<Shader>(sRendererData->APFarDynamicShaderHandle);
			if (shader)
				shader->Bind();

			RenderCommand::DispatchCompute((aerialPerspective->GetWidth() + 7) / 8, (aerialPerspective->GetHeight() + 7) / 8, 1);
			APFar->UnbindUAV(0, D3D11_COMPUTE_SHADER);
		}

		{
			TOAST_PROFILE(*sRendererData->FrameProfiler, "Shadow Height Map");
			auto heightCube = planet->GetHeightMapCubeTexture();
			if (heightCube)
			{
				RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 0, heightCube->GetSRV());
				RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 1, APFar->GetSRV());
				sRendererData->ShadowHeightMap->BindForReadWrite(0, D3D11_COMPUTE_SHADER);

				shader = AssetManager::GetAsset<Shader>(sRendererData->ShadowHeightMapCSShaderHandle);
				if (shader)
					shader->Bind();

				RenderCommand::DispatchCompute((512 + 7) / 8, (512 + 7) / 8, 1);
				sRendererData->ShadowHeightMap->UnbindUAV(0, D3D11_COMPUTE_SHADER); // read by the AP at t6 right after
			}
		}

		{
			TOAST_PROFILE(*sRendererData->FrameProfiler, "SkyView LUT");
			RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 0, planet->GetTransmittanceLUT()->GetSRV());
			RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 1, planet->GetMultiScatteringLUT()->GetSRV());

			skyview = planet->GetSkyViewLUT();
			skyview->BindForReadWrite(0, D3D11_COMPUTE_SHADER);

			shader = AssetManager::GetAsset<Shader>(sRendererData->SkyViewCSShaderHandle);
			if (shader)
				shader->Bind();

			RenderCommand::DispatchCompute((skyview->GetWidth() + 7) / 8, (skyview->GetHeight() + 7) / 8, 1);
			skyview->UnbindUAV(0, D3D11_COMPUTE_SHADER);
		}
		
		{
			TOAST_PROFILE(*sRendererData->FrameProfiler, "Aerial Perspective");
			aerialPerspective->BindForReadWrite(0, D3D11_COMPUTE_SHADER);
			RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 2, APFar->GetSRV());
			RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 3, sRendererData->ShadowMapArray->GetSRV());

			const uint32_t cur = sRendererData->APVisibilityCurrent;
			const uint32_t prev = 1u - cur;
			{
				const DirectX::XMVECTOR moved = DirectX::XMVectorSubtract(DirectX::XMLoadFloat4(&sRendererData->APCameraPos), DirectX::XMLoadFloat4(&sRendererData->APPrevCameraPos));
				const bool jumped = DirectX::XMVectorGetX(DirectX::XMVector3Length(moved)) > 1000.0f;
				const uint32_t historyValid = (sRendererData->APHistoryValid && !jumped) ? 1u : 0u;
				const float historyWeight = 0.1f;

				// Offsets match cbuffer APTemporal (b7) in AerialPerspectiveCS.hlsl
				sRendererData->APTemporalBuffer.Write((uint8_t*)&sRendererData->APPrevViewProj, 64, 0);
				sRendererData->APTemporalBuffer.Write((uint8_t*)&sRendererData->APPrevCameraPos, 12, 64);
				sRendererData->APTemporalBuffer.Write((uint8_t*)&sRendererData->APFrameIndex, 4, 76);
				sRendererData->APTemporalBuffer.Write((uint8_t*)&historyWeight, 4, 80);
				sRendererData->APTemporalBuffer.Write((uint8_t*)&historyValid, 4, 84);
				sRendererData->APTemporalCBuffer->Map(sRendererData->APTemporalBuffer);
				sRendererData->APTemporalCBuffer->Bind();
			}
			RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 4, sRendererData->APVisibility[prev]->GetSRV());
			sRendererData->APVisibility[cur]->BindForReadWrite(1, D3D11_COMPUTE_SHADER);
			sRendererData->APSkyShadow->BindForReadWrite(2, D3D11_COMPUTE_SHADER);
			RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 5, sRendererData->DepthBuffer->GetSRV());
			RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 6, sRendererData->ShadowHeightMap->GetSRV());

			shader = AssetManager::GetAsset<Shader>(sRendererData->AerialPerspectiveCSShaderHandle);
			if (shader)
				shader->Bind();

			RenderCommand::DispatchCompute((aerialPerspective->GetWidth() + 7) / 8, (aerialPerspective->GetHeight() + 7) / 8, 1);
			aerialPerspective->UnbindUAV(0, D3D11_COMPUTE_SHADER);
			sRendererData->APVisibility[cur]->UnbindUAV(1, D3D11_COMPUTE_SHADER);
			sRendererData->APSkyShadow->UnbindUAV(2, D3D11_COMPUTE_SHADER);

			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> nullSRV;
			RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 5, nullSRV);

			sRendererData->APVisibilityCurrent = prev;
			sRendererData->APPrevViewProj = sRendererData->APViewProj;
			sRendererData->APPrevCameraPos = sRendererData->APCameraPos;
			sRendererData->APHistoryValid = true;
			sRendererData->APFrameIndex++;
		}

		{
			TOAST_PROFILE(*sRendererData->FrameProfiler, "Atmosphere Raymarch");
			RenderCommand::ClearRenderTargets({ sRendererData->AtmospherePassRT->GetRTV().Get(), sRendererData->SunDiscMaskRT->GetRTV().Get(), sRendererData->SunHaloMaskRT->GetRTV().Get() }, { 0.0f, 0.0f, 0.0f, 1.0f });
			RenderCommand::SetRenderTargets({ sRendererData->AtmospherePassRT->GetRTV().Get(), sRendererData->SunDiscMaskRT->GetRTV().Get(), sRendererData->SunHaloMaskRT->GetRTV().Get() }, nullptr);
			RenderCommand::SetDepthStencilState(sRendererData->DepthEnabledStencilState);
			RenderCommand::SetBlendState(nullptr, { 1.0f, 1.0f, 1.0f, 1.0f });

			shader = AssetManager::GetAsset<Shader>(sRendererData->AtmosphereShaderHandle);
			if (shader)
				shader->Bind();

			RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 0, planet->GetTransmittanceLUT()->GetSRV());
			RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 1, planet->GetMultiScatteringLUT()->GetSRV());
			RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 2, skyview->GetSRV());
			RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 3, aerialPerspective->GetSRV());
			RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 5, APFar->GetSRV());
			RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 7, sRendererData->APSkyShadow->GetSRV());
			RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 9, sRendererData->DepthBuffer->GetSRV());
			RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 10, sRendererData->LPassRT->GetSRV());

			DrawFullscreenQuad();
		}

		static int currentFace = 0;// Tracks which face of the cube to render

		{
			TOAST_PROFILE(*sRendererData->FrameProfiler, "IBL Cube Face");

			const DirectX::XMMATRIX& viewMatrix = sRendererData->AtmosphericScatteringViewMatrices[currentFace];
			const DirectX::XMMATRIX& invViewMatrix = sRendererData->AtmosphericScatteringInvViewMatrices[currentFace];
			DirectX::XMFLOAT4 cameraPos = camPosWS;

			sRendererData->CameraBuffer.Write((uint8_t*)&viewMatrix, sizeof(viewMatrix), 64);
			sRendererData->CameraBuffer.Write((uint8_t*)&invViewMatrix, sizeof(invViewMatrix), 192);
			sRendererData->CameraBuffer.Write((uint8_t*)&cameraPos, sizeof(cameraPos), 320);
			sRendererData->CameraCBuffer->Map(sRendererData->CameraBuffer);

			int enableSun = 0;
			sRendererData->SunDiscSettingsBuffer.Write((uint8_t*)&enableSun, 4, offsetof(SunDiscCBufferGPU, SunDiscToggle));
			sRendererData->SunDiscSettingsCBuffer->Map(sRendererData->SunDiscSettingsBuffer);

			bakeIBL = 1.0f;

			sRendererData->AtmosphereBuffer.Write((uint8_t*)&currentFace, 4, offsetof(AtmosphereCBufferGPU, FaceIndex));
			sRendererData->AtmosphereBuffer.Write((uint8_t*)&bakeIBL, 4, offsetof(AtmosphereCBufferGPU, BakeIBL));
			sRendererData->AtmosphereCBuffer->Map(sRendererData->AtmosphereBuffer);

			RenderCommand::SetViewport(sRendererData->AtmosphereCubeViewport);
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> nullSRV = nullptr;
			RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 3, nullSRV);
			RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 9, nullSRV);
			RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 10, nullSRV);
			RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 6, planet->GetGroundIrradianceLUT()->GetSRV());
			RenderCommand::SetRenderTargets({ sRendererData->AtmosphereCubeRT->GetRTVFace(currentFace).Get(), sRendererData->Dummy1RT->GetRTVFace(currentFace).Get(), sRendererData->Dummy2RT->GetRTVFace(currentFace).Get() }, nullptr);

			DrawFullscreenQuad();

			RenderCommand::SetRenderTargets({ nullptr }, nullptr);
			RenderCommand::ClearShaderResources();
		}

		{
			TOAST_PROFILE(*sRendererData->FrameProfiler, "IBL Prefilter");
			UpdateEnvMapSource(sRendererData->AtmosphereCubeRT->GetTextureOriginal(), sRendererData->EnvMapSource, currentFace);
			GeneratePrefilteredEnvMap(sRendererData->AtmosphereCubeRT->GetTextureOriginal(), sRendererData->EnvMapFiltered, currentFace);
		}

		{
			TOAST_PROFILE(*sRendererData->FrameProfiler, "IBL Irradiance");
			GenerateIrradianceCubemap(sRendererData->EnvMapSource, sRendererData->IrradianceCubeMap, currentFace);
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

	void Renderer::ParticlesPass()
	{
		TOAST_PROFILE_FUNCTION();

#ifdef TOAST_DEBUG
		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation)
			annotation->BeginEvent(L"Particle Pass");
#endif
		auto particleSystem = sRendererData->Particles;

		RenderCommand::SetViewport(sRendererData->Viewport);
		RenderCommand::SetRasterizerState(sRendererData->NormalRasterizerState);
		RenderCommand::SetRenderTargets({ sRendererData->AtmospherePassRT->GetRTV().Get() }, sRendererData->DepthStencilView);
		RenderCommand::SetDepthStencilState(sRendererData->ParticleDepthStencilState);

		// Binding the pool of particles and the current alive list, this is the ping-pong list that alternates 
		// between frames.
		RenderCommand::SetShaderResource(D3D11_VERTEX_SHADER, 0, particleSystem->GetParticleBuffer()->GetSRV());
		RenderCommand::SetShaderResource(D3D11_VERTEX_SHADER, 1, particleSystem->GetCurrentAliveList()->GetSRV());

		if (particleSystem->GetMaskArray())
			particleSystem->GetMaskArray()->Bind(0, D3D11_PIXEL_SHADER);

		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 1, sRendererData->GPassPositionRT->GetSRV());

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		deviceContext->IASetIndexBuffer(sRendererData->ParticleIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

		auto shader = AssetManager::GetAsset<Shader>(sRendererData->ParticlesShaderHandle);
		if (shader)
			shader->Bind();

		struct { ID3D11BlendState* state; uint32_t mode; } passes[] =
		{
			{ sRendererData->ParticleAdditiveBlendState.Get(), 0u }, 
			{ sRendererData->ParticleAlphaBlendState.Get(), 1u }  
		};

		for (const auto& pass : passes)
		{
			RenderCommand::SetBlendState(pass.state, { 0.0f, 0.0f, 0.0f, 0.0f });

			particleSystem->GetParticleDrawBuffer()->Write((uint8_t*)&pass.mode, 4, 0);
			particleSystem->GetParticleDrawCBuffer()->Map(*particleSystem->GetParticleDrawBuffer());
			particleSystem->GetParticleDrawCBuffer()->Bind();

			RenderCommand::DrawIndexedInstancedIndirect(particleSystem->GetIndirectArgs(), ARGS_OFFSET_DRAW);
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

	void Renderer::BloomMipChainPass()
	{
		TOAST_PROFILE_FUNCTION();

#ifdef TOAST_DEBUG
		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation)
			annotation->BeginEvent(L"Bloom Mip Chain");
#endif

		auto shader = AssetManager::GetAsset<Shader>(sRendererData->BloomMipDownShaderHandle);
		if (!shader)
			return;

		RenderCommand::SetRasterizerState(sRendererData->NormalRasterizerState);
		RenderCommand::SetDepthStencilState(sRendererData->DepthDisabledStencilState);
		RenderCommand::SetBlendState(nullptr);
		RenderCommand::BindSampler(D3D11_PIXEL_SHADER, 3, SamplerStates::Get(SamplerType::LinearClamp));
		sRendererData->BloomMipCBuffer->Bind();
		shader->Bind();

		ID3D11RenderTargetView* nullRTV = nullptr;

		for (uint32_t i = 0; i < RendererData::BloomMipCount; ++i)
		{
			RenderTarget* src = (i == 0) ? sRendererData->AtmospherePassRT.get() : sRendererData->BloomMips[i - 1].get();
			RenderTarget* dst = sRendererData->BloomMips[i].get();

			auto [srcW, srcH] = src->GetSize();
			auto [dstW, dstH] = dst->GetSize();

			float params[4] = { 1.0f / float(srcW), 1.0f / float(srcH), (i == 0) ? 1.0f : 0.0f, 0.0f };
			sRendererData->BloomMipBuffer.Write((uint8_t*)params, sizeof(params), 0);
			sRendererData->BloomMipCBuffer->Map(sRendererData->BloomMipBuffer);

			D3D11_VIEWPORT viewport = { 0.0f, 0.0f, float(dstW), float(dstH), 0.0f, 1.0f };
			RenderCommand::SetViewport(viewport);
			RenderCommand::SetRenderTargets({ dst->GetRTV().Get() }, nullptr);
			RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 0, src->GetSRV());
			RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 1, sRendererData->SunDiscMaskRT->GetSRV());

			DrawFullscreenQuad();

			RenderCommand::SetRenderTargets({ nullRTV }, nullptr);
			RenderCommand::ClearShaderResources();
		}

		RenderCommand::SetViewport(sRendererData->Viewport);
		RenderCommand::SetDepthStencilState(nullptr);

#ifdef TOAST_DEBUG
		if (annotation)
			annotation->EndEvent();
#endif
	}

	void Renderer::BloomUpSamplePass(float falloff)
	{
		TOAST_PROFILE_FUNCTION();

#ifdef TOAST_DEBUG
		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation)
			annotation->BeginEvent(L"Bloom Up Sample");
#endif

		auto shader = AssetManager::GetAsset<Shader>(sRendererData->BloomMipUpShaderHandle);
		if (!shader)
			return;

		RenderCommand::SetRasterizerState(sRendererData->NormalRasterizerState);
		RenderCommand::SetDepthStencilState(sRendererData->DepthDisabledStencilState);
		RenderCommand::BindSampler(D3D11_PIXEL_SHADER, 3, SamplerStates::Get(SamplerType::LinearClamp));
		sRendererData->BloomMipCBuffer->Bind();
		shader->Bind();

		ID3D11RenderTargetView* nullRTV = nullptr;
		const uint32_t count = RendererData::BloomMipCount;
		float coarserWeight = std::pow(falloff, float(count - 1));

		for (int32_t i = int32_t(count) - 2; i >= 0; --i)
		{
			RenderTarget* src =  sRendererData->BloomMips[i + 1].get();
			RenderTarget* dst = sRendererData->BloomMips[i].get();

			auto [srcW, srcH] = src->GetSize();
			auto [dstW, dstH] = dst->GetSize();

			float params[4] = { 1.0f / float(srcW), 1.0f / float(srcH), (i == 0) ? 1.0f : 0.0f, 0.0f };
			sRendererData->BloomMipBuffer.Write((uint8_t*)params, sizeof(params), 0);
			sRendererData->BloomMipCBuffer->Map(sRendererData->BloomMipBuffer);

			float levelWeight = std::pow(falloff, float(i));
			float f = coarserWeight / (coarserWeight + levelWeight);
			coarserWeight += levelWeight;
			RenderCommand::SetBlendState(sRendererData->BloomUpBlendState, { f, f, f, f });

			D3D11_VIEWPORT viewport = { 0.0f, 0.0f, float(dstW), float(dstH), 0.0f, 1.0f };
			RenderCommand::SetViewport(viewport);
			RenderCommand::SetRenderTargets({ dst->GetRTV().Get() }, nullptr);
			RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 0, src->GetSRV());

			DrawFullscreenQuad();

			RenderCommand::SetRenderTargets({ nullRTV }, nullptr);
			RenderCommand::ClearShaderResources();
		}

		RenderCommand::SetBlendState(nullptr);
		RenderCommand::SetViewport(sRendererData->Viewport);
		RenderCommand::SetDepthStencilState(nullptr);

#ifdef TOAST_DEBUG
		if (annotation)
			annotation->EndEvent();
#endif
	}

	void Renderer::BloomBlendPass(float intensity, float falloff)
	{
		TOAST_PROFILE_FUNCTION();

#ifdef TOAST_DEBUG
		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation)
			annotation->BeginEvent(L"Bloom Blend");
#endif

		auto shader = AssetManager::GetAsset<Shader>(sRendererData->BloomBlendShaderHandle);
		if (!shader)
			return;

		auto [bloomW, bloomH] = sRendererData->BloomMips[0]->GetSize();
		float params[4] = { 1.0f / float(bloomW), 1.0f / (float)bloomH, intensity, falloff };
		sRendererData->BloomBlendBuffer.Write((uint8_t*)params, sizeof(params), 0);
		sRendererData->BloomBlendCBuffer->Map(sRendererData->BloomBlendBuffer);
		sRendererData->BloomBlendCBuffer->Bind();

		RenderCommand::SetRasterizerState(sRendererData->NormalRasterizerState);
		RenderCommand::SetDepthStencilState(sRendererData->DepthDisabledStencilState);
		RenderCommand::SetBlendState(nullptr);
		RenderCommand::BindSampler(D3D11_PIXEL_SHADER, 3, SamplerStates::Get(SamplerType::LinearClamp));
		RenderCommand::SetViewport(sRendererData->Viewport);
		RenderCommand::SetRenderTargets({ sRendererData->FinalBloomRT->GetRTV().Get() }, nullptr);
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 0, sRendererData->AtmospherePassRT->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 1, sRendererData->BloomMips[0]->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 2, sRendererData->SunVisibilityBuffer->GetSRV());
		shader->Bind();

		DrawFullscreenQuad();

		ID3D11RenderTargetView* nullRTV = nullptr;
		RenderCommand::SetRenderTargets({ nullRTV }, nullptr);
		RenderCommand::ClearShaderResources();
		RenderCommand::SetDepthStencilState(nullptr);

#ifdef TOAST_DEBUG
		if (annotation)
			annotation->EndEvent();
#endif
	}

	void Renderer::ExposureAdaptPass(const float dt, const Scene::ExposureParams& exposureParams, const float spaceFactor, const Ref<Planet>& planet)
	{
		TOAST_PROFILE_FUNCTION();

#ifdef TOAST_DEBUG
		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation)
			annotation->BeginEvent(L"Exposure Adapt Pass");
#endif

		const float compensation = exposureParams.Compensation;
		const float blendedMinEV = exposureParams.MinEV + (exposureParams.OrbitMinEV - exposureParams.MinEV) * std::clamp(spaceFactor, 0.0f, 1.0f);
		const float minEV = std::min(blendedMinEV, exposureParams.MaxEV);
		const float maxEV = exposureParams.MaxEV;
		const float speedToBright = std::max(exposureParams.SpeedToBright, 0.01f);
		const float speedToDark = std::max(exposureParams.SpeedToDark, 0.01f);
		const uint32_t reset = sRendererData->AutoExposureReset ? 1u : 0u;
		const uint32_t lockEV = exposureParams.LockEV ? 1u : 0u;
		const bool incidentValid = planet && planet->AtmosphereActivated();
		const uint32_t incidentValidU = incidentValid ? 1u : 0u;
		const float dimLightAdaptation = std::clamp(exposureParams.DimLightAdaptation, 0.0f, 1.0f);

		sRendererData->AutoExposureBuffer.Write((uint8_t*)&compensation, 4, 0);
		sRendererData->AutoExposureBuffer.Write((uint8_t*)&minEV, 4, 4);
		sRendererData->AutoExposureBuffer.Write((uint8_t*)&maxEV, 4, 8);
		sRendererData->AutoExposureBuffer.Write((uint8_t*)&speedToBright, 4, 12);
		sRendererData->AutoExposureBuffer.Write((uint8_t*)&speedToDark, 4, 16);
		sRendererData->AutoExposureBuffer.Write((uint8_t*)&dt, 4, 20);
		sRendererData->AutoExposureBuffer.Write((uint8_t*)&reset, 4, 24);
		sRendererData->AutoExposureBuffer.Write((uint8_t*)&lockEV, 4, 28);
		sRendererData->AutoExposureBuffer.Write((uint8_t*)&exposureParams.LockedEV, 4, 32);
		sRendererData->AutoExposureBuffer.Write((uint8_t*)&incidentValidU, 4,36);
		sRendererData->AutoExposureBuffer.Write((uint8_t*)&spaceFactor, 4, 40);      
		sRendererData->AutoExposureBuffer.Write((uint8_t*)&exposureParams.AdaptationKneeEV, 4, 44); 
		sRendererData->AutoExposureBuffer.Write((uint8_t*)&dimLightAdaptation, 4, 48);
		sRendererData->AutoExposureCBuffer->Map(sRendererData->AutoExposureBuffer);
		sRendererData->AutoExposureCBuffer->Bind();

		if (incidentValid)
		{
			sRendererData->CameraCBuffer->Bind();
			sRendererData->LightningCBuffer->Bind();
			planet->GetPlanetFrameCBuffer()->Bind();
			sRendererData->AtmosphereCBuffer->Bind();
			sRendererData->SunDiscSettingsCBuffer->Bind();
			RenderCommand::BindSampler(D3D11_COMPUTE_SHADER, 0, SamplerStates::Get(SamplerType::LinearClamp));
			RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 1, planet->GetTransmittanceLUT()->GetSRV());
			RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 2, planet->GetGroundIrradianceLUT()->GetSRV());
		}

		sRendererData->ExposureEVBuffer->BindUAV(0);

		auto shader = AssetManager::GetAsset<Shader>(sRendererData->ExposureAdaptShaderHandle);
		if (shader)
			shader->Bind();

		RenderCommand::DispatchCompute(1, 1, 1);

		sRendererData->ExposureEVBuffer->UnbindUAV(0);
		RenderCommand::ClearShaderResources();

		sRendererData->AutoExposureReset = false;

#ifdef TOAST_DEBUG
		if (annotation)
			annotation->EndEvent();
#endif
	}

	void Renderer::LocalExposureGridPass(Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& sceneHDR)
	{
		TOAST_PROFILE(*sRendererData->FrameProfiler, "Local Exposure Grid");

#ifdef TOAST_DEBUG
		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation)
			annotation->BeginEvent(L"Local Exposure Grid Pass");
#endif

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> nullSRV = nullptr;

		sRendererData->CameraCBuffer->Bind();
		RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 0, sceneHDR);
		RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 1, sRendererData->ExposureEVBuffer->GetSRV());
		sRendererData->LocalExposureGridRaw->BindForReadWrite(0, D3D11_COMPUTE_SHADER);

		if (auto shader = AssetManager::GetAsset<Shader>(sRendererData->LocalExposureGridShaderHandle))
			shader->Bind();
		RenderCommand::DispatchCompute(64, 32, 1);

		sRendererData->LocalExposureGridRaw->UnbindUAV(0, D3D11_COMPUTE_SHADER);
		RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 0, nullSRV);
		RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 1, nullSRV);

		RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 0, sRendererData->LocalExposureGridRaw->GetSRV());
		sRendererData->LocalExposureGrid->BindForReadWrite(0, D3D11_COMPUTE_SHADER);

		if (auto shader = AssetManager::GetAsset<Shader>(sRendererData->LocalExposureBlurShaderHandle))
			shader->Bind();
		RenderCommand::DispatchCompute(64 / 4, 32 / 4, 32 / 4);

		sRendererData->LocalExposureGrid->UnbindUAV(0, D3D11_COMPUTE_SHADER);
		RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 0, nullSRV);

#ifdef TOAST_DEBUG
		if (annotation)
			annotation->EndEvent();
#endif
	}

	void Renderer::SunVisibilityPass(Ref<Planet>& planet)
	{
		TOAST_PROFILE_FUNCTION();

#ifdef TOAST_DEBUG
		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation)
			annotation->BeginEvent(L"Sun Visibility Pass");
#endif

		auto shader = AssetManager::GetAsset<Shader>(sRendererData->SunVisibilityCSShaderHandle);
		if (shader)
		{
			RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 0, sRendererData->DepthBuffer->GetSRV());
			RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 1, planet->GetTransmittanceLUT()->GetSRV());
			RenderCommand::BindSampler(D3D11_COMPUTE_SHADER, 0, SamplerStates::Get(SamplerType::LinearClamp));
			sRendererData->SunVisibilityBuffer->BindUAV(0);
			shader->Bind();

			sRendererData->CameraCBuffer->Bind();                 
			sRendererData->LightningCBuffer->Bind();              
			planet->GetPlanetFrameCBuffer()->Bind();              
			sRendererData->AtmosphereCBuffer->Bind();             
			sRendererData->SunDiscSettingsCBuffer->Bind();        

			RenderCommand::DispatchCompute(1, 1, 1);

			sRendererData->SunVisibilityBuffer->UnbindUAV(0);
			RenderCommand::ClearShaderResources();
		}

#ifdef TOAST_DEBUG
		if (annotation)
			annotation->EndEvent();
#endif
	}

	void Renderer::PostProcessPass(const bool bloom, Scene::Environment& environment, const Scene::ExposureParams& exposureParams, Ref<Planet>& planet, const DirectX::XMFLOAT4& cameraPos, DirectX::XMFLOAT3 worldOffsetWS)
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
		RenderCommand::SetRenderTargets({ sRendererData->FinalRT->GetRTV().Get(), sRendererData->FinalEditorRT->GetRTV().Get() }, nullptr);
		RenderCommand::ClearRenderTargets(sRendererData->FinalRT->GetRTV().Get(), {0.0f, 0.0f, 0.0f, 1.0f});
		RenderCommand::ClearRenderTargets(sRendererData->FinalEditorRT->GetRTV().Get(), { 0.0f, 0.0f, 0.0f, 1.0f });
		RenderCommand::SetBlendState(sRendererData->PostProcessBlendState, { 0.0f, 0.0f, 0.0f, 0.0f });

		sRendererData->StarsBuffer.Write((uint8_t*)&environment.StarNits, 4, 0);
		sRendererData->StarsCBuffer->Map(sRendererData->StarsBuffer);
		sRendererData->StarsCBuffer->Bind();

		RenderCommand::BindSampler(D3D11_PIXEL_SHADER, 0, SamplerStates::Get(SamplerType::LinearClamp));
		RenderCommand::BindSampler(D3D11_PIXEL_SHADER, 1, SamplerStates::Get(SamplerType::PointClamp));

		if (environment.StarburstTextureHandle != AssetHandle(0))
		{
			if (auto starburst = AssetManager::GetAsset<Texture2D>(environment.StarburstTextureHandle))
				RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 13, starburst->GetSRV());
		}

		LocalExposureGridPass(bloom ? sRendererData->FinalBloomRT->GetSRV() : sRendererData->AtmospherePassRT->GetSRV());

		sRendererData->TonemappingBuffer.Write((uint8_t*)&environment.StarburstSize, 4, 0);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&environment.StarburstIntensity, 4, 4);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&environment.GhostIntensity, 4, 8);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&environment.GhostScale, 4, 12);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&exposureParams.LocalHighlightContrast, 4, 16); 
		sRendererData->TonemappingBuffer.Write((uint8_t*)&exposureParams.LocalShadowContrast, 4, 20);    
		sRendererData->TonemappingBuffer.Write((uint8_t*)&exposureParams.LocalDetailStrength, 4, 24);
		sRendererData->TonemappingCBuffer->Map(sRendererData->TonemappingBuffer);
		sRendererData->TonemappingCBuffer->Bind();

		auto shader = AssetManager::GetAsset<Shader>(sRendererData->ToneMappingShaderHandle);
		if (shader)
			shader->Bind();

		if(bloom)
			RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 9, sRendererData->FinalBloomRT->GetSRV());
		else
			RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 9, sRendererData->AtmospherePassRT->GetSRV());

		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 10, sRendererData->StarsRT->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 11, sRendererData->LocalExposureGrid->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 12, sRendererData->SunVisibilityBuffer->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 14, sRendererData->SunDiscMaskRT->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 15, sRendererData->SunHaloMaskRT->GetSRV());
		auto exposureSRV = sRendererData->ExposureEVBuffer->GetSRV(); 
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 17, exposureSRV);

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

	void Renderer::OutlinePass(const Scene::OutlineSettings& outlineSettings, const DirectX::XMFLOAT4& hoverTint)
	{
		bool hasSelected = !sRendererData->MeshSelectedDrawList.empty();
		bool hasHovered = !sRendererData->MeshHoveredDrawList.empty() && hoverTint.w > 0.0f;  // alpha 0 = off

		if (!hasSelected && !hasHovered)
			return;

#ifdef TOAST_DEBUG
		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation) 
			annotation->BeginEvent(L"Outline/Highlight Pass");
#endif
		RenderCommand::SetRasterizerState(sRendererData->NormalRasterizerState);

		if (hasSelected)
		{
			TOAST_PROFILE(*sRendererData->FrameProfiler, "Selected Mask");
			RenderCommand::SetRenderTargets({ sRendererData->SelectedMeshMaskRT->GetRTV().Get() }, sRendererData->DepthStencilView);
			RenderCommand::ClearRenderTargets(sRendererData->SelectedMeshMaskRT->GetRTV().Get(), { 0.0f, 0.0f, 0.0f, 1.0f });

			auto shader = AssetManager::GetAsset<Shader>(sRendererData->ObjectMaskShaderHandle);
			if (shader)
				shader->Bind();

			sRendererData->CurrentMesh = nullptr;

			for (const auto& meshCommand : sRendererData->MeshSelectedDrawList)
			{
				const Submesh& submesh = meshCommand.Mesh->mLODGroups[meshCommand.LODIndex]->Submeshes[meshCommand.SubmeshIndex];

				sRendererData->ModelBuffer.Write((uint8_t*)&meshCommand.Transform, 64, 0);
				sRendererData->ModelCBuffer->Map(sRendererData->ModelBuffer);

				if (sRendererData->CurrentMesh != meshCommand.Mesh.get())
				{
					meshCommand.Mesh->Bind(meshCommand.LODIndex);
					sRendererData->CurrentMesh = meshCommand.Mesh.get();
				}
				RenderCommand::DrawIndexed(0, submesh.BaseIndex, submesh.IndexCount);
			}
		}

		if (hasHovered)
		{
			TOAST_PROFILE(*sRendererData->FrameProfiler, "Hovered Mask");
			RenderCommand::SetRenderTargets({ sRendererData->HoveredMeshMaskRT->GetRTV().Get() }, sRendererData->DepthStencilView);
			RenderCommand::ClearRenderTargets(sRendererData->HoveredMeshMaskRT->GetRTV().Get(), { 0.0f, 0.0f, 0.0f, 1.0f });

			auto shader = AssetManager::GetAsset<Shader>(sRendererData->ObjectMaskShaderHandle);
			if (shader)
				shader->Bind();

			sRendererData->CurrentMesh = nullptr;
			for (const auto& meshCommand : sRendererData->MeshHoveredDrawList)
			{
				const Submesh& submesh = meshCommand.Mesh->mLODGroups[meshCommand.LODIndex]->Submeshes[meshCommand.SubmeshIndex];
				sRendererData->ModelBuffer.Write((uint8_t*)&meshCommand.Transform, 64, 0);
				sRendererData->ModelCBuffer->Map(sRendererData->ModelBuffer);
				if (sRendererData->CurrentMesh != meshCommand.Mesh.get())
				{
					meshCommand.Mesh->Bind(meshCommand.LODIndex);
					sRendererData->CurrentMesh = meshCommand.Mesh.get();
				}
				RenderCommand::DrawIndexed(0, submesh.BaseIndex, submesh.IndexCount);
			}
		}

		{
			TOAST_PROFILE(*sRendererData->FrameProfiler, "Outline Composite");
			RenderCommand::SetBlendState(sRendererData->UIBlendState, { 0.0f, 0.0f, 0.0f, 0.0f });
			RenderCommand::SetRenderTargets({ sRendererData->FinalRT->GetRTV().Get(), sRendererData->FinalEditorRT->GetRTV().Get() }, sRendererData->DepthStencilView);
			RenderCommand::SetDepthStencilState(sRendererData->DepthDisabledStencilState);

			if (hasSelected)
			{
				sRendererData->OutlineBuffer.Write((uint8_t*)&outlineSettings.Color, 16, 0);
				sRendererData->OutlineBuffer.Write((uint8_t*)&outlineSettings.Thickness, 4, 16);
				sRendererData->OutlineBuffer.Write((uint8_t*)&outlineSettings.Softness, 4, 20);
				sRendererData->OutlineCBuffer->Map(sRendererData->OutlineBuffer);
				sRendererData->OutlineCBuffer->Bind();

				auto shader = AssetManager::GetAsset<Shader>(sRendererData->OutlineShaderHandle);
				if (shader)
					shader->Bind();

				RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 11, sRendererData->SelectedMeshMaskRT->GetSRV());
				Renderer::DrawFullscreenQuad();
			}

			if (hasHovered)
			{
				sRendererData->HoverTintBuffer.Write((uint8_t*)&hoverTint, 16, 0);
				sRendererData->HoverTintCBuffer->Map(sRendererData->HoverTintBuffer);
				sRendererData->HoverTintCBuffer->Bind();

				auto shader = AssetManager::GetAsset<Shader>(sRendererData->HoverTintShaderHandle);
				if (shader)
					shader->Bind();

				RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 11, sRendererData->HoveredMeshMaskRT->GetSRV());
				Renderer::DrawFullscreenQuad();
				RenderCommand::ClearShaderResources();
			}

			RenderCommand::ClearShaderResources();
		}

#ifdef TOAST_DEBUG
		if (annotation)
			annotation->EndEvent();
#endif
	}

	void Renderer::GuidancePass(Vector3 worldTranslation)
	{
		if (sRendererData->MoveMarkerDrawList.empty())
		{
			sRendererData->MoveMarkerDrawList.clear();   // safety in case of empty-but-non-cleared
			return;
		}

#ifdef TOAST_DEBUG
		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation) annotation->BeginEvent(L"Guidance Pass");
#endif

		RenderCommand::SetBlendState(sRendererData->UIBlendState, { 0.0f, 0.0f, 0.0f, 0.0f });
		RenderCommand::SetRenderTargets({ sRendererData->FinalRT->GetRTV().Get(), sRendererData->FinalEditorRT->GetRTV().Get() }, sRendererData->DepthStencilView);
		RenderCommand::SetDepthStencilState(sRendererData->ParticleDepthStencilState);

		auto shader = AssetManager::GetAsset<Shader>(sRendererData->GuidanceMarkerShaderHandle);
		if (shader)
			shader->Bind();

		for (const auto& marker : sRendererData->MoveMarkerDrawList)
		{
			// Position lifted slightly along the surface normal to avoid z-fighting with terrain.
			Vector3 pos = marker.TargetWorldPos + marker.TargetSurfaceNormal * 0.05;
			Vector3 up = Vector3::Normalize(marker.TargetSurfaceNormal);
			Vector3 ref = (fabs(Vector3::Dot(up, Vector3(0.0, 1.0, 0.0))) > 0.99) ? Vector3(1.0, 0.0, 0.0) : Vector3(0.0, 1.0, 0.0);
			Vector3 right = Vector3::Normalize(Vector3::Cross(ref, up));
			Vector3 fwd = Vector3::Cross(up, right);

			float scale = marker.Size;

			DirectX::XMMATRIX worldMatrix = DirectX::XMMatrixSet(
				(float)(right.x * scale), (float)(right.y * scale), (float)(right.z * scale), 0,
				(float)(up.x * scale), (float)(up.y * scale), (float)(up.z * scale), 0,
				(float)(fwd.x * scale), (float)(fwd.y * scale), (float)(fwd.z * scale), 0,
				(float)pos.x, (float)pos.y, (float)pos.z, 1);

			sRendererData->ModelBuffer.Write((uint8_t*)&worldMatrix, 64, 0);
			sRendererData->ModelCBuffer->Map(sRendererData->ModelBuffer);

			// Alpha-only cbuffer (color removed; texture provides RGB)
			const float rotationSpeed = 0.5f;
			float time = sRendererData->ElapsedTime;
			DirectX::XMFLOAT4 markerParams = { marker.Alpha, time, rotationSpeed, 0.0f };
			sRendererData->MarkerBuffer.Write((uint8_t*)&markerParams, 16, 0);
			sRendererData->MarkerCBuffer->Map(sRendererData->MarkerBuffer);
			sRendererData->MarkerCBuffer->Bind();

			RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 0, AssetManager::GetAsset<Texture2D>(marker.Texture)->GetSRV());
			Renderer::DrawQuad();
		}

		RenderCommand::SetBlendState(nullptr);
		sRendererData->MoveMarkerDrawList.clear();   // drain so next frame starts empty (the silent-bug pattern from outline pass)

#ifdef TOAST_DEBUG
		if (annotation) annotation->EndEvent();
#endif
	}

	void Renderer::LoadEngineShaders()
	{
		static const char* kShaderPaths[] = {
			// Deferred Rendering
			"assets/shaders/Rendering/GeometryPass.hlsl",
			"assets/shaders/Rendering/ShadowPass.hlsl",
			"assets/shaders/Rendering/TerrainObjectShadowPass.hlsl",
			"assets/shaders/Rendering/SSAOPass.hlsl",
			"assets/shaders/Rendering/SSAOBlurPass.hlsl",
			"assets/shaders/Rendering/LightningPass.hlsl",
			"assets/shaders/Rendering/Particles.hlsl",
			"assets/shaders/Rendering/ParticleEmit.hlsl",
			"assets/shaders/Rendering/ParticleSimKickoff.hlsl",
			"assets/shaders/Rendering/ParticleSimulate.hlsl",
			"assets/shaders/Rendering/ParticleFinalize.hlsl",
			"assets/shaders/Rendering/HoverTint.hlsl",
			"assets/shaders/Rendering/GuidanceMarker.hlsl",
			"assets/shaders/Rendering/TerrainScatterKickoff.hlsl",
			"assets/shaders/Rendering/TerrainScatter.hlsl",
			"assets/shaders/Debug/ObjectMask.hlsl",
			"assets/shaders/Debug/Outline.hlsl",

			// Planet
			"assets/shaders/Planet/PlanetGeometryPass.hlsl",
			"assets/shaders/Planet/PlanetShadowPass.hlsl",
			"assets/shaders/Planet/HeightMapToCubeMap.hlsl",
			"assets/shaders/Planet/HeightCubeToNormalCube.hlsl",
			"assets/shaders/Planet/HeightCubeMaxMip.hlsl",
			"assets/shaders/Planet/AlbedoMapToCube.hlsl",
			"assets/shaders/Planet/Atmosphere/TransmittanceCS.hlsl",
			"assets/shaders/Planet/Atmosphere/MultiScatteringCS.hlsl",
			"assets/shaders/Planet/Atmosphere/GroundIrradianceCS.hlsl",
			"assets/shaders/Planet/Atmosphere/SkyViewCS.hlsl",
			"assets/shaders/Planet/Atmosphere/AerialPerspectiveCS.hlsl",
			"assets/shaders/Planet/Atmosphere/APFarDynamic.hlsl",
			"assets/shaders/Planet/Atmosphere/ShadowHeightMapCS.hlsl",
			"assets/shaders/Planet/PlanetMeshBake.hlsl",

			// Post Processes
			"assets/shaders/Post Process/StarField.hlsl",
			"assets/shaders/Post Process/Atmosphere.hlsl",
			"assets/shaders/Post Process/BloomMipDown.hlsl",
			"assets/shaders/Post Process/BloomMipUp.hlsl",
			"assets/shaders/Post Process/BloomBlend.hlsl",
			"assets/shaders/Post Process/ToneMapping.hlsl",
			"assets/shaders/Post Process/ExposureAdapt.hlsl",
			"assets/shaders/Post Process/LocalExposureGrid.hlsl",
			"assets/shaders/Post Process/LocalExposureBlur.hlsl",
			"assets/shaders/Post Process/SunVisibilityCS.hlsl",

			// Environment
			"assets/shaders/Environment/EnvironmentMipFilter.hlsl",
			"assets/shaders/Environment/EnvironmentIrradiance.hlsl",
			"assets/shaders/Environment/SPBRDF.hlsl",

			// Others
			"assets/shaders/Standard.hlsl",
			"assets/shaders/UI.hlsl",
			"assets/shaders/Utilities/Copy.hlsl",
		};

		for (const char* path : kShaderPaths)
		{
			AssetHandle handle = AssetManager::ImportExternalAsset(path, "Shaders", true);
			TOAST_CORE_ASSERT(handle, "Engine shader import failed: %s", path);

			std::string name = std::filesystem::path(path).stem().string();
			AssetManager::RegisterEngineShader(name, handle);
		}

		// Setting up shader handles that the Renderer uses
		sRendererData->TransmittanceCSShaderHandle = AssetManager::GetEngineShaderHandle("TransmittanceCS");
		sRendererData->MultiScatteringCSShaderHandle = AssetManager::GetEngineShaderHandle("MultiScatteringCS");
		sRendererData->GroundIrradianceCSShaderHandle = AssetManager::GetEngineShaderHandle("GroundIrradianceCS");
		sRendererData->PlanetGeometryPassShaderHandle = AssetManager::GetEngineShaderHandle("PlanetGeometryPass");
		sRendererData->PlanetShadowPassShaderHandle = AssetManager::GetEngineShaderHandle("PlanetShadowPass");
		sRendererData->TerrainObjectShadowPassShaderHandle = AssetManager::GetEngineShaderHandle("TerrainObjectShadowPass");
		sRendererData->GeometryPassShaderHandle = AssetManager::GetEngineShaderHandle("GeometryPass");
		sRendererData->ShadowPassShaderHandle = AssetManager::GetEngineShaderHandle("ShadowPass");
		sRendererData->SSAOPassShaderHandle = AssetManager::GetEngineShaderHandle("SSAOPass");
		sRendererData->SSAOBlurPassShaderHandle = AssetManager::GetEngineShaderHandle("SSAOBlurPass");
		sRendererData->LightningPassShaderHandle = AssetManager::GetEngineShaderHandle("LightningPass");
		sRendererData->StarFieldShaderHandle = AssetManager::GetEngineShaderHandle("StarField");
		sRendererData->APFarDynamicShaderHandle = AssetManager::GetEngineShaderHandle("APFarDynamic");
		sRendererData->SkyViewCSShaderHandle = AssetManager::GetEngineShaderHandle("SkyViewCS");
		sRendererData->AerialPerspectiveCSShaderHandle = AssetManager::GetEngineShaderHandle("AerialPerspectiveCS");
		sRendererData->ShadowHeightMapCSShaderHandle = AssetManager::GetEngineShaderHandle("ShadowHeightMapCS");
		sRendererData->AtmosphereShaderHandle = AssetManager::GetEngineShaderHandle("Atmosphere");
		sRendererData->ParticlesShaderHandle = AssetManager::GetEngineShaderHandle("Particles");
		sRendererData->ParticlesEmitShaderHandle = AssetManager::GetEngineShaderHandle("ParticleEmit");
		sRendererData->ParticlesSimKickoffShaderHandle = AssetManager::GetEngineShaderHandle("ParticleSimKickoff");
		sRendererData->ParticlesSimulateShaderHandle = AssetManager::GetEngineShaderHandle("ParticleSimulate");
		sRendererData->ParticlesFinalizeShaderHandle = AssetManager::GetEngineShaderHandle("ParticleFinalize");
		sRendererData->Particles->LoadShaders();
		sRendererData->TerrainObjects->LoadShaders();
		sRendererData->BloomMipDownShaderHandle = AssetManager::GetEngineShaderHandle("BloomMipDown");
		sRendererData->BloomMipUpShaderHandle = AssetManager::GetEngineShaderHandle("BloomMipUp");
		sRendererData->BloomBlendShaderHandle = AssetManager::GetEngineShaderHandle("BloomBlend");
		sRendererData->ExposureAdaptShaderHandle = AssetManager::GetEngineShaderHandle("ExposureAdapt");
		sRendererData->LocalExposureGridShaderHandle = AssetManager::GetEngineShaderHandle("LocalExposureGrid");
		sRendererData->LocalExposureBlurShaderHandle = AssetManager::GetEngineShaderHandle("LocalExposureBlur");
		sRendererData->SunVisibilityCSShaderHandle = AssetManager::GetEngineShaderHandle("SunVisibilityCS");
		sRendererData->ToneMappingShaderHandle = AssetManager::GetEngineShaderHandle("ToneMapping");
		sRendererData->ObjectMaskShaderHandle = AssetManager::GetEngineShaderHandle("ObjectMask");
		sRendererData->OutlineShaderHandle = AssetManager::GetEngineShaderHandle("Outline");
		sRendererData->HoverTintShaderHandle = AssetManager::GetEngineShaderHandle("HoverTint");
		sRendererData->GuidanceMarkerShaderHandle = AssetManager::GetEngineShaderHandle("GuidanceMarker");
		sRendererData->SPBRDFShaderHandle = AssetManager::GetEngineShaderHandle("SPBRDF");
		sRendererData->EnvironmentMipFilterShaderHandle = AssetManager::GetEngineShaderHandle("EnvironmentMipFilter");
		sRendererData->EnvironmentIrradianceShaderHandle = AssetManager::GetEngineShaderHandle("EnvironmentIrradiance");
		sRendererData->UIShaderHandle = AssetManager::GetEngineShaderHandle("UI");
	}

	void Renderer::GenerateSpecularBRDF()
	{
		sRendererData->SpecularBRDFLUT = CreateRef<Texture2D>(DXGI_FORMAT_R16G16_FLOAT, DXGI_FORMAT_R16G16_FLOAT, 256, 256);

		sRendererData->SpecularBRDFLUT->CreateUAV(0);

		sRendererData->SpecularBRDFLUT->BindForReadWrite(0, D3D11_COMPUTE_SHADER);

		auto shader = AssetManager::GetAsset<Shader>(sRendererData->SPBRDFShaderHandle);
		if (shader)
			shader->Bind();

		sRendererData->SpecularMapFilterSettingsCBuffer->Bind();

		RenderCommand::DispatchCompute(sRendererData->SpecularBRDFLUT->GetWidth() / 32, sRendererData->SpecularBRDFLUT->GetHeight() / 32, 1);
		sRendererData->SpecularBRDFLUT->UnbindUAV();
	}

	void Renderer::UpdateEnvMapSource(Texture* atmosphereCube, Ref<TextureCube> envMapSource, int faceIndex)
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		const uint32_t srcSubresource = D3D11CalcSubresource(0, faceIndex, atmosphereCube->GetMipLevelCount());
		const uint32_t dstSubresource = D3D11CalcSubresource(0, faceIndex, envMapSource->GetMipLevelCount());

		deviceContext->CopySubresourceRegion(envMapSource->GetResource(), dstSubresource, 0, 0, 0, envMapSource->GetResource(), srcSubresource, nullptr);

		envMapSource->GenerateMips();
	}

	void Renderer::GeneratePrefilteredEnvMap(Texture* sourceTexture, Ref<TextureCube> targetTexture, int faceIndex)
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		auto shader = AssetManager::GetAsset<Shader>(sRendererData->EnvironmentMipFilterShaderHandle);
		if (shader)
			shader->Bind();

		sRendererData->SpecularMapFilterSettingsCBuffer->Bind();

		// Bind the atmospheric scattering cube map as input (unfiltered environment map)
		RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 14, sourceTexture->GetSRV());

		// Bind the sampler
		RenderCommand::BindSampler(D3D11_COMPUTE_SHADER, 0, SamplerStates::Get(SamplerType::Default));

		const uint32_t srcW = sourceTexture->GetWidth();
		const uint32_t srcH = sourceTexture->GetHeight();
		const uint32_t dstW = targetTexture->GetWidth();
		const uint32_t dstH = targetTexture->GetHeight();

		const bool canCopyMip0 = (srcW == dstW) && (srcH == dstH);

		if (canCopyMip0)
		{
			// Calculate source and destination sub resource indices
			const uint32_t srcMipLevels = sourceTexture->GetMipLevelCount(); // 1
			const uint32_t srcSubresourceIndex = D3D11CalcSubresource(0, faceIndex, srcMipLevels); // faceIndex

			const uint32_t destMipLevels = targetTexture->GetMipLevelCount(); // 9
			const uint32_t destSubresourceIndex = D3D11CalcSubresource(0, faceIndex, destMipLevels); // faceIndex * 9

			const uint32_t subresourceIndex = D3D11CalcSubresource(0, faceIndex, srcMipLevels);
			// Perform the copy operation from AtmosphereCubeRT to EnvMapFiltered
			deviceContext->CopySubresourceRegion(targetTexture->GetResource(), destSubresourceIndex, // Destination sub resource
				0, 0, 0, // Destination X, Y, Z
				sourceTexture->GetResource(), // Source resource
				srcSubresourceIndex, // Source sub resource index
				nullptr // Source box
			);
		}

		// Pre-filter the rest of the mip chain for the current face
		const float deltaRoughness = 1.0f / std::max(float(targetTexture->GetMipLevelCount() - 1.0f), 1.0f);
		const uint32_t mipCount = targetTexture->GetMipLevelCount();

		const int firstMip = canCopyMip0 ? 1 : 0;

		for (int mipLevel = firstMip; mipLevel < (int)mipCount; ++mipLevel)
		{
			uint32_t size = targetTexture->GetWidth() / (1u << mipLevel);
			const int numGroups = std::max(1, int((size + 7) / 8));

			const float roughness = float(mipLevel) * deltaRoughness;
			sRendererData->SpecularMapFilterSettingsBuffer.Write((uint8_t*)&roughness, sizeof(float), 0);
			sRendererData->SpecularMapFilterSettingsBuffer.Write((uint8_t*)&faceIndex, 4, 4);
			sRendererData->SpecularMapFilterSettingsCBuffer->Map(sRendererData->SpecularMapFilterSettingsBuffer);

			// Bind the filtered environment map for writing (current face and mip level)
			targetTexture->BindForReadWriteUpdated(0, D3D11_COMPUTE_SHADER, mipLevel, faceIndex);

			// Dispatch compute shader for the current face and mip level
			RenderCommand::DispatchCompute(numGroups, numGroups, 1); // Process one face at a time

			// Unbind UAV for this mip level
			targetTexture->UnbindUAV(0, D3D11_COMPUTE_SHADER);
		}

		// Unbind resources
		RenderCommand::ClearShaderResources();
	}

	void Renderer::GenerateIrradianceCubemap(Ref<TextureCube> sourceTexture, Ref<TextureCube> targetTexture, int faceIndex)
	{
		auto shader = AssetManager::GetAsset<Shader>(sRendererData->EnvironmentIrradianceShaderHandle);
		if (shader)
			shader->Bind();

		sRendererData->SpecularMapFilterSettingsBuffer.Write((uint8_t*)&faceIndex, 4, 4);
		sRendererData->SpecularMapFilterSettingsCBuffer->Map(sRendererData->SpecularMapFilterSettingsBuffer);
		sRendererData->SpecularMapFilterSettingsCBuffer->Bind();

		RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 15, sourceTexture->GetSRV());
		RenderCommand::BindSampler(D3D11_COMPUTE_SHADER, 0, SamplerStates::Get(SamplerType::Default));

		targetTexture->BindForReadWriteUpdated(0, D3D11_COMPUTE_SHADER, 0, faceIndex);

		const uint32_t groupsX = (targetTexture->GetWidth() + 7) / 8;
		const uint32_t groupsY = (targetTexture->GetHeight() + 7) / 8;
		RenderCommand::DispatchCompute(groupsX, groupsY, 1);

		RenderCommand::ClearShaderResources();

		targetTexture->UnbindUAVUpdated(0, D3D11_COMPUTE_SHADER);
	}

	void Renderer::GenerateTransmittanceLUT(Planet* planet)
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

#ifdef TOAST_DEBUG
		RENDERDOC_API_1_4_1* rdoc = nullptr;
		if (auto mod = GetModuleHandleA("renderdoc.dll"))
		{
			pRENDERDOC_GetAPI getApi = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
			if (getApi)
				getApi(eRENDERDOC_API_Version_1_4_1, (void**)&rdoc);
		}

		if (rdoc)
			rdoc->StartFrameCapture((void*)device, nullptr);

		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation)
			annotation->BeginEvent(L"GenerateTransmittanceLUT");
#endif

		// Updating the planet data in the buffer and mapping it to the GPU
		auto& planetFrameBuffer = planet->GetPlanetFrameBuffer();
		auto& planetFrameCBuffer = planet->GetPlanetFrameCBuffer();
		DirectX::XMFLOAT3 planetCenter = DirectX::XMFLOAT3((float)planet->GetTranslation().x, (float)planet->GetTranslation().y, (float)planet->GetTranslation().z);
		float planetRadius = (float)planet->GetRadius();
		float MaxHeight = (float)planet->GetMaxHeight();
		float MinHeight = (float)planet->GetMinHeight();
		planetFrameBuffer.Write((uint8_t*)&planetCenter, 12, 0);
		planetFrameBuffer.Write((uint8_t*)&planetRadius, 4, 12);
		planetFrameBuffer.Write((uint8_t*)&MaxHeight, 4, 28);
		planetFrameBuffer.Write((uint8_t*)&MinHeight, 4, 44);
		planetFrameCBuffer->Map(planetFrameBuffer);
		planetFrameCBuffer->Bind();

		// Updating the atmospheric data in the buffer and mapping it to the GPU
		UploadAtmosphereCBuffer(planet, 0.0f);

		planet->GetTransmittanceLUT()->BindForReadWrite(0, D3D11_COMPUTE_SHADER);

		auto shader = AssetManager::GetAsset<Shader>(sRendererData->TransmittanceCSShaderHandle);
		if (shader)
			shader->Bind();

		const UINT width = planet->GetTransmittanceLUT()->GetWidth();
		const UINT height = planet->GetTransmittanceLUT()->GetHeight();
		const UINT gx = (width + 7) / 8;
		const UINT gy = (height + 7) / 8;
		RenderCommand::DispatchCompute(gx, gy, 1);

		planet->GetTransmittanceLUT()->UnbindUAV(0, D3D11_COMPUTE_SHADER);

		ID3D11RenderTargetView* nullRTV = nullptr;
		RenderCommand::SetRenderTargets({ nullRTV }, nullptr);
		RenderCommand::SetDepthStencilState(nullptr);
		RenderCommand::SetBlendState(nullptr);
		RenderCommand::ClearShaderResources();
#ifdef TOAST_DEBUG
		if (annotation)
			annotation->EndEvent();

		if (rdoc)
			rdoc->EndFrameCapture((void*)device, nullptr);
#endif
	}

	void Renderer::GenerateMultiScatteringLUT(Planet* planet)
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

#ifdef TOAST_DEBUG
		RENDERDOC_API_1_4_1* rdoc = nullptr;
		if (auto mod = GetModuleHandleA("renderdoc.dll"))
		{
			pRENDERDOC_GetAPI getApi = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
			if (getApi)
				getApi(eRENDERDOC_API_Version_1_4_1, (void**)&rdoc);
		}

		if (rdoc)
			rdoc->StartFrameCapture((void*)device, nullptr);

		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation)
			annotation->BeginEvent(L"GenerateMultiScatteringLUT");
#endif

		// Updating the planet data in the buffer and mapping it to the GPU
		auto& planetFrameBuffer = planet->GetPlanetFrameBuffer();
		auto& planetFrameCBuffer = planet->GetPlanetFrameCBuffer();
		DirectX::XMFLOAT3 planetCenter = DirectX::XMFLOAT3((float)planet->GetTranslation().x, (float)planet->GetTranslation().y, (float)planet->GetTranslation().z);
		float planetRadius = (float)planet->GetRadius();
		float MaxHeight = (float)planet->GetMaxHeight();
		float MinHeight = (float)planet->GetMinHeight();
		planetFrameBuffer.Write((uint8_t*)&planetCenter, 12, 0);
		planetFrameBuffer.Write((uint8_t*)&planetRadius, 4, 12);
		planetFrameBuffer.Write((uint8_t*)&MaxHeight, 4, 28);
		planetFrameBuffer.Write((uint8_t*)&MinHeight, 4, 44);
		planetFrameCBuffer->Map(planetFrameBuffer);
		planetFrameCBuffer->Bind();

		// Updating the atmospheric data in the buffer and mapping it to the GPU
		UploadAtmosphereCBuffer(planet, 0.0f);

		RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 0, planet->GetTransmittanceLUT()->GetSRV());
		RenderCommand::BindSampler(D3D11_COMPUTE_SHADER, 0, SamplerStates::Get(SamplerType::LinearClamp));

		planet->GetMultiScatteringLUT()->BindForReadWrite(0, D3D11_COMPUTE_SHADER);

		auto shader = AssetManager::GetAsset<Shader>(sRendererData->MultiScatteringCSShaderHandle);
		if (shader)
			shader->Bind();

		const UINT width = planet->GetMultiScatteringLUT()->GetWidth();
		const UINT height = planet->GetMultiScatteringLUT()->GetHeight();
		const UINT gx = (width + 7) / 8;
		const UINT gy = (height + 7) / 8;
		RenderCommand::DispatchCompute(gx, gy, 1);

		planet->GetMultiScatteringLUT()->UnbindUAV(0, D3D11_COMPUTE_SHADER);

		ID3D11RenderTargetView* nullRTV = nullptr;
		RenderCommand::SetRenderTargets({ nullRTV }, nullptr);
		RenderCommand::SetDepthStencilState(nullptr);
		RenderCommand::SetBlendState(nullptr);
		RenderCommand::ClearShaderResources();
#ifdef TOAST_DEBUG
		if (annotation)
			annotation->EndEvent();

		if (rdoc)
			rdoc->EndFrameCapture((void*)device, nullptr);
#endif
	}

	void Renderer::GenerateGroundIrradianceLUT(Planet* planet)
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		auto shader = AssetManager::GetAsset<Shader>(sRendererData->GroundIrradianceCSShaderHandle);
		if (!shader)
			return;

#ifdef TOAST_DEBUG
		RENDERDOC_API_1_4_1* rdoc = nullptr;
		if (auto mod = GetModuleHandleA("renderdoc.dll"))
		{
			pRENDERDOC_GetAPI getApi = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
			if (getApi)
				getApi(eRENDERDOC_API_Version_1_4_1, (void**)&rdoc);
		}

		if (rdoc)
			rdoc->StartFrameCapture((void*)device, nullptr);

		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation)
			annotation->BeginEvent(L"GenerateGroundIrradianceLUT");
#endif

		// Planet data (same as the multi-scattering LUT)
		auto& planetFrameBuffer = planet->GetPlanetFrameBuffer();
		auto& planetFrameCBuffer = planet->GetPlanetFrameCBuffer();
		DirectX::XMFLOAT3 planetCenter = DirectX::XMFLOAT3((float)planet->GetTranslation().x, (float)planet->GetTranslation().y, (float)planet->GetTranslation().z);
		float planetRadius = (float)planet->GetRadius();
		float maxHeight = (float)planet->GetMaxHeight();
		float minHeight = (float)planet->GetMinHeight();
		planetFrameBuffer.Write((uint8_t*)&planetCenter, 12, 0);
		planetFrameBuffer.Write((uint8_t*)&planetRadius, 4, 12);
		planetFrameBuffer.Write((uint8_t*)&maxHeight, 4, 28);
		planetFrameBuffer.Write((uint8_t*)&minHeight, 4, 44);
		planetFrameCBuffer->Map(planetFrameBuffer);
		planetFrameCBuffer->Bind();

		UploadAtmosphereCBuffer(planet, 0.0f);

		RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 0, planet->GetTransmittanceLUT()->GetSRV());
		RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 1, planet->GetMultiScatteringLUT()->GetSRV());
		RenderCommand::BindSampler(D3D11_COMPUTE_SHADER, 0, SamplerStates::Get(SamplerType::LinearClamp));
		planet->GetGroundIrradianceLUT()->BindForReadWrite(0, D3D11_COMPUTE_SHADER);

		shader->Bind();

		const UINT width = planet->GetGroundIrradianceLUT()->GetWidth();
		const UINT height = planet->GetGroundIrradianceLUT()->GetHeight();
		RenderCommand::DispatchCompute((width + 7) / 8, (height + 7) / 8, 1);

		planet->GetGroundIrradianceLUT()->UnbindUAV(0, D3D11_COMPUTE_SHADER);
		RenderCommand::ClearShaderResources();

#ifdef TOAST_DEBUG
		if (annotation)
			annotation->EndEvent();

		if (rdoc)
			rdoc->EndFrameCapture((void*)device, nullptr);
#endif
	}

	DirectX::XMFLOAT3 Renderer::SampleSSAONoiseTexture(uint32_t x, uint32_t y)
	{
		const uint32_t NOISE_DIM = 16;  // same dimension as your SSAO noise texture
		x = x % NOISE_DIM;
		y = y % NOISE_DIM;

		const DirectX::XMFLOAT4& sample = sRendererData->SSAONoiseCPU[y * NOISE_DIM + x];
		return { sample.x, sample.y, sample.z };
	}

	void Renderer::CreateParticleIndexBuffer()
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		uint16_t indices[] = { 0, 1, 2, 2, 1, 3 };

		D3D11_BUFFER_DESC indexBufferDesc = {};
		indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		indexBufferDesc.ByteWidth = sizeof(indices);
		indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

		D3D11_SUBRESOURCE_DATA indexData = { indices, 0, 0 };
		device->CreateBuffer(&indexBufferDesc, &indexData, &sRendererData->ParticleIndexBuffer);
	}

	FrameProfiler& Renderer::GetFrameProfiler()
	{
		return *sRendererData->FrameProfiler;
	}

	Ref<ParticleSystem> Renderer::GetParticleSystem()
	{
		return sRendererData->Particles;
	}

	void Renderer::UploadCameraCBuffer(Camera& camera, const DirectX::XMFLOAT4 cameraPos)
	{
		sRendererData->CameraBuffer.Write((uint8_t*)&camera.GetWorldTranslationMatrix(), 64, 0);
		sRendererData->CameraBuffer.Write((uint8_t*)&camera.GetViewMatrix(), 64, 64);
		sRendererData->CameraBuffer.Write((uint8_t*)&camera.GetProjection(), 64, 128);
		sRendererData->CameraBuffer.Write((uint8_t*)&camera.GetInvViewMatrix(), 64, 192);
		sRendererData->CameraBuffer.Write((uint8_t*)&camera.GetInvProjection(), 64, 256);
		sRendererData->CameraBuffer.Write((uint8_t*)&cameraPos.x, 16, 320);
		sRendererData->CameraBuffer.Write((uint8_t*)&camera.GetFarClip(), 4, 336);
		sRendererData->CameraBuffer.Write((uint8_t*)&camera.GetNearClip(), 4, 340);
		sRendererData->CameraBuffer.Write((uint8_t*)&sRendererData->Viewport.Width, 4, 344);
		sRendererData->CameraBuffer.Write((uint8_t*)&sRendererData->Viewport.Height, 4, 348);
		sRendererData->CameraCBuffer->Map(sRendererData->CameraBuffer);
	}

	void Renderer::UploadAtmosphereCBuffer(Planet* planet, float bakeIBL)
	{
		auto& atmosphere = planet->GetAtmosphere();

		AtmosphereCBufferGPU data = {};
		data.AtmosphereHeight = atmosphere.AtmosphereHeight;
		data.RayleighScaleHeight = atmosphere.RayleighScaleHeight;
		data.MieScaleHeight = atmosphere.MieScaleHeight;
		data.RayleighScattering = atmosphere.RayleighScattering;
		data.MieScattering = atmosphere.MieScattering;
		data.MieAbsorption = atmosphere.MieAbsorption;
		data.GroundAlbedo = atmosphere.GroundAlbedo;
		data.FaceIndex = 0;
		data.MieAnisotropy = atmosphere.MieAnisotropy;
		data.OzoneStrength = atmosphere.OzoneStrength;
		data.StepsTransmittance = atmosphere.StepsTransmittance;
		data.StepsMultiScattering = atmosphere.StepsMultiScattering;
		data.BakeIBL = bakeIBL;
		data.MieLobeWeight = atmosphere.MieLobeWeight;
		data.MieAnisotropyBroad = atmosphere.MieAnisotropyBroad;

		sRendererData->AtmosphereBuffer.Write((uint8_t*)&data, sizeof(data), 0);
		sRendererData->AtmosphereCBuffer->Map(sRendererData->AtmosphereBuffer);
		sRendererData->AtmosphereCBuffer->Bind();
	}

	void Renderer::BindPlanetTerrainResources(bool bindPixelSRVs, bool bindComputeSRVs)
	{
		if(bindComputeSRVs && sRendererData->PlanetDraw.Planet->GetHeightMapCubeTexture())
			RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 0, sRendererData->PlanetDraw.Planet->GetHeightMapCubeTexture()->GetSRV());

		if (sRendererData->PlanetDraw.Planet->GetNumMaterials() > 0 && sRendererData->PlanetDraw.Planet->GetMaterialSB() && sRendererData->PlanetDraw.Planet->GetMaterialNoiseSB() && sRendererData->PlanetDraw.Planet->GetMaterialNoisePermSB())
		{

			if (bindPixelSRVs)
			{
				RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 1, sRendererData->PlanetDraw.Planet->GetMaterialSB()->GetSRV());
				RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 4, sRendererData->PlanetDraw.Planet->GetMaterialNoisePermSB()->GetSRV());
			}

			if (bindComputeSRVs)
			{
				RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 1, sRendererData->PlanetDraw.Planet->GetMaterialSB()->GetSRV());
				RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 2, sRendererData->PlanetDraw.Planet->GetMaterialNoiseSB()->GetSRV());
				RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 3, sRendererData->PlanetDraw.Planet->GetMaterialNoisePermSB()->GetSRV());
				RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 4, sRendererData->PlanetDraw.Planet->GetAlbedoCubeTexture()->GetSRV());
			}
		}

		if (bindPixelSRVs && sRendererData->PlanetDraw.Planet->GetNormalMapCubeTexture())
			RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 2, sRendererData->PlanetDraw.Planet->GetNormalMapCubeTexture()->GetSRV());

		if (sRendererData->PlanetDraw.Planet->GetUseAlbedoMap())
		{
			if (bindPixelSRVs && sRendererData->PlanetDraw.Planet->GetAlbedoCubeTexture())
				RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 3, sRendererData->PlanetDraw.Planet->GetAlbedoCubeTexture()->GetSRV());
		}

		if (sRendererData->PlanetDraw.Planet->GetPBRAlbedoArray())
		{
			if (bindPixelSRVs)
			{
				RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 6, sRendererData->PlanetDraw.Planet->GetPBRAlbedoArray()->GetSRV());
				RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 7, sRendererData->PlanetDraw.Planet->GetPBRNormalArray()->GetSRV());
				RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 8, sRendererData->PlanetDraw.Planet->GetPBRRoughnessArray()->GetSRV());
				RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 9, sRendererData->PlanetDraw.Planet->GetPBRAOArray()->GetSRV());
				RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 10, sRendererData->PlanetDraw.Planet->GetPBRDisplacementArray()->GetSRV());
			}
		}

		// Cbuffers
		sRendererData->PlanetDraw.Planet->GetPlanetRenderingSettingsCBuffer()->Bind();
		sRendererData->PlanetDraw.Planet->GetPlanetFrameCBuffer()->Bind();
	}

	void Renderer::DrawTerrainObjects(Planet* planet)
	{
		const auto& objects = planet->GetTerrainObjects();
		if (objects.empty())
			return;

		// Two different things with similar names:
		//   objects      - the planet's TerrainObject config (WHAT to scatter),
		//                  edited from the PlanetPanel.
		//   objectSystem - the GPU side (WHERE this frame's instances are):
		//                  per-type instance buffers + indirect args.
		auto& objectSystem = sRendererData->TerrainObjects;

		// ModelCB: force instanced branch in your generic GPass VS
		{
			DirectX::XMMATRIX I = DirectX::XMMatrixIdentity();
			float clickable = 0.0f;
			int entityID = -1;
			int noWorldTransform = 1; // important: instanced path outputs camera-relative directly
			int isInstanced = 1;

			sRendererData->ModelBuffer.Write((uint8_t*)&I, 64, 0);
			sRendererData->ModelBuffer.Write((uint8_t*)&clickable, 4, 64);
			sRendererData->ModelBuffer.Write((uint8_t*)&entityID, 4, 68);
			sRendererData->ModelBuffer.Write((uint8_t*)&noWorldTransform, 4, 72);
			sRendererData->ModelBuffer.Write((uint8_t*)&isInstanced, 4, 76);
			sRendererData->ModelCBuffer->Map(sRendererData->ModelBuffer);
		}

		const uint32_t typeCount = std::min((uint32_t)objects.size(), (uint32_t)MAX_TERRAIN_OBJECT_TYPES);

		for (uint32_t t = 0; t < typeCount; ++t)
		{
			const TerrainObject& object = objects[t];

			if (!object.MeshObject || object.CandidateGridSize == 0)
				continue;

			Ref <Material> material = AssetManager::GetAsset <Material>(object.MeshObject->GetSubmeshes(0)[0].MaterialHandle);
			if (!material)
			{
				TOAST_CORE_WARN("Renderer: material handle %llu failed to resolve, skipping submesh", (uint64_t)object.MeshObject->GetSubmeshes(0)[0].MaterialHandle);
				continue;
			}
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

			if (material->GetUseAlbedo())
				RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 3, AssetManager::GetAsset<Texture2D>(material->GetAlbedoAssetHandle())->GetSRV());
			if (material->GetUseNormal())
				RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 4, AssetManager::GetAsset<Texture2D>(material->GetNormalAssetHandle())->GetSRV());
			if (material->GetUseMetalRough())
				RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 5, AssetManager::GetAsset<Texture2D>(material->GetMetalRoughAssetHandle())->GetSRV());

			object.MeshObject->Bind(0);

			RenderCommand::SetShaderResource(D3D11_VERTEX_SHADER, 0, objectSystem->GetInstanceBuffer(t)->GetSRV());

			RenderCommand::DrawIndexedInstancedIndirect(objectSystem->GetIndirectArgs(), objectSystem->GetArgsOffset(t));
		}

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> nullSRV;
		RenderCommand::SetShaderResource(D3D11_VERTEX_SHADER, 0, nullSRV);
	}

	static inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }

	void Renderer::ComputeCascadeEnds(float nearClip, float farClip, uint32_t cascadeCount, float lambda, float* outCascadeEnds)
	{
		// clamp
		cascadeCount = std::max(1u, cascadeCount);
		lambda = std::clamp(lambda, 0.0f, 1.0f);

		const float n = nearClip;
		const float f = farClip;

		const float range = f - n;
		const float ratio = f / n;

		for (uint32_t i = 1; i <= cascadeCount; ++i)
		{
			const float p = (float)i / (float)cascadeCount;

			const float uniformSplit = n + range * p;
			const float logSplit = n * std::pow(ratio, p);

			const float split = Lerp(uniformSplit, logSplit, lambda);

			outCascadeEnds[i - 1] = split;
		}

		// Guarantee last == farClip exactly (avoids precision edge cases)
		outCascadeEnds[cascadeCount - 1] = farClip;
	}

	void Renderer::ComputeFrustumSliceCornersWS(const Vector3& camPosWS, const Vector3& camRightWS, const Vector3& camUpWS, const Vector3& camForwardWS, float fovYRadians, float aspect, float sliceNear, float sliceFar, DirectX::XMVECTOR outCornersWS[8])
	{
		double heightNear = 2.0 * tan((double)fovYRadians / 2.0) * (double)sliceNear;
		double widthNear = heightNear * aspect;

		double heightFar = 2.0 * tan((double)fovYRadians / 2.0) * (double)sliceFar;
		double widthFar = heightFar * aspect;

		Vector3 centerNear = camPosWS + Vector3::Normalize(camForwardWS) * sliceNear;
		Vector3 centerFar = camPosWS + Vector3::Normalize(camForwardWS) * sliceFar;

		Vector3 nearTopLeft = centerNear + (camUpWS * (heightNear / 2.0)) - (camRightWS * (widthNear / 2.0));
		Vector3 nearTopRight = centerNear + (camUpWS * (heightNear / 2.0)) + (camRightWS * (widthNear / 2.0));
		Vector3 nearBottomLeft = centerNear - (camUpWS * (heightNear / 2.0)) - (camRightWS * (widthNear / 2.0));
		Vector3 nearBottomRight = centerNear - (camUpWS * (heightNear / 2.0)) + (camRightWS * (widthNear / 2.0));

		Vector3 farTopLeft = centerFar + (camUpWS * (heightFar / 2.0)) - (camRightWS * (widthFar / 2.0));
		Vector3 farTopRight = centerFar + (camUpWS * (heightFar / 2.0)) + (camRightWS * (widthFar / 2.0));
		Vector3 farBottomLeft = centerFar - (camUpWS * (heightFar / 2.0)) - (camRightWS * (widthFar / 2.0));
		Vector3 farBottomRight = centerFar - (camUpWS * (heightFar / 2.0)) + (camRightWS * (widthFar / 2.0));

		// Near
		outCornersWS[0] = DirectX::XMVectorSet((float)nearBottomLeft.x, (float)nearBottomLeft.y, (float)nearBottomLeft.z, 1.0f);
		outCornersWS[1] = DirectX::XMVectorSet((float)nearBottomRight.x, (float)nearBottomRight.y, (float)nearBottomRight.z, 1.0f);
		outCornersWS[2] = DirectX::XMVectorSet((float)nearTopRight.x, (float)nearTopRight.y, (float)nearTopRight.z, 1.0f);
		outCornersWS[3] = DirectX::XMVectorSet((float)nearTopLeft.x, (float)nearTopLeft.y, (float)nearTopLeft.z, 1.0f);
						 
		// Far			  
		outCornersWS[4] = DirectX::XMVectorSet((float)farBottomLeft.x, (float)farBottomLeft.y, (float)farBottomLeft.z, 1.0f);
		outCornersWS[5] = DirectX::XMVectorSet((float)farBottomRight.x, (float)farBottomRight.y, (float)farBottomRight.z, 1.0f);
		outCornersWS[6] = DirectX::XMVectorSet((float)farTopRight.x, (float)farTopRight.y, (float)farTopRight.z, 1.0f);
		outCornersWS[7] = DirectX::XMVectorSet((float)farTopLeft.x, (float)farTopLeft.y, (float)farTopLeft.z, 1.0f);
	}

	DirectX::XMMATRIX Renderer::BuildLightViewForCascade(DirectX::XMVECTOR lightDirWS, DirectX::XMVECTOR cascadeCenterWS, float D)
	{
		using namespace DirectX;

		// Robust up selection (same logic you already use)
		XMVECTOR defaultUp = XMVectorSet(0, 1, 0, 0);
		XMVECTOR right = XMVector3Cross(defaultUp, lightDirWS);

		if (XMVectorGetX(XMVector3LengthSq(right)) < 1e-6f)
		{
			defaultUp = XMVectorSet(0, 0, 1, 0);
			right = XMVector3Cross(defaultUp, lightDirWS);
		}
		right = XMVector3Normalize(right);

		XMVECTOR up = XMVector3Normalize(XMVector3Cross(lightDirWS, right));

		XMVECTOR lightPosWS = XMVectorSubtract(cascadeCenterWS, XMVectorScale(lightDirWS, D));

		return XMMatrixLookToLH(lightPosWS, lightDirWS, up);
	}

	//DirectX::XMMATRIX Renderer::FitOrthoToCorners(DirectX::XMMATRIX lightView, const DirectX::XMVECTOR cornersWS[8], float border, float zPadNear)
	//{
	//	using namespace DirectX;

	//	XMVECTOR minV = XMVectorSet(+FLT_MAX, +FLT_MAX, +FLT_MAX, 1.0f);
	//	XMVECTOR maxV = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 1.0f);

	//	for (int i = 0; i < 8; ++i)
	//	{
	//		XMVECTOR cLS = XMVector3TransformCoord(cornersWS[i], lightView);
	//		minV = XMVectorMin(minV, cLS);
	//		maxV = XMVectorMax(maxV, cLS);
	//	}

	//	XMFLOAT3 mn, mx;
	//	XMStoreFloat3(&mn, minV);
	//	XMStoreFloat3(&mx, maxV);

	//	// Add a little padding to reduce edge clipping/popping
	//	mn.x -= border; mn.y -= border;
	//	mx.x += border; mx.y += border;

	//	// Depth padding too (helps with precision and small movements)
	//	mn.z -= zPadNear;
	//	mx.z += 200.0f;

	//	// OrthoOffCenterLH(left,right,bottom,top,nearZ,farZ)
	//	// In LH light space, +Z is forward; minZ can be < 0, that's OK.
	//	return XMMatrixOrthographicOffCenterLH(mn.x, mx.x, mn.y, mx.y, mn.z, mx.z);
	//}

	DirectX::XMMATRIX Renderer::FitOrthoToCornersSnapped(DirectX::XMMATRIX lightView, const DirectX::XMVECTOR cornersWS[8], float border, float zPadNear, uint32_t shadowRes)
	{
		using namespace DirectX;

		XMVECTOR minV = XMVectorSet(+FLT_MAX, +FLT_MAX, +FLT_MAX, 1.0f);
		XMVECTOR maxV = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 1.0f);

		for (int i = 0; i < 8; ++i)
		{
			XMVECTOR cLS = XMVector3TransformCoord(cornersWS[i], lightView);
			minV = XMVectorMin(minV, cLS);
			maxV = XMVectorMax(maxV, cLS);
		}

		XMFLOAT3 mn, mx;
		XMStoreFloat3(&mn, minV);
		XMStoreFloat3(&mx, maxV);

		// Padding
		mn.x -= border; mn.y -= border;
		mx.x += border; mx.y += border;

		mn.z -= zPadNear;
		mx.z += 200.0f;

		// Center + extents
		float centerX = 0.5f * (mn.x + mx.x);
		float centerY = 0.5f * (mn.y + mx.y);

		float extentX = mx.x - mn.x;
		float extentY = mx.y - mn.y;

		// Force square (reduces "breathing")
		float half = 0.5f * std::max(extentX, extentY);
		float extent = 2.0f * half;

		// Texel size in light space for this cascade
		// (protect against degenerate cases)
		float texel = (shadowRes > 0) ? (extent / (float)shadowRes) : 0.0f;
		if (texel > 0.0f)
		{
			// Snap center to nearest texel
			centerX = std::floor(centerX / texel + 0.5f) * texel;
			centerY = std::floor(centerY / texel + 0.5f) * texel;
		}

		// Rebuild bounds from snapped center + square half-size
		mn.x = centerX - half;  mx.x = centerX + half;
		mn.y = centerY - half;  mx.y = centerY + half;

		return XMMatrixOrthographicOffCenterLH(mn.x, mx.x, mn.y, mx.y, mn.z, mx.z);
	}

	void Renderer::ComputeCSMLightViewProj(const Vector3 camPosWS, Camera* camera, const Quaternion& playerCamRot, float fovYRadians, float aspect, DirectX::XMVECTOR lightDirWS, float cameraNear, const float cascadeEnds[MaxCascades], uint32_t cascadeCount, float shadowDistance, DirectX::XMMATRIX outLightViewProj[MaxCascades])
	{
		using namespace DirectX;

		Vector3 camForwardWS = camera->GetForwardVectorWS(playerCamRot);
		Vector3 camUpWS = camera->GetUpVectorWS(playerCamRot);
		Vector3 camRightWS = camera->GetRightVectorWS(playerCamRot);

		for (uint32_t c = 0; c < cascadeCount; ++c)
		{
			const float sliceNear = (c == 0) ? cameraNear : cascadeEnds[c - 1];
			const float sliceFar = cascadeEnds[c];

			// 1) corners
			XMVECTOR cornersWS[8];
			ComputeFrustumSliceCornersWS(camPosWS, camRightWS, camUpWS, camForwardWS, fovYRadians, aspect, sliceNear, sliceFar, cornersWS);

			// 2) center
			XMVECTOR center = XMVectorZero();
			for (int i = 0; i < 8; ++i) center = XMVectorAdd(center, cornersWS[i]);
			center = XMVectorScale(center, 1.0f / 8.0f);

			// 3) light view
			const float D = shadowDistance * 2.0f; // any "large enough" distance works
			XMMATRIX lightView = BuildLightViewForCascade(lightDirWS, center, D);

			// 4) fitted ortho
			//XMMATRIX lightProj = FitOrthoToCorners(lightView, cornersWS, 10.0f, shadowDistance);
			XMMATRIX lightProj = FitOrthoToCornersSnapped(lightView, cornersWS, 10.0f, shadowDistance, 4096);


			outLightViewProj[c] = XMMatrixMultiply(lightView, lightProj);
		}

		// Optionally clear unused cascades
		for (uint32_t c = cascadeCount; c < Toast::MaxCascades; ++c)
			outLightViewProj[c] = XMMatrixIdentity();
	}

}