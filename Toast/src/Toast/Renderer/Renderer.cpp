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
		sRendererData->LightningCBuffer = ConstantBufferLibrary::Load("DirectionalLight", 336, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_VERTEX_SHADER, CBufferBindSlot::DirectionalLight), CBufferBindInfo(D3D11_PIXEL_SHADER, CBufferBindSlot::DirectionalLight), CBufferBindInfo(D3D11_COMPUTE_SHADER, CBufferBindSlot::DirectionalLight) });
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
		sRendererData->GodRaysCBuffer = ConstantBufferLibrary::Load("God Rays", 32, std::vector<CBufferBindInfo>{  CBufferBindInfo(D3D11_PIXEL_SHADER, CBufferBindSlot::GodRays) });
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
		sRendererData->TonemappingCBuffer = ConstantBufferLibrary::Load("Tonemapping", 128, std::vector<CBufferBindInfo>{  CBufferBindInfo(D3D11_PIXEL_SHADER, (CBufferBindSlot)10) });
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

		// Setting up the render target for SSAO Pass
		sRendererData->SSAORT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R8G8B8A8_UNORM);
		sRendererData->SSAOBlurRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R8G8B8A8_UNORM);

		// Setting up the render target for the Lightning Pass
		sRendererData->LPassRT = CreateRef<RenderTarget>(RenderTargetType::Color, width, height, 1, TextureFormat::R16G16B16A16_FLOAT);

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

		// Setting -Y led to the black since nothing should reflect. 
		// TODO this should most likely be dynamic in the future depending on which color the surface is. It is gray during the night but orange during the day.
		RenderCommand::ClearRenderTargets({ sRendererData->AtmosphereCubeRT->GetRTVFace(0).Get(), sRendererData->Dummy1RT->GetRTVFace(0).Get(), sRendererData->Dummy2RT->GetRTVFace(0).Get() }, { 0.0f, 0.0f, 0.0f, 0.0f });
		RenderCommand::ClearRenderTargets({ sRendererData->AtmosphereCubeRT->GetRTVFace(1).Get(), sRendererData->Dummy1RT->GetRTVFace(1).Get(), sRendererData->Dummy2RT->GetRTVFace(1).Get() }, { 0.0f, 0.0f, 0.0f, 0.0f });
		RenderCommand::ClearRenderTargets({ sRendererData->AtmosphereCubeRT->GetRTVFace(2).Get(), sRendererData->Dummy1RT->GetRTVFace(2).Get(), sRendererData->Dummy2RT->GetRTVFace(2).Get() }, { 0.0f, 0.0f, 0.0f, 0.0f });
		RenderCommand::ClearRenderTargets({ sRendererData->AtmosphereCubeRT->GetRTVFace(3).Get(), sRendererData->Dummy1RT->GetRTVFace(3).Get(), sRendererData->Dummy2RT->GetRTVFace(3).Get() }, { 0.0f, 0.0f, 0.0f, 0.0f });
		RenderCommand::ClearRenderTargets({ sRendererData->AtmosphereCubeRT->GetRTVFace(4).Get(), sRendererData->Dummy1RT->GetRTVFace(4).Get(), sRendererData->Dummy2RT->GetRTVFace(4).Get() }, { 0.0f, 0.0f, 0.0f, 0.0f });
		RenderCommand::ClearRenderTargets({ sRendererData->AtmosphereCubeRT->GetRTVFace(5).Get(), sRendererData->Dummy1RT->GetRTVFace(5).Get(), sRendererData->Dummy2RT->GetRTVFace(5).Get() }, { 0.0f, 0.0f, 0.0f, 0.0f });

		// Setting up dynamic environmental maps 
		sRendererData->EnvMapFilteredDay = CreateRef<TextureCube>("EnvMapFilteredDay", 256, 256, 9);
		sRendererData->IrradianceCubeMapDay = CreateRef<TextureCube>("IrradianceCubemapDay", 256, 256, 1);
		sRendererData->EnvMapFilteredNight = CreateRef<TextureCube>("EnvMapFilteredNight", 256, 256, 9);
		sRendererData->IrradianceCubeMapNight = CreateRef<TextureCube>("IrradianceCubemapNight", 256, 256, 1);

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

		sRendererData->Wireframe = wireFrame;

		// Updating the camera data in the buffer and mapping it to the GPU
		UploadCameraCBuffer(camera, cameraPos);

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

		TextureLibrary::GetSampler("Default")->Bind(0, D3D11_PIXEL_SHADER);
		if (TextureLibrary::ExistsSampler("BRDFSampler"))
			TextureLibrary::GetSampler("BRDFSampler")->Bind(1, D3D11_PIXEL_SHADER);

		// Updating the render settings data in the buffer and mapping it to the GPU
		float renderOverlay = (float)scene->mSettings.RenderOverlaySetting;
		sRendererData->RenderSettingsBuffer.Write((uint8_t*)&renderOverlay, 4, 0);
		sRendererData->RenderSettingsCBuffer->Map(sRendererData->RenderSettingsBuffer);
	}

	void Renderer::EndScene(Ref<Planet>& planet, Scene::Environment& environment, Scene::ExposureParams& exposureParams, Scene::BloomParams& bloomParams, const bool debugActivated, const bool shadows, const bool SSAO, const bool dynamicIBL, Camera& camera, const DirectX::XMFLOAT4 cameraPos, float SSAORadius, float SSAObias, Scene::GodRayParams godRayParams, Scene::CascadedShadowMapParams& shadowParams, float dt)
	{
		RenderCommand::SetViewport(sRendererData->Viewport);

		// Deffered Renderer
		GeometryPass();

		if(shadows)
			ShadowPass(shadowParams);
		else
			RenderCommand::ClearDepthStencilView(sRendererData->ShadowPassDepthStencilView[0], 1.0f);

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

		if (sRendererData->PlanetDraw.Planet)
		{
			if (sRendererData->PlanetDraw.Planet->AtmosphereActivated())
				GodRayPass(godRayParams);
		}

		if(bloomParams.Enabled)
			BloomPass(bloomParams, planet, cameraPos, camera.GetVerticalFOV(), camera.GetWorldTranslation());

		PostProcessPass(bloomParams.Enabled, environment, exposureParams, planet, cameraPos, camera.GetWorldTranslation());

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

		// God Ray Pass Blend State
		{
			D3D11_BLEND_DESC blendDesc = {};
			blendDesc.AlphaToCoverageEnable = FALSE;
			blendDesc.IndependentBlendEnable = FALSE;
			blendDesc.RenderTarget[0].BlendEnable = TRUE;
			blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
			blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
			blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
			blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
			result = device->CreateBlendState(&blendDesc, &sRendererData->GodRayPassBlendState);
			TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create God Ray Pass blend state");
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
			blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
			blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
			blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_INV_DEST_ALPHA;
			blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
			blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

			blendDesc.RenderTarget[1].BlendEnable = TRUE;
			blendDesc.RenderTarget[1].SrcBlend = D3D11_BLEND_SRC_ALPHA;
			blendDesc.RenderTarget[1].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
			blendDesc.RenderTarget[1].BlendOp = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[1].SrcBlendAlpha = D3D11_BLEND_INV_DEST_ALPHA;
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
		rasterDesc.DepthClipEnable = true;
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

	void Renderer::SubmitMesh(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, const int entityID, uint32_t submeshIndex, bool wireframe, int noWorldTransform, bool atmosphere)
	{
		sRendererData->PlanetData.Atmosphere = atmosphere;
;		sRendererData->MeshDrawList.emplace_back(mesh, transform, wireframe, noWorldTransform, entityID, submeshIndex);
	}

	void Renderer::SubmitSelecetedMesh(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, bool wireframe, uint32_t submeshIndex)
	{
		bool noWorldTransform = false;
		int entityID = 0;
		sRendererData->MeshSelectedDrawList.emplace_back(mesh, transform, wireframe, noWorldTransform, entityID, submeshIndex);
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

				if (sRendererData->PlanetDraw.Planet->GetMeshMode() == PlanetMeshMode::GeometryClipmapping)
				{
					ShaderLibrary::Get("assets/shaders/Planet/PlanetGeometryPass.hlsl")->Bind();
					sRendererData->PlanetDraw.Planet->GetShaderLayout()->Bind();
				}
				else if(sRendererData->PlanetDraw.Planet->GetMeshMode() == PlanetMeshMode::Icosphere)
				{
					ShaderLibrary::Get("assets/shaders/Planet/PlanetIcosphereGeometryPass.hlsl")->Bind();
					sRendererData->PlanetDraw.Planet->GetIcosphereMesh()->GetShaderInputLayout()->Bind();
				}

				if (sRendererData->PlanetDraw.Planet->GetNumHeightDetails() > 0)
				{
					RenderCommand::SetShaderResource(D3D11_VERTEX_SHADER, 8, sRendererData->PlanetDraw.Planet->GetHeightDetailSettingsSB()->GetSRV());
					RenderCommand::SetShaderResource(D3D11_VERTEX_SHADER, 9, sRendererData->PlanetDraw.Planet->GetHeightDetailPermSB()->GetSRV());
					RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 8, sRendererData->PlanetDraw.Planet->GetHeightDetailSettingsSB()->GetSRV());
					RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 9, sRendererData->PlanetDraw.Planet->GetHeightDetailPermSB()->GetSRV());
				}

				sRendererData->PlanetDraw.Planet->MapRenderingSettings();
				sRendererData->PlanetDraw.Planet->GetPlanetRenderingSettingsCBuffer()->Bind();

				if (sRendererData->PlanetDraw.Planet->GetMeshMode() == PlanetMeshMode::GeometryClipmapping)
				{
					sRendererData->PlanetDraw.Planet->GetPlanetFrameCBuffer()->Bind();
				}
				else if (sRendererData->PlanetDraw.Planet->GetMeshMode() == PlanetMeshMode::Icosphere)
				{
					sRendererData->ModelBuffer.Write((uint8_t*)&sRendererData->PlanetDraw.Planet->GetTransformRotation(), 64, 0);
					sRendererData->ModelCBuffer->Map(sRendererData->ModelBuffer);
				}

				TextureLibrary::GetSampler("UWrapVClampLinearSampler")->Bind(5, D3D11_VERTEX_SHADER);
				TextureLibrary::GetSampler("UWrapVClampLinearSampler")->Bind(5, D3D11_PIXEL_SHADER);
				RenderCommand::SetShaderResource(D3D11_VERTEX_SHADER, 0, sRendererData->PlanetDraw.Planet->GetHeightMapCubeTexture()->GetSRV());
				RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 2, sRendererData->PlanetDraw.Planet->GetNormalMapCubeTexture()->GetSRV());

				sRendererData->MaterialBuffer.Write((uint8_t*)&sRendererData->PlanetDraw.Planet->GetAlbedoColor(), 16, 0);
				sRendererData->MaterialBuffer.Write((uint8_t*)&sRendererData->PlanetDraw.Planet->GetMetalness(), 4, 20);
				sRendererData->MaterialBuffer.Write((uint8_t*)&sRendererData->PlanetDraw.Planet->GetRoughness(), 4, 24);
				int useAlbedo = static_cast<int>(sRendererData->PlanetDraw.Planet->GetUseAlbedoMap());
				sRendererData->MaterialBuffer.Write((uint8_t*)&useAlbedo, 4, 28);
				sRendererData->MaterialCBuffer->Map(sRendererData->MaterialBuffer);

				if (sRendererData->PlanetDraw.Planet->GetUseAlbedoMap())
					RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 3, sRendererData->PlanetDraw.Planet->GetAlbedoCubeTexture()->GetSRV());

				if(sRendererData->PlanetDraw.Planet->GetMeshMode() == PlanetMeshMode::GeometryClipmapping)
				{
					auto& levels = sRendererData->PlanetDraw.Planet->GetLevels();
					auto& LODInfo = sRendererData->PlanetDraw.Planet->GetLODDrawInfo();

					const uint32_t L0 = LODInfo.first;
					const uint32_t Ln = L0 + LODInfo.count;          // one-past-last

					for (uint32_t L = L0; L < Ln; ++L)
					{
						const auto& level = levels[L];
						if (!level.Dirty && !level.InFrustum)
							continue;

						auto cb = sRendererData->PlanetDraw.Planet->BuildLevelCB(L);

						uint32_t drawMode = 1;
						cb.Write(reinterpret_cast<uint8_t*>(&drawMode), sizeof(uint32_t), 16);
						sRendererData->PlanetDraw.Planet->GetPlanetLevelCBuffer()->Map(cb);
						sRendererData->PlanetDraw.Planet->GetPlanetLevelCBuffer()->Bind();

						sRendererData->PlanetDraw.Planet->GetLODGridVertexBuffer()->Bind();
						sRendererData->PlanetDraw.Planet->GetLODGridIndexBuffer()->Bind();
						RenderCommand::DrawIndexed(0, 0, sRendererData->PlanetDraw.Planet->GetLODGridIndexCount());

						drawMode = 0;
						cb.Write(reinterpret_cast<uint8_t*>(&drawMode), sizeof(uint32_t), 16);
						sRendererData->PlanetDraw.Planet->GetPlanetLevelCBuffer()->Map(cb);
						sRendererData->PlanetDraw.Planet->GetPlanetLevelCBuffer()->Bind();

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
				else if(sRendererData->PlanetDraw.Planet->GetMeshMode() == PlanetMeshMode::Icosphere)
				{
					auto& icosphereMesh = sRendererData->PlanetDraw.Planet->GetIcosphereMesh();
					icosphereMesh->BindGPUData();

					RenderCommand::DrawIndexedInstanced(icosphereMesh->GetIndexCount(), icosphereMesh->GetPatchCount(), 0, 0, 0);
				}
				
			}
		}

		RenderCommand::ClearShaderResources();

		RenderCommand::SetShaderResource(D3D11_VERTEX_SHADER, 0, sRendererData->PlanetDraw.Planet->GetHeightMapCubeTexture()->GetSRV());

		ShaderLibrary::Get("assets/shaders/Rendering/GeometryPass.hlsl")->Bind();

		if (sRendererData->PlanetDraw.Planet)
		{
			if (sRendererData->PlanetDraw.Planet->IsValid())
			{
				Planet* planet = sRendererData->PlanetDraw.Planet.get();
				const auto& LODInfo = planet->GetLODDrawInfo();
				const uint32_t L0 = LODInfo.first;
				const uint32_t Ln = L0 + LODInfo.count;

				if (!sRendererData->PlanetDraw.Planet->GetTerrainObjects().empty())
				{
					if (sRendererData->PlanetDraw.Planet->GetNumHeightDetails() > 0)
					{
						RenderCommand::SetShaderResource(D3D11_VERTEX_SHADER, 8, sRendererData->PlanetDraw.Planet->GetHeightDetailSettingsSB()->GetSRV());
						RenderCommand::SetShaderResource(D3D11_VERTEX_SHADER, 9, sRendererData->PlanetDraw.Planet->GetHeightDetailPermSB()->GetSRV());
						RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 8, sRendererData->PlanetDraw.Planet->GetHeightDetailSettingsSB()->GetSRV());
						RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 9, sRendererData->PlanetDraw.Planet->GetHeightDetailPermSB()->GetSRV());
					}

					for (uint32_t L = L0; L < Ln; ++L)
					{
						// --- Edge strip (DrawMode=1) ---
						{
							auto cb = planet->BuildLevelCB(L);
							uint32_t drawMode = 1;
							cb.Write(reinterpret_cast<uint8_t*>(&drawMode), sizeof(uint32_t), 16);
							planet->GetPlanetLevelCBuffer()->Map(cb);
							planet->GetPlanetLevelCBuffer()->Bind(); // b7

							DrawTerrainObjectsForLevel(planet, L, L0);
						}

						// --- Interior (DrawMode=0) ---
						{
							auto cb = planet->BuildLevelCB(L);
							uint32_t drawMode = 0;
							cb.Write(reinterpret_cast<uint8_t*>(&drawMode), sizeof(uint32_t), 16);
							planet->GetPlanetLevelCBuffer()->Map(cb);
							planet->GetPlanetLevelCBuffer()->Bind(); // b7

							DrawTerrainObjectsForLevel(planet, L, L0);
						}
					}
				}
			}
		}

		sRendererData->CurrentMesh = nullptr;

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

			const Submesh& submesh = meshCommand.Mesh->mLODGroups[meshCommand.Mesh->mActiveLODGroup]->Submeshes[meshCommand.SubmeshIndex];

			// Model data
			sRendererData->ModelBuffer.Write((uint8_t*)&meshCommand.Transform, 64, 0);
			sRendererData->ModelBuffer.Write((uint8_t*)&clickable, 4, 64);
			sRendererData->ModelBuffer.Write((uint8_t*)&meshCommand.EntityID, 4, 68);
			sRendererData->ModelBuffer.Write((uint8_t*)&meshCommand.NoWorldTransform, 4, 72);
			sRendererData->ModelBuffer.Write((uint8_t*)&isInstanced, 4, 76);
			sRendererData->ModelCBuffer->Map(sRendererData->ModelBuffer);

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

			if (sRendererData->CurrentMesh != meshCommand.Mesh.get())
			{
				meshCommand.Mesh->Bind();
				sRendererData->CurrentMesh = meshCommand.Mesh.get();
			}

			RenderCommand::DrawIndexed(0, submesh.BaseIndex, submesh.IndexCount);
		}

		std::vector<ID3D11RenderTargetView*> nullRTVs(6, nullptr);

		ShaderLibrary::Get("assets/shaders/Rendering/GeometryPass.hlsl")->Unbind();
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

		RenderCommand::SetViewport(sRendererData->ShadowMapViewport);
		RenderCommand::SetRasterizerState(sRendererData->ShadowMapRasterizerState);
		RenderCommand::SetDepthStencilState(sRendererData->ShadowPassDepthStencilState);
		RenderCommand::SetPrimitiveTopology(Topology::TRIANGLELIST);

		ShaderLibrary::Get("assets/shaders/Rendering/ShadowPass.hlsl")->Bind();

		ID3D11RenderTargetView* nullRTV = nullptr;

		sRendererData->CurrentMesh = nullptr;

		for (uint32_t i = 0; i < shadowParams.CascadeCount; ++i)
		{
			// Bind the slice
			RenderCommand::SetRenderTargets({ nullRTV }, sRendererData->ShadowPassDepthStencilView[i]);
			RenderCommand::ClearDepthStencilView(sRendererData->ShadowPassDepthStencilView[i], 1.0f);

			sRendererData->LightningBuffer.Write((uint8_t*)&i, 4, 320);
			sRendererData->LightningCBuffer->Map(sRendererData->LightningBuffer);

			for (const auto& meshCommand : sRendererData->MeshDrawList)
			{
				const Submesh& submesh = meshCommand.Mesh->mLODGroups[meshCommand.Mesh->mActiveLODGroup]->Submeshes[meshCommand.SubmeshIndex];

				if (sRendererData->CurrentMesh != meshCommand.Mesh.get())
				{
					meshCommand.Mesh->Bind();
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

		sRendererData->StarsBuffer.Write((uint8_t*)&environment.NightAmbient, 12, 36);
		sRendererData->StarsCBuffer->Map(sRendererData->StarsBuffer);

		sRendererData->PlanetDraw.Planet->GetPlanetFrameCBuffer()->Bind();

		ShaderLibrary::Get("assets/shaders/Rendering/LightningPass.hlsl")->Bind();

		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 0, sRendererData->GPassPositionRT->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 1, sRendererData->GPassNormalRT->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 2, sRendererData->GPassAlbedoMetallicRT->GetSRV());

		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 3, sRendererData->GPassRoughnessAORT->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 10, sRendererData->SSAOBlurRT->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 12, sRendererData->ShadowMapArray->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 13, sRendererData->GPassPickingRT->GetSRV());

		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 4, sRendererData->IrradianceCubeMapDay->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 5, sRendererData->EnvMapFilteredDay->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 14, sRendererData->IrradianceCubeMapNight->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 15, sRendererData->EnvMapFilteredNight->GetSRV());
		if (sRendererData->PlanetDraw.Planet->IsValid())
			RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 16, sRendererData->PlanetDraw.Planet->GetHeightMapCubeTexture()->GetSRV());

		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 17, sRendererData->DepthBuffer->GetSRV());

		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 6, sRendererData->SpecularBRDFLUT->GetSRV());

		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 7, planet->GetTransmittanceLUT()->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 8, planet->GetMultiScatteringLUT()->GetSRV());

		TextureLibrary::GetSampler("Default")->Bind(0, D3D11_PIXEL_SHADER);
		TextureLibrary::GetSampler("BRDFSampler")->Bind(1, D3D11_PIXEL_SHADER);
		TextureLibrary::GetSampler("PointSampler")->Bind(2, D3D11_PIXEL_SHADER);
		TextureLibrary::GetSampler("LinearSampler")->Bind(3, D3D11_PIXEL_SHADER);
		TextureLibrary::GetSampler("ShadowCmp")->Bind(4, D3D11_PIXEL_SHADER);

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

		sRendererData->AtmosphereBuffer.Write((uint8_t*)&currentFace, 4, 76);
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

		// Day time IBL generation
		GeneratePrefilteredEnvMap(sRendererData->AtmosphereCubeRT->GetTextureOriginal(), sRendererData->EnvMapFilteredDay, currentFace);
		GenerateIrradianceCubemap(sRendererData->EnvMapFilteredDay, sRendererData->IrradianceCubeMapDay, currentFace);

		// Night time IBL generation(This only need to be done 6 times)
		if (!sRendererData->NightTimeIBLDone)
		{
			GeneratePrefilteredEnvMap(planet->GetStarFieldTextureCube().get(), sRendererData->EnvMapFilteredNight, currentFace);
			GenerateIrradianceCubemap(sRendererData->EnvMapFilteredNight, sRendererData->IrradianceCubeMapNight, currentFace);
		}

		if (currentFace == 5)
			sRendererData->NightTimeIBLDone = true;

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

	void Renderer::GodRayPass(Scene::GodRayParams params)
	{
		TOAST_PROFILE_FUNCTION();

#ifdef TOAST_DEBUG
		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation)
			annotation->BeginEvent(L"God Ray Pass");
