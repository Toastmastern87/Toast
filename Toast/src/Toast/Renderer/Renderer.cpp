#include "tpch.h"
#include "Renderer.h"

#include "Toast/Renderer/Renderer2D.h"
#include "Toast/Renderer/RendererDebug.h"

#include "Toast/Renderer/PlanetSystem.h"

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

		sRendererData->ViewportHalf.TopLeftX = 0.0f;
		sRendererData->ViewportHalf.TopLeftY = 0.0f;
		sRendererData->ViewportHalf.Width = static_cast<float>(width) / 2.0f;
		sRendererData->ViewportHalf.Height = static_cast<float>(height) / 2.0f;
		sRendererData->ViewportHalf.MinDepth = 0.0f;
		sRendererData->ViewportHalf.MaxDepth = 1.0f;

		sRendererData->ViewportQuarter.TopLeftX = 0.0f;
		sRendererData->ViewportQuarter.TopLeftY = 0.0f;
		sRendererData->ViewportQuarter.Width = static_cast<float>(width) / 4.0f;
		sRendererData->ViewportQuarter.Height = static_cast<float>(height) / 4.0f;
		sRendererData->ViewportQuarter.MinDepth = 0.0f;
		sRendererData->ViewportQuarter.MaxDepth = 1.0f;

		// Setting viewport for shadow mapping
		sRendererData->ShadowMapViewport.TopLeftX = 0.0f;
		sRendererData->ShadowMapViewport.TopLeftY = 0.0f;
		sRendererData->ShadowMapViewport.Width = 8192.0f;
		sRendererData->ShadowMapViewport.Height = 8192.0f;
		sRendererData->ShadowMapViewport.MinDepth = 0.0f;
		sRendererData->ShadowMapViewport.MaxDepth = 1.0f;

		// Setting up the constant buffer and data buffer for the camera rendering
		sRendererData->CameraCBuffer = ConstantBufferLibrary::Load("Camera", 352, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_VERTEX_SHADER, CBufferBindSlot::Camera), CBufferBindInfo(D3D11_PIXEL_SHADER, CBufferBindSlot::Camera), CBufferBindInfo(D3D11_COMPUTE_SHADER, CBufferBindSlot::Camera) });
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
		sRendererData->LightningCBuffer = ConstantBufferLibrary::Load("DirectionalLight", 112, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_VERTEX_SHADER, CBufferBindSlot::DirectionalLight), CBufferBindInfo(D3D11_PIXEL_SHADER, CBufferBindSlot::DirectionalLight), CBufferBindInfo(D3D11_COMPUTE_SHADER, CBufferBindSlot::DirectionalLight) });
		sRendererData->LightningCBuffer->Bind();
		sRendererData->LightningBuffer.Allocate(sRendererData->LightningCBuffer->GetSize());
		sRendererData->LightningBuffer.ZeroInitialize();

		// Setting up the constant buffer and data buffer for environmental rendering
		sRendererData->SunDiscSettingsCBuffer = ConstantBufferLibrary::Load("SunDiscSettings", 80, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_PIXEL_SHADER, CBufferBindSlot::SunDiscSettings), CBufferBindInfo(D3D11_COMPUTE_SHADER, CBufferBindSlot::SunDiscSettings)  });
		sRendererData->SunDiscSettingsCBuffer->Bind();
		sRendererData->SunDiscSettingsBuffer.Allocate(sRendererData->SunDiscSettingsCBuffer->GetSize());
		sRendererData->SunDiscSettingsBuffer.ZeroInitialize();

		// Setting up the constant buffer and data buffer for the render settings
		sRendererData->RenderSettingsCBuffer = ConstantBufferLibrary::Load("RenderSettings", 16, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_PIXEL_SHADER, CBufferBindSlot::RenderSettings) });
		sRendererData->RenderSettingsCBuffer->Bind();
		sRendererData->RenderSettingsBuffer.Allocate(sRendererData->RenderSettingsCBuffer->GetSize());
		sRendererData->RenderSettingsBuffer.ZeroInitialize();

		// Setting up the constant buffer for stars rendering
		sRendererData->StarsCBuffer = ConstantBufferLibrary::Load("StarsParams", 48, std::vector<CBufferBindInfo>{  CBufferBindInfo(D3D11_PIXEL_SHADER, (CBufferBindSlot)7) });
		sRendererData->StarsCBuffer->Bind();
		sRendererData->StarsBuffer.Allocate(sRendererData->StarsCBuffer->GetSize());
		sRendererData->StarsBuffer.ZeroInitialize();

		// Setting up the constant buffer for atmosphere rendering
		sRendererData->AtmosphereCBuffer = ConstantBufferLibrary::Load("Atmosphere", 128, std::vector<CBufferBindInfo>{  CBufferBindInfo(D3D11_PIXEL_SHADER, (CBufferBindSlot)5), CBufferBindInfo(D3D11_COMPUTE_SHADER, (CBufferBindSlot)5) });
		sRendererData->AtmosphereCBuffer->Bind();
		sRendererData->AtmosphereBuffer.Allocate(sRendererData->AtmosphereCBuffer->GetSize());
		sRendererData->AtmosphereBuffer.ZeroInitialize();

		// Setting up the constant buffer for floating origin
		sRendererData->FloatingOriginCBuffer = ConstantBufferLibrary::Load("FloatingOrigin", 16, std::vector<CBufferBindInfo>{  CBufferBindInfo(D3D11_PIXEL_SHADER, (CBufferBindSlot)7), CBufferBindInfo(D3D11_COMPUTE_SHADER, (CBufferBindSlot)7) });
		sRendererData->FloatingOriginCBuffer->Bind();
		sRendererData->FloatingOriginBuffer.Allocate(sRendererData->FloatingOriginCBuffer->GetSize());
		sRendererData->FloatingOriginBuffer.ZeroInitialize();

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

		// Setting up the constant buffer for God Rays
		sRendererData->GodRaysCBuffer = ConstantBufferLibrary::Load("God Rays", 16, std::vector<CBufferBindInfo>{  CBufferBindInfo(D3D11_PIXEL_SHADER, CBufferBindSlot::GodRays) });
		sRendererData->GodRaysCBuffer->Bind();
		sRendererData->GodRaysBuffer.Allocate(sRendererData->GodRaysCBuffer->GetSize());
		sRendererData->GodRaysBuffer.ZeroInitialize();

		// Setting up the constant buffer for bloom rendering
		sRendererData->BloomCBuffer = ConstantBufferLibrary::Load("Bloom", 64, std::vector<CBufferBindInfo>{  CBufferBindInfo(D3D11_PIXEL_SHADER, CBufferBindSlot::Bloom) });
		sRendererData->BloomCBuffer->Bind();
		sRendererData->BloomBuffer.Allocate(sRendererData->BloomCBuffer->GetSize());
		sRendererData->BloomBuffer.ZeroInitialize();
		sRendererData->DownSampleCBuffer = ConstantBufferLibrary::Load("DownsampleParams", 16, std::vector<CBufferBindInfo>{  CBufferBindInfo(D3D11_PIXEL_SHADER, (CBufferBindSlot)13)});
		sRendererData->DownSampleCBuffer->Bind();
		sRendererData->DownSampleBuffer.Allocate(sRendererData->DownSampleCBuffer->GetSize());
		sRendererData->DownSampleBuffer.ZeroInitialize();
		sRendererData->WideBlurCBuffer = ConstantBufferLibrary::Load("WideBlurParams", 16, std::vector<CBufferBindInfo>{  CBufferBindInfo(D3D11_PIXEL_SHADER, (CBufferBindSlot)12)});
		sRendererData->WideBlurCBuffer->Bind();
		sRendererData->WideBlurBuffer.Allocate(sRendererData->WideBlurCBuffer->GetSize());
		sRendererData->WideBlurBuffer.ZeroInitialize();
		sRendererData->UpSampleCBuffer = ConstantBufferLibrary::Load("UpSampleParams", 16, std::vector<CBufferBindInfo>{  CBufferBindInfo(D3D11_PIXEL_SHADER, (CBufferBindSlot)13)});
		sRendererData->UpSampleCBuffer->Bind();
		sRendererData->UpSampleBuffer.Allocate(sRendererData->UpSampleCBuffer->GetSize());
		sRendererData->UpSampleBuffer.ZeroInitialize();

		// Setting up the constant buffer for Tonemapping
		sRendererData->TonemappingCBuffer = ConstantBufferLibrary::Load("Tonemapping", 48, std::vector<CBufferBindInfo>{  CBufferBindInfo(D3D11_PIXEL_SHADER, (CBufferBindSlot)10) });
		sRendererData->TonemappingCBuffer->Bind();
		sRendererData->TonemappingBuffer.Allocate(sRendererData->TonemappingCBuffer->GetSize());
		sRendererData->TonemappingBuffer.ZeroInitialize();

		// Setting up the constant buffer and data buffer for the lightning pass settings
		sRendererData->LightningPassCBuffer = ConstantBufferLibrary::Load("LightningPassSettings", 16, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_PIXEL_SHADER, (CBufferBindSlot)8) });
		sRendererData->LightningPassCBuffer->Bind();
		sRendererData->LightningPassBuffer.Allocate(sRendererData->LightningPassCBuffer->GetSize());
		sRendererData->LightningPassBuffer.ZeroInitialize();

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
		sRendererData->SSAOBlurRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R8G8B8A8_UNORM);

		// Setting up the render target for the Lightning Pass
		sRendererData->LPassRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R16G16B16A16_FLOAT);

		// Setting up the render target for the God Ray pass
		sRendererData->GodRaySunMaskRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, width, 1, TextureFormat::R16G16B16A16_FLOAT);

		// Setting up the render target for Bloom Pass
		sRendererData->SunBloomRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R16G16B16A16_FLOAT);
		sRendererData->SkyBloomRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R16G16B16A16_FLOAT);
		sRendererData->GeometryBloomRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R16G16B16A16_FLOAT);
		sRendererData->SunBloomHalfRT = CreateRef<RenderTarget>(RenderTargetType::Color, width / 2.0f, height / 2.0f, 1, TextureFormat::R16G16B16A16_FLOAT);
		sRendererData->SunBloomQuarterRT = CreateRef<RenderTarget>(RenderTargetType::Color, width / 4.0f, height / 4.0f, 1, TextureFormat::R16G16B16A16_FLOAT);
		sRendererData->SunBloomQuarterBlurRT = CreateRef<RenderTarget>(RenderTargetType::Color, width / 4.0f, height / 4.0f, 1, TextureFormat::R16G16B16A16_FLOAT);
		sRendererData->SunBloomUpSampleRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R16G16B16A16_FLOAT);
		sRendererData->SkyBloomHalfRT = CreateRef<RenderTarget>(RenderTargetType::Color, width / 2.0f, height / 2.0f, 1, TextureFormat::R16G16B16A16_FLOAT);
		sRendererData->SkyBloomQuarterRT = CreateRef<RenderTarget>(RenderTargetType::Color, width / 4.0f, height / 4.0f, 1, TextureFormat::R16G16B16A16_FLOAT);
		sRendererData->SkyBloomQuarterBlurRT = CreateRef<RenderTarget>(RenderTargetType::Color, width / 4.0f, height / 4.0f, 1, TextureFormat::R16G16B16A16_FLOAT);
		sRendererData->SkyBloomUpSampleRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R16G16B16A16_FLOAT);
		sRendererData->GeometryBloomHalfRT = CreateRef<RenderTarget>(RenderTargetType::Color, width / 2.0f, height / 2.0f, 1, TextureFormat::R16G16B16A16_FLOAT);
		sRendererData->GeometryBloomQuarterRT = CreateRef<RenderTarget>(RenderTargetType::Color, width / 4.0f, height / 4.0f, 1, TextureFormat::R16G16B16A16_FLOAT);
		sRendererData->GeometryBloomQuarterBlurRT = CreateRef<RenderTarget>(RenderTargetType::Color, width / 4.0f, height / 4.0f, 1, TextureFormat::R16G16B16A16_FLOAT);
		sRendererData->GeometryBloomUpSampleRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R16G16B16A16_FLOAT);
		sRendererData->FinalBloomRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R16G16B16A16_FLOAT);

		// Setting up the render targets for the Post Process pass
		sRendererData->FinalRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R16G16B16A16_FLOAT, false, true);

		// Setting up the render target for the back buffer
		sRendererData->BackbufferRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R16G16B16A16_FLOAT, true);

		// Setting up the render target for the Atmosphere Pass
		sRendererData->AtmospherePassRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R16G16B16A16_FLOAT);
		sRendererData->StarsRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R16G16B16A16_FLOAT);
		sRendererData->AtmosphereCubeRT = CreateRef<RenderTarget>(RenderTargetType::ColorCube, 256, 256, 1, TextureFormat::R16G16B16A16_FLOAT);
		sRendererData->SunDiscMaskRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R8G8B8A8_UNORM);
		sRendererData->SunHaloMaskRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R8G8B8A8_UNORM);
		sRendererData->Dummy1RT = CreateRef<RenderTarget>(RenderTargetType::ColorCube, 256, 256, 1, TextureFormat::R8G8B8A8_UNORM);
		sRendererData->Dummy2RT = CreateRef<RenderTarget>(RenderTargetType::ColorCube, 256, 256, 1, TextureFormat::R8G8B8A8_UNORM);

		// Setting -Y led to the black since nothing should reflect. 
		// TODO this should most likely be dynamic in the future depending on which color the surface is. It is gray during the night but orange during the day.
		RenderCommand::ClearRenderTargets({ sRendererData->AtmosphereCubeRT->GetRTVFace(0).Get(), sRendererData->Dummy1RT->GetRTVFace(0).Get(), sRendererData->Dummy2RT->GetRTVFace(0).Get() }, { 0.0f, 0.0f, 0.0f, 0.0f });
		RenderCommand::ClearRenderTargets({ sRendererData->AtmosphereCubeRT->GetRTVFace(1).Get(), sRendererData->Dummy1RT->GetRTVFace(1).Get(), sRendererData->Dummy2RT->GetRTVFace(1).Get() }, { 0.0f, 0.0f, 0.0f, 0.0f });
		RenderCommand::ClearRenderTargets({ sRendererData->AtmosphereCubeRT->GetRTVFace(2).Get(), sRendererData->Dummy1RT->GetRTVFace(2).Get(), sRendererData->Dummy2RT->GetRTVFace(2).Get() }, { 0.0f, 0.0f, 0.0f, 0.0f });
		RenderCommand::ClearRenderTargets({ sRendererData->AtmosphereCubeRT->GetRTVFace(3).Get(), sRendererData->Dummy1RT->GetRTVFace(3).Get(), sRendererData->Dummy2RT->GetRTVFace(3).Get() }, { 0.0f, 0.0f, 0.0f, 0.0f });
		RenderCommand::ClearRenderTargets({ sRendererData->AtmosphereCubeRT->GetRTVFace(4).Get(), sRendererData->Dummy1RT->GetRTVFace(4).Get(), sRendererData->Dummy2RT->GetRTVFace(4).Get() }, { 0.0f, 0.0f, 0.0f, 0.0f });
		RenderCommand::ClearRenderTargets({ sRendererData->AtmosphereCubeRT->GetRTVFace(5).Get(), sRendererData->Dummy1RT->GetRTVFace(5).Get(), sRendererData->Dummy2RT->GetRTVFace(5).Get() }, { 0.0f, 0.0f, 0.0f, 0.0f });

		// Setting up dynamic environmental maps 
		sRendererData->EnvMapFiltered = CreateRef<TextureCube>("EnvMapFiltered", 256, 256, 9);
		sRendererData->IrradianceCubeMap = CreateRef<TextureCube>("IrradianceCubemap", 256, 256, 1);

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

		GenerateParticleBuffers();

		GenerateSpecularBRDF();
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

		sRendererData->ViewportHalf.TopLeftX = 0.0f;
		sRendererData->ViewportHalf.TopLeftY = 0.0f;
		sRendererData->ViewportHalf.Width = static_cast<float>(width) / 2.0f;
		sRendererData->ViewportHalf.Height = static_cast<float>(height) / 2.0f;
		sRendererData->ViewportHalf.MinDepth = 0.0f;
		sRendererData->ViewportHalf.MaxDepth = 1.0f;

		sRendererData->ViewportQuarter.TopLeftX = 0.0f;
		sRendererData->ViewportQuarter.TopLeftY = 0.0f;
		sRendererData->ViewportQuarter.Width = static_cast<float>(width) / 4.0f;
		sRendererData->ViewportQuarter.Height = static_cast<float>(height) / 4.0f;
		sRendererData->ViewportQuarter.MinDepth = 0.0f;
		sRendererData->ViewportQuarter.MaxDepth = 1.0f;

		sRendererData->GPassPositionRT->Resize(width, height);
		sRendererData->GPassNormalRT->Resize(width, height);
		sRendererData->GPassAlbedoMetallicRT->Resize(width, height);
		sRendererData->GPassRoughnessAORT->Resize(width, height);
		sRendererData->GPassPickingRT->Resize(width, height);

		sRendererData->SSAORT->Resize(width, height);
		sRendererData->SSAOBlurRT->Resize(width, height);

		sRendererData->GodRaySunMaskRT->Resize(width, height);

		sRendererData->SunBloomRT->Resize(width, height);
		sRendererData->SkyBloomRT->Resize(width, height);
		sRendererData->GeometryBloomRT->Resize(width, height);
		sRendererData->SunBloomHalfRT->Resize(width / 2.0f, height / 2.0f);
		sRendererData->SunBloomQuarterRT->Resize(width / 4.0f, height / 4.0f);
		sRendererData->SunBloomQuarterBlurRT->Resize(width / 4.0f, height / 4.0f);
		sRendererData->SunBloomUpSampleRT->Resize(width, height);
		sRendererData->SkyBloomHalfRT->Resize(width / 2.0f, height / 2.0f);
		sRendererData->SkyBloomQuarterRT->Resize(width / 4.0f, height / 4.0f);
		sRendererData->SkyBloomQuarterBlurRT->Resize(width / 4.0f, height / 4.0f);
		sRendererData->SkyBloomUpSampleRT->Resize(width, height);
		sRendererData->GeometryBloomHalfRT->Resize(width / 2.0f, height / 2.0f);
		sRendererData->GeometryBloomQuarterRT->Resize(width / 4.0f, height / 4.0f);
		sRendererData->GeometryBloomQuarterBlurRT->Resize(width / 4.0f, height / 4.0f);
		sRendererData->GeometryBloomUpSampleRT->Resize(width, height);
		sRendererData->FinalBloomRT->Resize(width, height);

		sRendererData->LPassRT->Resize(width, height);

		sRendererData->AtmospherePassRT->Resize(width, height);
		sRendererData->StarsRT->Resize(width, height);
		sRendererData->SunDiscMaskRT->Resize(width, height);
		sRendererData->SunHaloMaskRT->Resize(width, height);

		sRendererData->FinalRT->Resize(width, height);

		sRendererData->DepthStencilView.Reset();
		sRendererData->ShadowPassStencilView.Reset();

		CreateDepthBuffer(width, height);
		CreateDepthStencilView();

		RendererDebug::OnWindowResize(width, height);
	}

	void Renderer::BeginScene(const Scene* scene, Camera& camera, const DirectX::XMFLOAT4 cameraPos, Scene::Environment& environment, int wireFrame)
	{
		TOAST_PROFILE_FUNCTION();

		sRendererData->Wireframe = wireFrame;

		// Updating the camera data in the buffer and mapping it to the GPU
		UploadCameraCBuffer(camera, cameraPos);

		// Updating the lightning data in the buffer and mapping it to the GPU
		sRendererData->LightningBuffer.Write((uint8_t*)&scene->mLightEnvironment.DirectionalLights[0].ViewProjectionMatrix, 64, 0);
		sRendererData->LightningBuffer.Write((uint8_t*)&scene->mLightEnvironment.DirectionalLights[0].Direction, 16, 64);
		sRendererData->LightningBuffer.Write((uint8_t*)&scene->mLightEnvironment.DirectionalLights[0].Radiance, 16, 80);
		sRendererData->LightningBuffer.Write((uint8_t*)&environment.SunIntensity, 4, 96);
		sRendererData->LightningBuffer.Write((uint8_t*)&scene->mSettings.DirectionalLightningGain, 4, 100);
		sRendererData->LightningCBuffer->Map(sRendererData->LightningBuffer);

		sRendererData->SpecularBRDFLUT->Bind(2, D3D11_PIXEL_SHADER);

		TextureLibrary::GetSampler("Default")->Bind(0, D3D11_PIXEL_SHADER);
		if (TextureLibrary::ExistsSampler("BRDFSampler"))
			TextureLibrary::GetSampler("BRDFSampler")->Bind(1, D3D11_PIXEL_SHADER);

		// Updating the render settings data in the buffer and mapping it to the GPU
		float renderOverlay = (float)scene->mSettings.RenderOverlaySetting;
		sRendererData->RenderSettingsBuffer.Write((uint8_t*)&renderOverlay, 4, 0);
		sRendererData->RenderSettingsCBuffer->Map(sRendererData->RenderSettingsBuffer);
	}

	void Renderer::EndScene(Ref<Planet>& planet, Scene::Environment& environment, Scene::ExposureParams& exposureParams, Scene::BloomParams& bloomParams, const bool debugActivated, const bool shadows, const bool SSAO, const bool dynamicIBL, Camera& camera, const DirectX::XMFLOAT4 cameraPos, float SSAORadius, float SSAObias, float godRayExposure, float godRayDecay, float godRayDensity, float godRayWeight, float dt)
	{
		RenderCommand::SetViewport(sRendererData->Viewport);

		// Deffered Renderer
		GeometryPass();

		if(shadows)
			ShadowPass();
		else
			RenderCommand::ClearDepthStencilView(sRendererData->ShadowPassStencilView);

		if(SSAO)
			SSAOPass(SSAORadius, SSAObias);
		else 
		{
			RenderCommand::ClearRenderTargets(sRendererData->SSAORT->GetRTV().Get(), { 1.0f, 1.0f, 1.0f, 1.0f });
			RenderCommand::ClearRenderTargets(sRendererData->SSAOBlurRT->GetRTV().Get(), { 1.0f, 1.0f, 1.0f, 1.0f });
		}

		LightningPass(planet, environment);

		const bool hasPlanet = (sRendererData->PlanetDraw.Planet != nullptr);
		const bool atmoActive = hasPlanet && sRendererData->PlanetDraw.Planet->AtmosphereActivated();

		const bool hasSkyView = (planet->GetSkyViewLUT() != nullptr);       
		const bool hasAP3D = (planet->GetAerialPerspectiveLUT() != nullptr);

		// Post Processes
		if (hasPlanet && atmoActive && hasSkyView && hasAP3D) 
		{
			StarFieldPass(environment, planet, planet->GetAtmosphere().AtmosphereHeight);
			AtmospherePass(planet, environment, cameraPos, camera.GetWorldTranslation(), dynamicIBL);
			UploadCameraCBuffer(camera, cameraPos);
		}
		else 
		{
			RenderCommand::ClearRenderTargets({ sRendererData->AtmospherePassRT->GetRTV().Get(), sRendererData->SunDiscMaskRT->GetRTV().Get(), sRendererData->SunHaloMaskRT->GetRTV().Get() }, { 0.0f, 0.0f, 0.0f, 0.0f });
		}

		// Particles only for now, but will most likely be renamed and handle more things in the future.
		// If there are no particles that needs to be rendered, this pass will be skipped.
		if (sRendererData->ParticleIndexBuffer.Get())
			ParticlesPass();

		//if (sRendererData->PlanetDraw.Planet)
		//{
		//	if (sRendererData->PlanetDraw.Planet->AtmosphereActivated())
		//		GodRayPass(godRayExposure, godRayDecay, godRayDensity, godRayWeight);
		//}

		if(bloomParams.Enabled)
			BloomPass(bloomParams, planet, cameraPos, camera.GetVerticalFOV());

		PostProcessPass(bloomParams.Enabled, environment, exposureParams, planet, cameraPos);

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

		result = device->CreateDepthStencilState(&depthStencilDesc, &sRendererData->DepthStarFieldStencilState);
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

	void Renderer::SubmitMesh(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, const int entityID, bool wireframe, int noWorldTransform, bool atmosphere)
	{
		sRendererData->PlanetData.Atmosphere = atmosphere;
;		sRendererData->MeshDrawList.emplace_back(mesh, transform, wireframe, noWorldTransform, entityID);
	}

	void Renderer::SubmitSelecetedMesh(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, bool wireframe)
	{
		sRendererData->MeshSelectedDrawList.emplace_back(mesh, transform, wireframe);
	}

	void Renderer::SubmitPlanet(const Ref<Planet> planet, bool wireframe)
	{
		sRendererData->PlanetDraw = { planet, wireframe };
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

	Ref<TextureCube> Renderer::CreateStarFieldTexture(const Texture2D* starFieldTexture)
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		const uint32_t cubemapSize = 4096;

		TextureSampler* defaultSampler = TextureLibrary::GetSampler("Default");
		Ref<TextureCube> envMapUnfiltered = CreateRef<TextureCube>("EnvMapUnfiltered", cubemapSize, cubemapSize);
		Ref<TextureCube> envMapFiltered = CreateRef<TextureCube>("EnvMapFiltered", cubemapSize, cubemapSize);

		envMapUnfiltered->CreateUAV(0);

		if (!equirectangularConversionShader)
			equirectangularConversionShader = CreateScope<Shader>("assets/shaders/Environment/EquirectangularToCubeMap.hlsl");

		equirectangularConversionShader->Bind();
		starFieldTexture->Bind(0, D3D11_COMPUTE_SHADER);
		defaultSampler->Bind(0, D3D11_COMPUTE_SHADER);
		envMapUnfiltered->BindForReadWrite(0, D3D11_COMPUTE_SHADER);
		RenderCommand::DispatchCompute(cubemapSize / 32, cubemapSize / 32, 6);
		envMapUnfiltered->UnbindUAV();

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

		if (sRendererData->PlanetDraw.Planet)
		{
			if (sRendererData->PlanetDraw.Planet->IsValid())
			{
				if (sRendererData->Wireframe == 1)
					RenderCommand::SetRasterizerState(sRendererData->WireframeRasterizerState);
				else
					RenderCommand::SetRasterizerState(sRendererData->NormalRasterizerState);

				ShaderLibrary::Get("assets/shaders/Planet/PlanetGeometryPass.hlsl")->Bind();
				sRendererData->PlanetDraw.Planet->GetShaderLayout()->Bind();

				sRendererData->PlanetDraw.Planet->GetPlanetFrameCBuffer()->Bind();

				TextureLibrary::GetSampler("UWrapVClampLinearSampler")->Bind(5, D3D11_VERTEX_SHADER);
				TextureLibrary::GetSampler("UWrapVClampLinearSampler")->Bind(5, D3D11_PIXEL_SHADER);
				RenderCommand::SetShaderResource(D3D11_VERTEX_SHADER, 0, sRendererData->PlanetDraw.Planet->GetHeightMapCubeTexture()->GetSRV());
				RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 0, sRendererData->PlanetDraw.Planet->GetHeightMapCubeTexture()->GetSRV());
				sRendererData->MaterialBuffer.Write((uint8_t*)&sRendererData->PlanetDraw.Planet->GetAlbedoColor(), 16, 0);
				sRendererData->MaterialBuffer.Write((uint8_t*)&sRendererData->PlanetDraw.Planet->GetMetalness(), 4, 20);
				sRendererData->MaterialBuffer.Write((uint8_t*)&sRendererData->PlanetDraw.Planet->GetRoughness(), 4, 24);
				sRendererData->MaterialCBuffer->Map(sRendererData->MaterialBuffer);

				auto& levels = sRendererData->PlanetDraw.Planet->GetLevels();
				auto& LODInfo = sRendererData->PlanetDraw.Planet->GetLODDrawInfo();

				const uint32_t L0 = LODInfo.first;
				const uint32_t Ln = L0 + LODInfo.count;          // one-past-last

				for (uint32_t L = L0; L < Ln; ++L)
				{
					const auto& level = levels[L];
					if (!level.Dirty && !level.InFrustum)
						continue;

					sRendererData->PlanetDraw.Planet->GetPlanetLevelCBuffer()->Map(sRendererData->PlanetDraw.Planet->BuildLevelCB(L));

					sRendererData->PlanetDraw.Planet->GetLODGridVertexBuffer()->Bind();
					sRendererData->PlanetDraw.Planet->GetLODGridIndexBuffer()->Bind();
					RenderCommand::DrawIndexed(0, 0, sRendererData->PlanetDraw.Planet->GetLODGridIndexCount());

					sRendererData->PlanetDraw.Planet->GetGridVertexBuffer()->Bind();

					if (L == L0)                            // center patch
					{
						sRendererData->PlanetDraw.Planet->GetCenterGridIndexBuffer()->Bind();
						RenderCommand::DrawIndexed(0, 0, sRendererData->PlanetDraw.Planet->GetGridIndexCount());

						continue;
					}

					// inside the ring-drawing branch
					sRendererData->PlanetDraw.Planet->GetRingGridIndexBuffer()->Bind();
					RenderCommand::DrawIndexed(0, 0, sRendererData->PlanetDraw.Planet->GetRingGridIndexCount());
				}
			}
		}

		ShaderLibrary::Get("assets/shaders/Rendering/GeometryPass.hlsl")->Bind();

		for (const auto& meshCommand : sRendererData->MeshDrawList)
		{
			if (meshCommand.Wireframe)
				RenderCommand::SetRasterizerState(sRendererData->WireframeRasterizerState);
			else
				RenderCommand::SetRasterizerState(sRendererData->NormalRasterizerState);

			RenderCommand::SetPrimitiveTopology(meshCommand.Mesh->mTopology);

			int isInstanced = meshCommand.Mesh->IsInstanced() ? 1 : 0;

			float clickable = 1.0f;

			// Model data
			sRendererData->ModelBuffer.Write((uint8_t*)&meshCommand.Transform, 64, 0);
			sRendererData->ModelBuffer.Write((uint8_t*)&clickable, 4, 64);
			sRendererData->ModelBuffer.Write((uint8_t*)&meshCommand.EntityID, 4, 68);
			sRendererData->ModelBuffer.Write((uint8_t*)&meshCommand.NoWorldTransform, 4, 72);
			sRendererData->ModelBuffer.Write((uint8_t*)&isInstanced, 4, 76);
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

			float clickable = 1.0f;

			// Model data
			sRendererData->ModelBuffer.Write((uint8_t*)&meshCommand.Transform, 64, 0);
			sRendererData->ModelBuffer.Write((uint8_t*)&clickable, 4, 64);
			sRendererData->ModelBuffer.Write((uint8_t*)&meshCommand.EntityID, 4, 68);
			sRendererData->ModelBuffer.Write((uint8_t*)&meshCommand.NoWorldTransform, 4, 72);
			sRendererData->ModelBuffer.Write((uint8_t*)&isInstanced, 4, 76);
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

	void Renderer::SSAOPass(float radius, float bias)
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
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 3, sRendererData->GPassPickingRT->GetSRV());

		TextureLibrary::GetSampler("PointSampler")->Bind(3, D3D11_PIXEL_SHADER);
		TextureLibrary::GetSampler("LinearSampler")->Bind(4, D3D11_PIXEL_SHADER);

		sRendererData->SSAOBuffer.Write((uint8_t*)&sRendererData->SSAOKernel[0], 1024, 0);
		sRendererData->SSAOBuffer.Write((uint8_t*)&radius, 4, 1024);
		sRendererData->SSAOBuffer.Write((uint8_t*)&bias, 4, 1028);

		sRendererData->SSAOCBuffer->Map(sRendererData->SSAOBuffer);
		sRendererData->SSAOCBuffer->Bind();

		DrawFullscreenQuad();

		RenderCommand::SetRenderTargets({ sRendererData->SSAOBlurRT->GetRTV().Get() }, nullptr);

		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 0, sRendererData->SSAORT->GetSRV());

		ShaderLibrary::Get("assets/shaders/Rendering/SSAOBlurPass.hlsl")->Bind();

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
		sRendererData->LightningPassCBuffer->Map(sRendererData->LightningPassBuffer);
		sRendererData->LightningPassCBuffer->Bind();

		sRendererData->PlanetDraw.Planet->GetPlanetFrameCBuffer()->Bind();

		ShaderLibrary::Get("assets/shaders/Rendering/LightningPass.hlsl")->Bind();

		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 0, sRendererData->GPassPositionRT->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 1, sRendererData->GPassNormalRT->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 2, sRendererData->GPassAlbedoMetallicRT->GetSRV());

		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 3, sRendererData->GPassRoughnessAORT->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 10, sRendererData->SSAOBlurRT->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 12, sRendererData->ShadowPassDepth->GetSRV());

		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 4, sRendererData->IrradianceCubeMap->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 5, sRendererData->EnvMapFiltered->GetSRV());

		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 6, sRendererData->SpecularBRDFLUT->GetSRV());

		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 7, planet->GetTransmittanceLUT()->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 8, planet->GetMultiScatteringLUT()->GetSRV());

		TextureLibrary::GetSampler("Default")->Bind(0, D3D11_PIXEL_SHADER);
		TextureLibrary::GetSampler("BRDFSampler")->Bind(1, D3D11_PIXEL_SHADER);
		TextureLibrary::GetSampler("PointSampler")->Bind(2, D3D11_PIXEL_SHADER);
		TextureLibrary::GetSampler("LinearSampler")->Bind(3, D3D11_PIXEL_SHADER);

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

				TextureLibrary::GetSampler("LinearSampler")->Bind(0, D3D11_PIXEL_SHADER);
				TextureLibrary::GetSampler("PointSampler")->Bind(1, D3D11_PIXEL_SHADER);

				RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 0, planet->GetTransmittanceLUT()->GetSRV());
				RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 5, sRendererData->PlanetDraw.Planet->GetStarFieldTextureCube()->GetSRV());
				RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 9, sRendererData->DepthBuffer->GetSRV());

				ShaderLibrary::Get("assets/shaders/Post Process/StarField.hlsl")->Bind();

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

	void Renderer::AtmospherePass(Ref<Planet>& planet, Scene::Environment& environment, DirectX::XMFLOAT4 camPosWS, DirectX::XMFLOAT3 worldOffsetWS, const bool dynamicIBL)
	{
		TOAST_PROFILE_FUNCTION();

#ifdef TOAST_DEBUG
		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation)
			annotation->BeginEvent(L"Atmosphere Pass");
