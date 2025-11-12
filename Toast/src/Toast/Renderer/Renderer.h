#pragma once

#include "Toast/Renderer/RenderCommand.h"
#include "Toast/Renderer/OrthographicCamera.h"
#include "Toast/Renderer/Mesh.h"
#include "Toast/Renderer/RenderTarget.h"

#include "Toast/Scene/Scene.h"
#include "Toast/Scene/SceneCamera.h"
#include "Toast/Scene/Components.h"

namespace Toast {

	class Renderer 
	{
	private:
		struct DrawCommand
		{
		public:
			DrawCommand(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, const bool wireframe, int noWorldTransform = 1, const int entityID = 0)
				: Mesh(mesh), Transform(transform), Wireframe(wireframe), NoWorldTransform(noWorldTransform), EntityID(entityID) {}
		public:
			Ref<Mesh> Mesh;
			DirectX::XMMATRIX Transform;
			bool Wireframe;
			int NoWorldTransform;
			const int EntityID;
		};

		struct DrawCommandPlanet 
		{
		public:
			DrawCommandPlanet() = default;
			DrawCommandPlanet(const Ref<Planet> planet, const bool wireframe)
				: Planet(planet), Wireframe(wireframe) {}
		public:
			Ref<Planet> Planet{};
			bool Wireframe = false;
		};

		struct RendererData
		{
			DirectX::XMFLOAT4 CameraPos;
			DirectX::XMFLOAT4X4 ViewMatrix;
			DirectX::XMFLOAT4X4 ProjectionMatrix;

			DirectX::XMMATRIX AtmosphericScatteringViewMatrices[6];
			DirectX::XMMATRIX AtmosphericScatteringInvViewMatrices[6];

			std::vector<DirectX::XMFLOAT4> SSAOKernel;

			int Wireframe = 0;

			struct SceneInfo
			{
				LightEnvironment SceneLightEnvironment;

				struct SkyboxInfo
				{
					float Intensity;
					float LOD;
				} SkyboxData;

			} SceneData;

			struct PlanetInfo
			{
				bool Atmosphere = false;
			} PlanetData;

			std::vector<DrawCommand> MeshDrawList, MeshSelectedDrawList, MeshWireframeDrawList, MeshNoWireframeDrawList;
			DrawCommandPlanet PlanetDraw;

			Ref<ConstantBuffer> CameraCBuffer, LightningCBuffer, SunDiscSettingsCBuffer, RenderSettingsCBuffer, AtmosphereCBuffer, ModelCBuffer, MaterialCBuffer, SpecularMapFilterSettingsCBuffer, SSAOCBuffer, GodRaysCBuffer, FloatingOriginCBuffer;
			Buffer CameraBuffer, LightningBuffer, SunDiscSettingsBuffer, RenderSettingsBuffer, AtmosphereBuffer, ModelBuffer, MaterialBuffer, SpecularMapFilterSettingsBuffer, SSAOBuffer, GodRaysBuffer, FloatingOriginBuffer;

			// Back buffer
			Ref<RenderTarget> BackbufferRT;

			// Geometry Pass
			Ref<RenderTarget> GPassPositionRT, GPassNormalRT, GPassAlbedoMetallicRT, GPassRoughnessAORT, GPassPickingRT;

			// Shadow mapping Pass
			Ref<RenderTarget> ShadowMapRT;
			
			// Lightning Pass
			Ref<RenderTarget> LPassRT;
			Ref<ConstantBuffer> LightningPassCBuffer;
			Buffer LightningPassBuffer;

			// Particle Pass
			Microsoft::WRL::ComPtr<ID3D11BlendState> ParticleBlendState;
			Microsoft::WRL::ComPtr<ID3D11DepthStencilState> ParticleDepthStencilState;

			// Stars pass
			Ref<RenderTarget> StarsRT;
			Ref<ConstantBuffer> StarsCBuffer;
			Buffer StarsBuffer;

			// Atmosphere pass
			Ref<RenderTarget> AtmospherePassRT;			
			Ref<RenderTarget> AtmosphereCubeRT, Dummy1RT, Dummy2RT;

			// Environmental Textures
			Ref<TextureCube> EnvMapFiltered, IrradianceCubeMap;

			// Post Process
			Ref<RenderTarget> FinalRT;

			// Viewports
			D3D11_VIEWPORT Viewport, ShadowMapViewport, EditorViewport, AtmosphereCubeViewport, ViewportHalf, ViewportQuarter;

			// Rasterization states
			Microsoft::WRL::ComPtr<ID3D11RasterizerState> NormalRasterizerState, WireframeRasterizerState, ShadowMapRasterizerState;

			// Depth data
			Scope<Texture2D> DepthBuffer, ShadowPassDepth;
			Microsoft::WRL::ComPtr<ID3D11DepthStencilState> DepthEnabledStencilState, DepthDisabledStencilState, DepthStarFieldStencilState, ShadowPassDepthStencilState;
			Microsoft::WRL::ComPtr<ID3D11DepthStencilView> DepthStencilView, ShadowPassStencilView;