#endif

		RenderCommand::SetRenderTargets({ sRendererData->AtmospherePassRT->GetRTV().Get() }, nullptr);

		ShaderLibrary::Get("assets/shaders/Post Process/GodRays.hlsl")->Bind();

		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 0, sRendererData->DepthBuffer->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 1, sRendererData->SunDiscMaskRT->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 2, sRendererData->SunHaloMaskRT->GetSRV());

		TextureLibrary::GetSampler("ClampSampler")->Bind(0, D3D11_PIXEL_SHADER);
		TextureLibrary::GetSampler("PointSampler")->Bind(1, D3D11_PIXEL_SHADER);

		RenderCommand::SetBlendState(sRendererData->GodRayPassBlendState, { 0.0f, 0.0f, 0.0f, 0.0f });

		sRendererData->GodRaysBuffer.Write((uint8_t*)&params.Exposure, 4, 0);
		sRendererData->GodRaysBuffer.Write((uint8_t*)&params.Decay, 4, 4);
		sRendererData->GodRaysBuffer.Write((uint8_t*)&params.Density, 4, 8);
		sRendererData->GodRaysBuffer.Write((uint8_t*)&params.Weight, 4, 12);
		sRendererData->GodRaysBuffer.Write((uint8_t*)&params.KHalo, 4, 16);
		sRendererData->GodRaysBuffer.Write((uint8_t*)&params.HaloPower, 4, 20);
		sRendererData->GodRaysBuffer.Write((uint8_t*)&params.FogRangeMeters, 4, 24);
		sRendererData->GodRaysCBuffer->Map(sRendererData->GodRaysBuffer);
		sRendererData->GodRaysCBuffer->Bind();

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

	void Renderer::BloomPass(Scene::BloomParams& bloomParams, Ref<Planet>& planet, const DirectX::XMFLOAT4& cameraPos, const float verticalFovDeg, DirectX::XMFLOAT3 worldOffsetWS)
	{
		TOAST_PROFILE_FUNCTION();

#ifdef TOAST_DEBUG
		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation)
			annotation->BeginEvent(L"Bloom Pass");