#endif

		TextureLibrary::GetSampler("ClampSampler")->Bind(0, D3D11_COMPUTE_SHADER);
		TextureLibrary::GetSampler("PointSampler")->Bind(1, D3D11_COMPUTE_SHADER);
		TextureLibrary::GetSampler("ClampSampler")->Bind(0, D3D11_PIXEL_SHADER);
		TextureLibrary::GetSampler("PointSampler")->Bind(1, D3D11_PIXEL_SHADER);		
		TextureLibrary::GetSampler("SkyTest")->Bind(3, D3D11_PIXEL_SHADER);

		float bakeIBL = 0.0f;

		// Updating the atmospheric data in the buffer and mapping it to the GPU
		auto& atmosphere = planet->GetAtmosphere();
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.AtmosphereHeight, 4, 0);
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.RayleighScaleHeight, 4, 4);
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.MieScaleHeight, 4, 8);
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.MSGain, 4, 12);
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.RayleighScattering, 12, 16);
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.SGain, 4, 28);
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.MieScattering, 12, 32);
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.MieAbsorption, 12, 48);
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.GroundAlbedo, 12, 64);
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.MieAnisotropy, 12, 80);
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.OzoneStrength, 4, 96);
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.StepsTransmittance, 4, 100);
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.StepsMultiScattering, 4, 104);
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.APFarDynamic, 4, 108);
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.SunsetTint, 12, 112);
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&bakeIBL, 4, 124);
		sRendererData->AtmosphereCBuffer->Map(sRendererData->AtmosphereBuffer);
		sRendererData->AtmosphereCBuffer->Bind();

		sRendererData->FloatingOriginBuffer.Write((uint8_t*)&worldOffsetWS, 12, 0);
		sRendererData->FloatingOriginCBuffer->Map(sRendererData->FloatingOriginBuffer);
		sRendererData->FloatingOriginCBuffer->Bind();

		{
			auto& buf = sRendererData->SunDiscSettingsBuffer;

			int sunDiscToggle = environment.SunDiscToggle ? 1 : 0;

			buf.Write((uint8_t*)&environment.SunDiscRadius, 4, 0);
			buf.Write((uint8_t*)&environment.SunEdgeSoftness, 4, 4);
			buf.Write((uint8_t*)&sunDiscToggle, 4, 8);   // int32
			buf.Write((uint8_t*)&environment.SpaceDiscBrightnessScale, 4, 12);

			buf.Write((uint8_t*)&environment.SunWhite, 12, 16);   // float3
			buf.Write((uint8_t*)&environment.AirHaloIntensity, 4, 28);

			buf.Write((uint8_t*)&environment.WarmTint, 12, 32);   // float3
			buf.Write((uint8_t*)&environment.AirHaloStartFrac, 4, 44);

			buf.Write((uint8_t*)&environment.AirHaloFalloffPow, 4, 48);
			buf.Write((uint8_t*)&environment.HorizonRefractionDeg, 4, 52);
			buf.Write((uint8_t*)&environment.TwilightBlendDeg, 4, 56);
			buf.Write((uint8_t*)&environment.SpaceHaloWidthDeg, 4, 60);

			buf.Write((uint8_t*)&environment.SpaceHaloIntensity, 4, 64);
			buf.Write((uint8_t*)&environment.SpaceHaloCutoffDeg, 4, 68);

			sRendererData->SunDiscSettingsCBuffer->Map(sRendererData->SunDiscSettingsBuffer);
			sRendererData->SunDiscSettingsCBuffer->Bind();
		}

		sRendererData->PlanetDraw.Planet->GetPlanetFrameCBuffer()->Bind();

		UINT zero[4] = { 0,0,0,0 };

		auto& aerialPerspective = planet->GetAerialPerspectiveLUT();
		auto& APFar = planet->GetAPFar();
		RenderCommand::ClearUAV(APFar->GetUAV().Get(), zero);
		RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 0, sRendererData->DepthBuffer->GetSRV());
		APFar->BindForReadWrite(0, D3D11_COMPUTE_SHADER);
		ShaderLibrary::Get("assets/shaders/Planet/Atmosphere/APFarDynamic.hlsl")->Bind();
		RenderCommand::DispatchCompute((aerialPerspective->GetWidth() + 7) / 8, (aerialPerspective->GetHeight() + 7) / 8, 1);
		APFar->UnbindUAV(0, D3D11_COMPUTE_SHADER);

		RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 0, planet->GetTransmittanceLUT()->GetSRV());
		RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 1, planet->GetMultiScatteringLUT()->GetSRV());

		auto& skyview = planet->GetSkyViewLUT();
		skyview->BindForReadWrite(0, D3D11_COMPUTE_SHADER);
		ShaderLibrary::Get("assets/shaders/Planet/Atmosphere/SkyViewCS.hlsl")->Bind();
		RenderCommand::DispatchCompute((skyview->GetWidth() + 7) / 8, (skyview->GetHeight() + 7) / 8, 1);
		skyview->UnbindUAV(0, D3D11_COMPUTE_SHADER);
		
		aerialPerspective->BindForReadWrite(0, D3D11_COMPUTE_SHADER);
		RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 2, APFar->GetSRV());
		ShaderLibrary::Get("assets/shaders/Planet/Atmosphere/AerialPerspectiveCS.hlsl")->Bind();
		RenderCommand::DispatchCompute((aerialPerspective->GetWidth() + 7) / 8, (aerialPerspective->GetHeight() + 7) / 8, aerialPerspective->GetDepth());
		aerialPerspective->UnbindUAV(0, D3D11_COMPUTE_SHADER);

		RenderCommand::ClearRenderTargets({ sRendererData->AtmospherePassRT->GetRTV().Get(), sRendererData->SunDiscMaskRT->GetRTV().Get(), sRendererData->SunHaloMaskRT->GetRTV().Get() }, { 0.0f, 0.0f, 0.0f, 1.0f });
		RenderCommand::SetRenderTargets({ sRendererData->AtmospherePassRT->GetRTV().Get(), sRendererData->SunDiscMaskRT->GetRTV().Get(), sRendererData->SunHaloMaskRT->GetRTV().Get() }, nullptr);
		RenderCommand::SetDepthStencilState(sRendererData->DepthEnabledStencilState);
		RenderCommand::SetBlendState(nullptr, { 1.0f, 1.0f, 1.0f, 1.0f });

		ShaderLibrary::Get("assets/shaders/Post Process/Atmosphere.hlsl")->Bind();

		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 0, planet->GetTransmittanceLUT()->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 1, planet->GetMultiScatteringLUT()->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 2, skyview->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 3, aerialPerspective->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 4, sRendererData->GPassPositionRT->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 5, APFar->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 9, sRendererData->DepthBuffer->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 10, sRendererData->LPassRT->GetSRV());

		DrawFullscreenQuad();

		static int currentFace = 0;// Tracks which face of the cube to render

		const DirectX::XMMATRIX& viewMatrix = sRendererData->AtmosphericScatteringViewMatrices[currentFace];
		const DirectX::XMMATRIX& invViewMatrix = sRendererData->AtmosphericScatteringInvViewMatrices[currentFace];
		DirectX::XMFLOAT4 cameraPos = camPosWS;

		sRendererData->CameraBuffer.Write((uint8_t*)&viewMatrix, sizeof(viewMatrix), 64);
		sRendererData->CameraBuffer.Write((uint8_t*)&invViewMatrix, sizeof(invViewMatrix), 192);
		sRendererData->CameraBuffer.Write((uint8_t*)&cameraPos, sizeof(cameraPos), 320);
		sRendererData->CameraCBuffer->Map(sRendererData->CameraBuffer);

		int enableSun = 0;
		sRendererData->SunDiscSettingsBuffer.Write((uint8_t*)&enableSun, 4, 8);
		sRendererData->SunDiscSettingsCBuffer->Map(sRendererData->SunDiscSettingsBuffer);

		bakeIBL = 1.0f;

		sRendererData->AtmosphereBuffer.Write((uint8_t*)&bakeIBL, 4, 124);
		sRendererData->AtmosphereCBuffer->Map(sRendererData->AtmosphereBuffer);

		RenderCommand::SetViewport(sRendererData->AtmosphereCubeViewport);
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> nullSRV = nullptr;
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 3, nullSRV);
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 9, nullSRV);
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 10, nullSRV);
		RenderCommand::SetRenderTargets({ sRendererData->AtmosphereCubeRT->GetRTVFace(currentFace).Get(), sRendererData->Dummy1RT->GetRTVFace(currentFace).Get(), sRendererData->Dummy2RT->GetRTVFace(currentFace).Get() }, nullptr);

		DrawFullscreenQuad();

		RenderCommand::SetRenderTargets({ nullptr }, nullptr);
		RenderCommand::ClearShaderResources();

		GeneratePrefilteredEnvMap(currentFace);

		GenerateIrradianceCubemap(currentFace);

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

	void Renderer::GodRayPass(float exposure, float decay, float density, float weight)
	{
		TOAST_PROFILE_FUNCTION();

#ifdef TOAST_DEBUG
		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation)
			annotation->BeginEvent(L"God Ray Pass");