			// Blend data
			Microsoft::WRL::ComPtr<ID3D11BlendState> GPassBlendState, LPassBlendState, AtmospherePassBlendState, PostProcessBlendState, UIBlendState;

			// SSAO data
			Ref<RenderTarget> SSAORT, SSAOBlurRT;
			std::vector<DirectX::XMFLOAT4> SSAONoiseCPU;
			Scope<Texture2D> SSAONoiseTexture;

			// Good Ray data
			Ref<RenderTarget> GodRaySunMaskRT;

			// Bloom data
			Ref<RenderTarget> SunBloomRT, SkyBloomRT, GeometryBloomRT;
			Ref<RenderTarget> SunBloomHalfRT, SunBloomQuarterRT, SunBloomQuarterBlurRT, SunBloomUpSampleRT;
			Ref<RenderTarget> SkyBloomHalfRT, SkyBloomQuarterRT, SkyBloomQuarterBlurRT, SkyBloomUpSampleRT;
			Ref<RenderTarget> GeometryBloomHalfRT, GeometryBloomQuarterRT, GeometryBloomQuarterBlurRT, GeometryBloomUpSampleRT;
			Ref<RenderTarget> FinalBloomRT;
			Ref<ConstantBuffer> BloomCBuffer, DownSampleCBuffer, WideBlurCBuffer, UpSampleCBuffer;
			Buffer BloomBuffer, DownSampleBuffer, WideBlurBuffer, UpSampleBuffer;

			// Tonemapping data
			Ref<ConstantBuffer> TonemappingCBuffer;
			Buffer TonemappingBuffer;

			// Utils
			Ref<RenderTarget> SunDiscMaskRT, SunHaloMaskRT;
			Ref<Texture2D> SpecularBRDFLUT;
			Ref<ConstantBuffer> BlurCBuffer;
			Buffer BlurBuffer;

			// Particle Data
			Microsoft::WRL::ComPtr<ID3D11Buffer> ParticleBuffer;
			Microsoft::WRL::ComPtr<ID3D11Buffer> ParticleIndexBuffer;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ParticlesSRV;
			size_t NrOfParticlesToRender;
			Texture2D* ParticleMaskTexture;
		};

	protected:
		static Scope<RendererData> sRendererData;

	public:
		static void Init(uint32_t width, uint32_t height);
		static void Shutdown();

		static void OnWindowResize(uint32_t width, uint32_t height);
		static void OnViewportResize(uint32_t width, uint32_t height);

		static void BeginScene(const Scene* scene, Camera& camera, const DirectX::XMFLOAT4 cameraPos, Scene::Environment& environment, int wireFrame);
		static void EndScene(Ref<Planet>& planet, Scene::Environment& environment, Scene::ExposureParams& exposureParams, Scene::BloomParams& bloomParams, const bool debugActivated, const bool shadows, const bool SSAO, const bool dynamicIBL, Camera& camera, const DirectX::XMFLOAT4 cameraPos, float SSAORadius, float SSAObias, float godRayExposure, float godRayDecay, float godRayDensity, float godRayWeight, float dt);

		static void CreateDepthBuffer(uint32_t width, uint32_t height);
		static void CreateDepthStencilView();
		static void CreateDepthStencilStates();
		static void CreateBlendStates();
		static void CreateRasterizerStates();

		// SSAO
		static void GenerateSampleKernel();
		static void GenerateNoiseTexture();

		static Ref<TextureCube> CreateStarFieldTexture(const Texture2D* starFieldTexture);

		static void SetUpAtmosphericScatteringMatrices();

		static void Submit(const Ref<IndexBuffer>& indexBuffer, const Ref<Shader> shader, const Ref<ShaderLayout> bufferLayout, const Ref<VertexBuffer> vertexBuffer, const DirectX::XMMATRIX& transform);
		static void SubmitSkybox(const DirectX::XMFLOAT4& cameraPos, const DirectX::XMFLOAT4X4& viewMatrix, const DirectX::XMFLOAT4X4& projectionMatrix, float intensity, float LOD);
		static void SubmitMesh(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, const int entityID, bool wireframe = false, int noWorldTransform = 0, bool atmosphere = false);
		static void SubmitSelecetedMesh(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, bool wireframe = false);
		static void SubmitPlanet(const Ref<Planet> planet, bool wireframe = false);

		static void DrawFullscreenQuad();

		static void ClearDrawList();

		// Deffered Rendering
		static void GeometryPass();
		static void ShadowPass();
		static void LightningPass(Ref<Planet>& planet, Scene::Environment& environment);
		static void ParticlesPass(Camera& camera, const DirectX::XMFLOAT4 cameraPos);
		static void SSAOPass(float radius, float bias);