#endif
		DirectX::XMFLOAT3 camPos = DirectX::XMFLOAT3(cameraPos.x, cameraPos.y, cameraPos.z);

		float spaceFactor = planet->GetSpaceFactor(camPos, worldOffsetWS);

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

	void Renderer::PostProcessPass(const bool bloom, Scene::Environment& environment, Scene::ExposureParams& exposureParams, Ref<Planet>& planet, const DirectX::XMFLOAT4& cameraPos, DirectX::XMFLOAT3 worldOffsetWS)
	{
		TOAST_PROFILE_FUNCTION();
#ifdef TOAST_DEBUG
		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation)
			annotation->BeginEvent(L"Tonemapping Pass");
#endif

		DirectX::XMFLOAT3 camPos = DirectX::XMFLOAT3(cameraPos.x, cameraPos.y, cameraPos.z);

		float spaceFactor = planet->GetSpaceFactor(camPos, worldOffsetWS);

		float lensStrength = (environment.SunUV.valid ? 1.0f : 0.0f);

		RenderCommand::SetDepthStencilState(sRendererData->DepthDisabledStencilState);

		//Tonemapping
		RenderCommand::SetRenderTargets({ sRendererData->FinalRT->GetRTV().Get(), sRendererData->FinalEditorRT->GetRTV().Get() }, nullptr);
		RenderCommand::ClearRenderTargets(sRendererData->FinalRT->GetRTV().Get(), {0.0f, 0.0f, 0.0f, 1.0f});
		RenderCommand::ClearRenderTargets(sRendererData->FinalEditorRT->GetRTV().Get(), { 0.0f, 0.0f, 0.0f, 1.0f });
		RenderCommand::SetBlendState(sRendererData->PostProcessBlendState, { 0.0f, 0.0f, 0.0f, 0.0f });

		sRendererData->StarsBuffer.Write((uint8_t*)&environment.StarNits, 4, 0);
		sRendererData->StarsBuffer.Write((uint8_t*)&environment.NightAmbient, 12, 20);
		sRendererData->StarsCBuffer->Map(sRendererData->StarsBuffer);
		sRendererData->StarsCBuffer->Bind();

		TextureLibrary::GetSampler("ClampSampler")->Bind(0, D3D11_PIXEL_SHADER);
		TextureLibrary::GetSampler("PointSampler")->Bind(1, D3D11_PIXEL_SHADER);

		sRendererData->TonemappingBuffer.Write((uint8_t*)&exposureParams.EVSurfaceDay, 4, 0);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&exposureParams.EVSpaceDay, 4, 4);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&exposureParams.EVSurfaceNight, 4, 8);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&exposureParams.EVSpaceNight, 4, 12);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&exposureParams.AltFadeFrac.x, 8, 16);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&exposureParams.SunFadeDeg.x, 8, 24);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&environment.SunUV.uv, 8, 32);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&spaceFactor, 4, 40);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&lensStrength, 4, 44);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&environment.SunSpikes, 4, 48);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&environment.SunSpikeSharpness, 4, 52);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&environment.SunSpikeRadiusSurface, 4, 56);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&environment.SunSpikeRadiusSpace, 4, 60);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&environment.SunSpikeFallOff, 4, 64);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&environment.SunSpikeStrengthSurface, 4, 68);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&environment.SunSpikeStrengthSpace, 4, 72);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&environment.SunGlareStrengthSurface, 4, 76);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&environment.SunGlareStrengthSpace, 4, 80);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&environment.SunGlareRadiusSurface, 4, 84);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&environment.SunGlareRadiusSpace, 4, 88);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&environment.LensAltStart, 4, 92);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&environment.LensAltEnd, 4, 96);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&environment.GhostStrength, 4, 100);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&environment.GhostSpacing, 4, 104);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&environment.GhostFalloff, 4, 108);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&environment.GhostSizeSurface, 4, 112);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&environment.GhostSizeSpace, 4, 116);
		sRendererData->TonemappingBuffer.Write((uint8_t*)&environment.GhostAirSuppression, 4, 120);

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
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 16, sRendererData->AtmospherePassRT->GetSRV());

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

	void Renderer::GeneratePrefilteredEnvMap(Texture* sourceTexture, Ref<TextureCube> targetTexture, int faceIndex)
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		ShaderLibrary::Get("assets/shaders/Environment/EnvironmentMipFilter.hlsl")->Bind();

		sRendererData->SpecularMapFilterSettingsCBuffer->Bind();

		// Bind the atmospheric scattering cube map as input (unfiltered environment map)
		RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 14, sourceTexture->GetSRV());

		// Bind the sampler
		TextureLibrary::GetSampler("Default")->Bind(0, D3D11_COMPUTE_SHADER);

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
		const uint32_t cubemapSize = targetTexture->GetWidth();
		int size = cubemapSize / 2;
		const uint32_t mipCount = targetTexture->GetMipLevelCount();

		const int firstMip = canCopyMip0 ? 1 : 0;

		for (int mipLevel = firstMip; mipLevel < (int)mipCount; ++mipLevel)
		{
			uint32_t size = targetTexture->GetWidth() / (1u << mipLevel);
			const int numGroups = std::max(1, int((size + 7) / 8));

			targetTexture->CreateUAVUpdated(mipLevel, faceIndex);

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
		ShaderLibrary::Get("assets/shaders/Environment/EnvironmentIrradiance.hlsl")->Bind();

		sRendererData->SpecularMapFilterSettingsCBuffer->Bind();

		RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 15, sourceTexture->GetSRV());

		targetTexture->CreateUAVUpdated(0, faceIndex);

		targetTexture->BindForReadWriteUpdated(0, D3D11_COMPUTE_SHADER, 0, faceIndex);

		// Determine dispatch dimensions
		uint32_t textureDepth;
		uint32_t textureWidth = targetTexture->GetWidth();
		uint32_t textureHeight = targetTexture->GetHeight();

		uint32_t dispatchX = (textureWidth + 31) / 32;
		uint32_t dispatchY = (textureHeight + 31) / 32;

		RenderCommand::DispatchCompute(dispatchX, dispatchY, 1); 

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

	void Renderer::DrawTerrainObjectsForLevel(Planet* planet, uint32_t L, uint32_t L0)
	{
		const auto& objects = planet->GetTerrainObjects();
		if (objects.empty())
			return;

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

		for (const TerrainObject& object : objects)
		{
			if (!object.MeshObject)
				continue;

			if (L > (uint32_t)object.LODActivation)
				continue;

			uint32_t cellSize = 1u << L; 
			uint32_t gridSize = planet->GetGridSize();

			// Decide instance count for this level
			uint32_t instCount = planet->ObjectInstancesForLevelFromDensity(object, cellSize, gridSize);
			if (instCount == 0)
				continue;

			const double cells = double(gridSize - 1);
			const double widthM = cells * double(cellSize);
			const double areaM2 = widthM * widthM;
			double scatterCellSize = std::sqrt(areaM2 / std::max<double>(1.0, double(instCount)));
			scatterCellSize = std::clamp(scatterCellSize, 0.25 * double(cellSize), 8.0 * double(cellSize));
			float scatterCellSizeF = static_cast<float>(scatterCellSize);

			uint32_t scatterCells = (uint32_t)std::ceil(widthM / scatterCellSize);
			scatterCells = std::clamp<uint32_t>(scatterCells, 16u, 1024u);

			// Fill per-layer CB (b13)
			auto& buffer = planet->GetTerrainObjectBuffer();
			buffer.Write((uint8_t*)&object.Seed, 4, 0);
			buffer.Write((uint8_t*)&object.LODActivation, 4, 4);
			buffer.Write((uint8_t*)&instCount, 4, 8);
			buffer.Write((uint8_t*)&object.MinScale, 4, 12);
			buffer.Write((uint8_t*)&object.MaxScale, 4, 16);
			buffer.Write((uint8_t*)&scatterCellSizeF, 4, 20);
			buffer.Write((uint8_t*)&scatterCells, 4, 24);

			planet->GetTerrainObjectCBuffer()->Map(buffer);
			planet->GetTerrainObjectCBuffer()->Bind(); // b13

			auto& material = object.MeshObject->GetMaterial(object.MeshObject->GetSubmeshes()[0].MaterialName);
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
				RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 3, material->GetAlbedoTexture()->GetSRV());
			if (material->GetUseNormal())
				RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 4, material->GetNormalTexture()->GetSRV());
			if (material->GetUseMetalRough())
				RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 5, material->GetMetalRoughTexture()->GetSRV());

			// Bind mesh + material like your normal path (important!)
			// If your Mesh::Bind() does not bind material SRVs, do it here.
			object.MeshObject->Bind();

			uint32_t candidateCount = scatterCells * scatterCells;

			// Start with LOD0 group submesh 0 (same assumption you used previously)
			const auto& sub = object.MeshObject->mLODGroups[0]->Submeshes[0];
			RenderCommand::DrawIndexedInstanced(sub.IndexCount, candidateCount, sub.BaseIndex, 0, 0);
		}
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