#endif

		// Disable depth test for screen-space passes
		RenderCommand::SetDepthStencilState(sRendererData->DepthDisabledStencilState);

		// Set render target for the sun mask (this needs to be created in RendererData)
		RenderCommand::SetRenderTargets({ sRendererData->GodRaySunMaskRT->GetRTV().Get() }, nullptr);
		RenderCommand::ClearRenderTargets(sRendererData->GodRaySunMaskRT->GetRTV().Get(), { 0,0,0,0 });

		// Bind shader for rendering the sun mask (assumed to be created at assets/shaders/Post Process/SunDiscMask.hlsl)
		ShaderLibrary::Get("assets/shaders/Utilities/SunDiscMask.hlsl")->Bind();

		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 9, sRendererData->DepthBuffer->GetSRV());

		DrawFullscreenQuad();

		RenderCommand::ClearShaderResources();

		RenderCommand::SetRenderTargets({ sRendererData->AtmospherePassRT->GetRTV().Get() }, nullptr);

		ShaderLibrary::Get("assets/shaders/Post Process/GodRays.hlsl")->Bind();

		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 0, sRendererData->DepthBuffer->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 1, sRendererData->GodRaySunMaskRT->GetSRV());

		RenderCommand::SetBlendState(sRendererData->ParticleBlendState, { 0.0f, 0.0f, 0.0f, 0.0f });

		sRendererData->GodRaysBuffer.Write((uint8_t*)&exposure, 4, 0);
		sRendererData->GodRaysBuffer.Write((uint8_t*)&decay, 4, 4);
		sRendererData->GodRaysBuffer.Write((uint8_t*)&density, 4, 8);
		sRendererData->GodRaysBuffer.Write((uint8_t*)&weight, 4, 12);
		sRendererData->GodRaysCBuffer->Map(sRendererData->GodRaysBuffer);

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

	void Renderer::BloomPass(Scene::BloomParams& bloomParams, Ref<Planet>& planet, const DirectX::XMFLOAT4& cameraPos, const float verticalFovDeg)
	{
		TOAST_PROFILE_FUNCTION();

#ifdef TOAST_DEBUG
		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation)
			annotation->BeginEvent(L"Bloom Pass");