		// Post Processes
		static void StarFieldPass(Scene::Environment& environment, Ref<Planet>& planet, const float atmosphereHeight );
		static void AtmospherePass(Ref<Planet>& planet, Scene::Environment& environment, DirectX::XMFLOAT4 camPosWS, DirectX::XMFLOAT3 worldOffsetWS, const bool dynamicIBL);
		static void BloomPass(Scene::BloomParams& bloomParams, Ref<Planet>& planet, const DirectX::XMFLOAT4& cameraPos, const float verticalFovDeg);
		static void GodRayPass(float exposure, float decay, float density, float weight);
		static void PostProcessPass(const bool bloom, Scene::Environment& environment, Scene::ExposureParams& exposureParams, Ref<Planet>& planet, const DirectX::XMFLOAT4& cameraPos);

		static Ref<RenderTarget>& GetGPassPositionRT() { return sRendererData->GPassPositionRT; }
		static Ref<RenderTarget>& GetGPassNormalRT() { return sRendererData->GPassNormalRT; }
		static Ref<RenderTarget>& GetGPassAlbedoMetallicRT() { return sRendererData->GPassAlbedoMetallicRT; }
		static Ref<RenderTarget>& GetGPassRoughnessAORT() { return sRendererData->GPassRoughnessAORT; }
		static Ref<RenderTarget>& GetGPassPickingRT() { TOAST_PROFILE_FUNCTION(); return sRendererData->GPassPickingRT; }

		static Ref<RenderTarget>& GetAtmosphericScatteringRT() { return sRendererData->AtmospherePassRT; }
		static Ref<RenderTarget>& GetStarsRT() { return sRendererData->StarsRT; }

		static Ref<RenderTarget>& GetSSAORT() { return sRendererData->SSAORT; }
		static Ref<RenderTarget>& GetSSAOBlurRT() { return sRendererData->SSAOBlurRT; }

		static Ref<RenderTarget>& GetSunBloomRT() { return sRendererData->SunBloomRT; }
		static Ref<RenderTarget>& GetSkyBloomRT() { return sRendererData->SkyBloomRT; }
		static Ref<RenderTarget>& GetGeometryBloomRT() { return sRendererData->GeometryBloomRT; }
		static Ref<RenderTarget>& GetSunBloomHalfRT() { return sRendererData->SunBloomHalfRT; }
		static Ref<RenderTarget>& GetSunBloomQuarterRT() { return sRendererData->SunBloomQuarterRT; }
		static Ref<RenderTarget>& GetSkyBloomHalfRT() { return sRendererData->SkyBloomHalfRT; }
		static Ref<RenderTarget>& GetSkyBloomQuarterRT() { return sRendererData->SkyBloomQuarterRT; }
		static Ref<RenderTarget>& GetGeometryBloomHalfRT() { return sRendererData->GeometryBloomHalfRT; }
		static Ref<RenderTarget>& GetGeometryBloomQuarterRT() { return sRendererData->GeometryBloomQuarterRT; }
		static Ref<RenderTarget>& GetFinalBloomRT() { return sRendererData->FinalBloomRT; }

		static Ref<RenderTarget>& GetLPassRT() { return sRendererData->LPassRT; }

		static Ref<RenderTarget>& GetFinalRT() { return sRendererData->FinalRT; }

		static void EnableAtmosphere(bool atmosphere) { sRendererData->PlanetData.Atmosphere = atmosphere; }

		static void SetParticlesIndexBuffer(Microsoft::WRL::ComPtr<ID3D11Buffer>& indexBuffer) { sRendererData->ParticleIndexBuffer = indexBuffer; }
		static void SetParticlesSRV(Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv) { sRendererData->ParticlesSRV = srv; }
		static void SetNrOfParticles(size_t particles) { sRendererData->NrOfParticlesToRender = particles; }
		static void SetParticleMaskTexture(Texture2D* maskTexture) { sRendererData->ParticleMaskTexture = maskTexture; }

		//Stats
		struct Statistics
		{
			uint32_t DrawCalls = 0;
			uint32_t QuadCount = 0;

			uint32_t GetTotalVertexCount() { return QuadCount * 4; }
			uint32_t GetTotalIndexCount() { return QuadCount * 6; }
		};

		static Statistics GetStats();
		static void ResetStats();

		static void GenerateSpecularBRDF();

		static void GeneratePrefilteredEnvMap(int faceIndex);
		static void GenerateIrradianceCubemap(int faceIndex);

		// Atmospheric Scattering helpers
		static void GenerateTransmittanceLUT(Planet* planet);
		static void GenerateMultiScatteringLUT(Planet* planet);

		// SSAO Stuff
		static DirectX::XMFLOAT3 SampleSSAONoiseTexture(uint32_t x, uint32_t y);
		static std::vector<DirectX::XMFLOAT4> GetSSAOKernel() { return sRendererData->SSAOKernel; }

		// Particle System TODO: This needs reworking!
		static void GenerateParticleBuffers();
		static void InvalidateParticleBuffers(size_t nrOfParticles, size_t maxNrOfParticles);
		static void FillParticleBuffer(std::vector<Particle>& particles);
	};
}