#endif
		DirectX::XMFLOAT3 camPos = DirectX::XMFLOAT3(cameraPos.x, cameraPos.y, cameraPos.z);

		float spaceFactor = planet->GetSpaceFactor(camPos);

		RenderCommand::SetViewport(sRendererData->Viewport);
		RenderCommand::SetRasterizerState(sRendererData->NormalRasterizerState);
		RenderCommand::SetRenderTargets({ sRendererData->SunBloomRT->GetRTV().Get(), sRendererData->SkyBloomRT->GetRTV().Get(), sRendererData->GeometryBloomRT->GetRTV().Get() }, nullptr);
		RenderCommand::SetDepthStencilState(sRendererData->DepthDisabledStencilState);
		RenderCommand::SetBlendState(sRendererData->ParticleBlendState, { 0.0f, 0.0f, 0.0f, 0.0f });
		RenderCommand::ClearRenderTargets({ sRendererData->SunBloomRT->GetRTV().Get(), sRendererData->SkyBloomRT->GetRTV().Get(), sRendererData->GeometryBloomRT->GetRTV().Get() }, { 0.0f, 0.0f, 0.0f, 1.0f });

		TextureLibrary::GetSampler("PointSampler")->Bind(2, D3D11_PIXEL_SHADER);

		sRendererData->BloomBuffer.Write((uint8_t*)&bloomParams.SunSurfaceThreshold, sizeof(float), 0);
		sRendererData->BloomBuffer.Write((uint8_t*)&bloomParams.SunSurfaceIntensity, sizeof(float), 4);
		sRendererData->BloomBuffer.Write((uint8_t*)&bloomParams.SunSpaceThreshold, sizeof(float), 8);
		sRendererData->BloomBuffer.Write((uint8_t*)&bloomParams.SunSpaceIntensity, sizeof(float), 12);
		sRendererData->BloomBuffer.Write((uint8_t*)&bloomParams.SkySurfaceThreshold, sizeof(float), 16);
		sRendererData->BloomBuffer.Write((uint8_t*)&bloomParams.SkySurfaceIntensity, sizeof(float), 20);
		sRendererData->BloomBuffer.Write((uint8_t*)&bloomParams.SkySpaceThreshold, sizeof(float), 24);
		sRendererData->BloomBuffer.Write((uint8_t*)&bloomParams.SkySpaceIntensity, sizeof(float), 28);
		sRendererData->BloomBuffer.Write((uint8_t*)&bloomParams.GeometryThreshold, sizeof(float), 32);
		sRendererData->BloomBuffer.Write((uint8_t*)&bloomParams.GeometryIntensity, sizeof(float), 36);
		sRendererData->BloomBuffer.Write((uint8_t*)&bloomParams.SunRadius, sizeof(float), 40);
		sRendererData->BloomBuffer.Write((uint8_t*)&bloomParams.SkySurfaceRadius, sizeof(float), 44);
		sRendererData->BloomBuffer.Write((uint8_t*)&bloomParams.SkySpaceRadius, sizeof(float), 48);
		sRendererData->BloomBuffer.Write((uint8_t*)&bloomParams.SoftKnee, sizeof(float), 52);
		sRendererData->BloomBuffer.Write((uint8_t*)&bloomParams.SaturationClamp, sizeof(float), 56);
		sRendererData->BloomBuffer.Write((uint8_t*)&spaceFactor, sizeof(float), 60);
		sRendererData->BloomCBuffer->Map(sRendererData->BloomBuffer);

		ShaderLibrary::Get("assets/shaders/Post Process/Bloom.hlsl")->Bind();

		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 0, sRendererData->AtmospherePassRT->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 1, sRendererData->DepthBuffer->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 2, sRendererData->SunDiscMaskRT->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 3, sRendererData->SunHaloMaskRT->GetSRV());

		TextureLibrary::GetSampler("ClampSampler")->Bind(3, D3D11_PIXEL_SHADER);

		DrawFullscreenQuad();

		auto RunBloomChain = [&](RenderTarget* srcFullRT,
			RenderTarget* outHalfRT,
			RenderTarget* outQuarterRT,
			RenderTarget* outQuarterBlurRT,
			RenderTarget* outUpSampleRT,
			float sigmaQuarter)
			{
				// --- Downsample: Full -> Half ---
				{
					auto [W, H] = srcFullRT->GetSize();
					DirectX::XMFLOAT2 srcTexelSize(1.0f / float(W), 1.0f / float(H));
					sRendererData->DownSampleBuffer.Write((uint8_t*)&srcTexelSize.x, 8, 0);
					sRendererData->DownSampleCBuffer->Map(sRendererData->DownSampleBuffer);
					sRendererData->DownSampleCBuffer->Bind();

					RenderCommand::SetViewport(sRendererData->ViewportHalf);
					RenderCommand::SetRenderTargets({ outHalfRT->GetRTV().Get() }, nullptr);
					RenderCommand::ClearRenderTargets({ outHalfRT->GetRTV().Get() }, { 0,0,0,1 });
					RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 0, srcFullRT->GetSRV());
					ShaderLibrary::Get("assets/shaders/Post Process/BloomDownSample.hlsl")->Bind();
					DrawFullscreenQuad();
					RenderCommand::ClearShaderResources();
				}

				// --- Downsample: Half -> Quarter ---
				{
					auto [W2, H2] = outHalfRT->GetSize();
					DirectX::XMFLOAT2 srcTexelSize(1.0f / float(W2), 1.0f / float(H2));
					sRendererData->DownSampleBuffer.Write((uint8_t*)&srcTexelSize.x, 8, 0);
					sRendererData->DownSampleCBuffer->Map(sRendererData->DownSampleBuffer);
					sRendererData->DownSampleCBuffer->Bind();

					RenderCommand::SetViewport(sRendererData->ViewportQuarter);
					RenderCommand::SetRenderTargets({ outQuarterRT->GetRTV().Get() }, nullptr);
					RenderCommand::ClearRenderTargets({ outQuarterRT->GetRTV().Get() }, { 0,0,0,1 });
					RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 0, outHalfRT->GetSRV());
					ShaderLibrary::Get("assets/shaders/Post Process/BloomDownSample.hlsl")->Bind();
					DrawFullscreenQuad();
					RenderCommand::ClearShaderResources();
				}

				// --- Wide blur @ 1/4 res ---
				{
					auto [QW, QH] = outQuarterRT->GetSize();
					DirectX::XMFLOAT2 qTexel(1.0f / float(QW), 1.0f / float(QH));

					sRendererData->WideBlurBuffer.Write((uint8_t*)&qTexel.x, 8, 0);
					sRendererData->WideBlurBuffer.Write((uint8_t*)&sigmaQuarter, 4, 8);
					sRendererData->WideBlurCBuffer->Map(sRendererData->WideBlurBuffer);
					sRendererData->WideBlurCBuffer->Bind();

					RenderCommand::SetRenderTargets({ outQuarterBlurRT->GetRTV().Get() }, nullptr);
					RenderCommand::ClearRenderTargets({ outQuarterBlurRT->GetRTV().Get() }, { 0,0,0,1 });
					RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 0, outQuarterRT->GetSRV());
					ShaderLibrary::Get("assets/shaders/Post Process/BloomWideBlur.hlsl")->Bind();
					DrawFullscreenQuad();
					RenderCommand::ClearShaderResources();
				}

				// --- Upsample 1/4 → full ---
				{
					auto [QW, QH] = outQuarterBlurRT->GetSize();
					DirectX::XMFLOAT2 quarterTexelSize(1.0f / float(QW), 1.0f / float(QH));
					float weightQuarter = 0.85f; // same as before

					sRendererData->UpSampleBuffer.Write((uint8_t*)&quarterTexelSize.x, 8, 0);
					sRendererData->UpSampleBuffer.Write((uint8_t*)&weightQuarter, 4, 8);
					sRendererData->UpSampleCBuffer->Map(sRendererData->UpSampleBuffer);
					sRendererData->UpSampleCBuffer->Bind();

					RenderCommand::SetViewport(sRendererData->Viewport);
					RenderCommand::SetRenderTargets({ outUpSampleRT->GetRTV().Get() }, nullptr);
					RenderCommand::ClearRenderTargets({ outUpSampleRT->GetRTV().Get() }, { 0,0,0,1 });

					RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 0, outQuarterBlurRT->GetSRV());
					ShaderLibrary::Get("assets/shaders/Post Process/BloomUpSample.hlsl")->Bind();
					DrawFullscreenQuad();
					RenderCommand::ClearShaderResources();
				}
			};

		auto degToPx = [&](float degHalf, float verticalFovDeg, float viewportH)->float
			{
				float a = std::max(degHalf * float(M_PI / 180.0), 1e-4f);
				float f = std::tan(a) / std::tan(0.5f * verticalFovDeg * float(M_PI / 180.0));
				return f * (viewportH * 0.5f);
			};

		float halfRadiusPxAtmos = degToPx(1.2f, verticalFovDeg, sRendererData->EditorViewport.Height);
		float halfRadiusPxSpace = degToPx(0.6f, verticalFovDeg, sRendererData->EditorViewport.Height);
		float fullHalfPx = lerp(halfRadiusPxAtmos, halfRadiusPxSpace, spaceFactor);

		// SUN: from SunRadius
		float sigmaSunQuarter = std::max(bloomParams.SunRadius * fullHalfPx / 4.0f, 8.0f);

		// SKY: blend surface/space
		float skyRadiusBlend = lerp(bloomParams.SkySurfaceRadius, bloomParams.SkySpaceRadius, spaceFactor);
		float sigmaSkyQuarter = std::max(skyRadiusBlend * fullHalfPx / 4.0f, 8.0f);

		// GEOMETRY: reuse sky or tighten slightly
		float sigmaGeomQuarter = std::max(0.8f * sigmaSkyQuarter, 8.0f);

		// SUN
		RunBloomChain(sRendererData->SunBloomRT.get(),
			sRendererData->SunBloomHalfRT.get(),
			sRendererData->SunBloomQuarterRT.get(),
			sRendererData->SunBloomQuarterBlurRT.get(),
			sRendererData->SunBloomUpSampleRT.get(),
			sigmaSunQuarter);

		// SKY
		RunBloomChain(sRendererData->SkyBloomRT.get(),
			sRendererData->SkyBloomHalfRT.get(),
			sRendererData->SkyBloomQuarterRT.get(),
			sRendererData->SkyBloomQuarterBlurRT.get(),
			sRendererData->SkyBloomUpSampleRT.get(),
			sigmaSkyQuarter);

		// GEOMETRY
		RunBloomChain(sRendererData->GeometryBloomRT.get(),
			sRendererData->GeometryBloomHalfRT.get(),
			sRendererData->GeometryBloomQuarterRT.get(),
			sRendererData->GeometryBloomQuarterBlurRT.get(),
			sRendererData->GeometryBloomUpSampleRT.get(),
			sigmaGeomQuarter);

		RenderCommand::SetRenderTargets({ sRendererData->FinalBloomRT->GetRTV().Get() }, nullptr);
		RenderCommand::ClearRenderTargets({ sRendererData->FinalBloomRT->GetRTV().Get() }, { 0.0f, 0.0f, 0.0f, 1.0f });

		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 0, sRendererData->AtmospherePassRT->GetSRV());      // scene HDR
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 1, sRendererData->SunBloomUpSampleRT->GetSRV());    // t1
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 2, sRendererData->SkyBloomUpSampleRT->GetSRV());    // t2
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 3, sRendererData->GeometryBloomUpSampleRT->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 4, planet->GetTransmittanceLUT()->GetSRV());

		ShaderLibrary::Get("assets/shaders/Post Process/BloomComposite.hlsl")->Bind();

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

	void Renderer::PostProcessPass(const bool bloom, Scene::Environment& environment, Scene::ExposureParams& exposureParams, Ref<Planet>& planet, const DirectX::XMFLOAT4& cameraPos)
	{
		TOAST_PROFILE_FUNCTION();
#ifdef TOAST_DEBUG
		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation)
			annotation->BeginEvent(L"Tonemapping Pass");
#endif

		DirectX::XMFLOAT3 camPos = DirectX::XMFLOAT3(cameraPos.x, cameraPos.y, cameraPos.z);

		float spaceFactor = planet->GetSpaceFactor(camPos);

		RenderCommand::SetDepthStencilState(sRendererData->DepthDisabledStencilState);

		//Tonemapping
		RenderCommand::SetRenderTargets({ sRendererData->FinalRT->GetRTV().Get() }, nullptr);
		RenderCommand::ClearRenderTargets(sRendererData->FinalRT->GetRTV().Get(), {0.0f, 0.0f, 0.0f, 1.0f});
		RenderCommand::SetBlendState(sRendererData->LPassBlendState, { 0.0f, 0.0f, 0.0f, 0.0f });

		sRendererData->StarsBuffer.Write((uint8_t*)&environment.StarNits, 4, 0);
		sRendererData->StarsBuffer.Write((uint8_t*)&environment.NightAmbient, 12, 20);
		sRendererData->StarsCBuffer->Map(sRendererData->StarsBuffer);
		sRendererData->StarsCBuffer->Bind();

		TextureLibrary::GetSampler("ClampSampler")->Bind(0, D3D11_PIXEL_SHADER);
		TextureLibrary::GetSampler("PointSampler")->Bind(1, D3D11_PIXEL_SHADER);

		sRendererData->TonemappingBuffer.Write((uint8_t*)&exposureParams.EVGeometrySurface, 4, 0);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&exposureParams.EVGeometrySpace, 4, 4);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&exposureParams.EVGeometryNight, 4, 8);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&exposureParams.EVSkySurface, 4, 12);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&exposureParams.EVSkySpace, 4, 16);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&exposureParams.EVSkySurfaceNight, 4, 20);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&exposureParams.EVSkySpaceNight, 4, 24);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&exposureParams.AltFadeStartFrac, 4, 28);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&exposureParams.AltFadeEndFrac, 4, 32);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&exposureParams.SunFadeStartDeg, 4, 36);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&exposureParams.SunFadeEndDeg, 4, 40);
		sRendererData->TonemappingCBuffer->Map(sRendererData->TonemappingBuffer);
		sRendererData->TonemappingCBuffer->Bind();

		ShaderLibrary::Get("assets/shaders/Post Process/ToneMapping.hlsl")->Bind();

		if(bloom)
			RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 9, sRendererData->FinalBloomRT->GetSRV());
		else
			RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 9, sRendererData->AtmospherePassRT->GetSRV());

		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 10, sRendererData->StarsRT->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 11, sRendererData->SSAOBlurRT->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 12, sRendererData->DepthBuffer->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 13, sRendererData->GPassNormalRT->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 14, sRendererData->SunDiscMaskRT->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 15, sRendererData->SunHaloMaskRT->GetSRV());

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

	void Renderer::ResetStats()
	{
		memset(&sData.Stats, 0, sizeof(Statistics));
	}

	void Renderer::GenerateSpecularBRDF()
	{
		sRendererData->SpecularBRDFLUT = CreateRef<Texture2D>(DXGI_FORMAT_R16G16_FLOAT, DXGI_FORMAT_R16G16_FLOAT, 256, 256);

		sRendererData->SpecularBRDFLUT->CreateUAV(0);

		sRendererData->SpecularBRDFLUT->BindForReadWrite(0, D3D11_COMPUTE_SHADER);
		ShaderLibrary::Load("assets/shaders/Environment/SPBRDF.hlsl")->Bind();

		sRendererData->SpecularMapFilterSettingsCBuffer->Bind();

		RenderCommand::DispatchCompute(sRendererData->SpecularBRDFLUT->GetWidth() / 32, sRendererData->SpecularBRDFLUT->GetHeight() / 32, 1);
		sRendererData->SpecularBRDFLUT->UnbindUAV();
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

		sRendererData->SpecularMapFilterSettingsCBuffer->Bind();

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

		sRendererData->SpecularMapFilterSettingsCBuffer->Bind();

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
		auto& atmosphere = planet->GetAtmosphere();
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.AtmosphereHeight, 4, 0);
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.RayleighScaleHeight, 4, 4);
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.MieScaleHeight, 4, 8);
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.RayleighScattering, 12, 16);
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.MieScattering, 12, 32);
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.MieAbsorption, 12, 48);
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.GroundAlbedo, 12, 64);
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.OzoneStrength, 4, 76);
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.MieAnisotropy, 12, 80);
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.StepsTransmittance, 4, 96);
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.StepsMultiScattering, 4, 100);
		sRendererData->AtmosphereCBuffer->Map(sRendererData->AtmosphereBuffer);
		sRendererData->AtmosphereCBuffer->Bind();

		planet->GetTransmittanceLUT()->BindForReadWrite(0, D3D11_COMPUTE_SHADER);

		ShaderLibrary::Get("assets/shaders/Planet/Atmosphere/TransmittanceCS.hlsl")->Bind();

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
		auto& atmosphere = planet->GetAtmosphere();
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.AtmosphereHeight, 4, 0);
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.RayleighScaleHeight, 4, 4);
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.MieScaleHeight, 4, 8);
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.RayleighScattering, 12, 16);
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.MieScattering, 12, 32);
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.MieAbsorption, 12, 48);
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.GroundAlbedo, 12, 64);
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.OzoneStrength, 4, 76);
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.MieAnisotropy, 12, 80);
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.StepsTransmittance, 4, 96);
		sRendererData->AtmosphereBuffer.Write((uint8_t*)&atmosphere.StepsMultiScattering, 4, 100);
		sRendererData->AtmosphereCBuffer->Map(sRendererData->AtmosphereBuffer);
		sRendererData->AtmosphereCBuffer->Bind();

		RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 0, planet->GetTransmittanceLUT()->GetSRV());
		TextureLibrary::GetSampler("ClampSampler")->Bind(0, D3D11_COMPUTE_SHADER);

		planet->GetMultiScatteringLUT()->BindForReadWrite(0, D3D11_COMPUTE_SHADER);

		ShaderLibrary::Get("assets/shaders/Planet/Atmosphere/MultiScatteringCS.hlsl")->Bind();

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

	DirectX::XMFLOAT3 Renderer::SampleSSAONoiseTexture(uint32_t x, uint32_t y)
	{
		const uint32_t NOISE_DIM = 16;  // same dimension as your SSAO noise texture
		x = x % NOISE_DIM;
		y = y % NOISE_DIM;

		const DirectX::XMFLOAT4& sample = sRendererData->SSAONoiseCPU[y * NOISE_DIM + x];
		return { sample.x, sample.y, sample.z };
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

}