#pragma once

#include "Toast/Renderer/RenderCommand.h"
#include "Toast/Renderer/OrthographicCamera.h"
#include "Toast/Renderer/Mesh.h"
#include "Toast/Renderer/SceneEnvironment.h"
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
			DrawCommand(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, const bool wireframe, int noWorldTransform = 1, const int entityID = 0, PlanetComponent::GPUData* planetData = nullptr)
				: Mesh(mesh), Transform(transform), Wireframe(wireframe), NoWorldTransform(noWorldTransform), EntityID(entityID), PlanetData(planetData) {}
		public:
			Ref<Mesh> Mesh;
			DirectX::XMMATRIX Transform;
			bool Wireframe;
			int NoWorldTransform;
			const int EntityID;
			PlanetComponent::GPUData* PlanetData;
		};

		struct RendererData
		{
			DirectX::XMFLOAT4 CameraPos;
			DirectX::XMFLOAT4X4 ViewMatrix;
			DirectX::XMFLOAT4X4 ProjectionMatrix;

			DirectX::XMMATRIX AtmosphericScatteringViewMatrices[6];
			DirectX::XMMATRIX AtmosphericScatteringInvViewMatrices[6];

			std::vector<DirectX::XMFLOAT4> SSAOKernel;

			struct SceneInfo
			{
				Environment SceneEnvironment;
				float SceneEnvironmentIntensity;
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

			Ref<ConstantBuffer> CameraCBuffer, LightningCBuffer, EnvironmentCBuffer, RenderSettingsCBuffer, AtmosphereCBuffer, ModelCBuffer, MaterialCBuffer, SpecularMapFilterSettingsCBuffer, SSAOCBuffer;
			Buffer CameraBuffer, LightningBuffer, EnvironmentBuffer, RenderSettingsBuffer, AtmosphereBuffer, ModelBuffer, MaterialBuffer, SpecularMapFilterSettingsBuffer, SSAOBuffer;

			// Back buffer
			Ref<RenderTarget> BackbufferRT;

			// Geometry Pass
			Ref<RenderTarget> GPassPositionRT, GPassNormalRT, GPassAlbedoMetallicRT, GPassRoughnessAORT, GPassPickingRT;

			// Shadow mapping Pass
			Ref<RenderTarget> ShadowMapRT;

			// SSAO Pass
			Ref<RenderTarget> SSAORT;
			
			// Lightning Pass
			Ref<RenderTarget> LPassRT;

			// Atmosphere pass
			Ref<RenderTarget> AtmospherePassRT;
			Ref<RenderTarget> AtmosphereCubeRT;

			// Environmental Textures
			Ref<TextureCube> EnvMapFiltered, IrradianceCubeMap;

			// Post Process
			Ref<RenderTarget> FinalRT;

			// Viewports
			D3D11_VIEWPORT Viewport, ShadowMapViewport, EditorViewport;

			// Rasterization states
			Microsoft::WRL::ComPtr<ID3D11RasterizerState> NormalRasterizerState, WireframeRasterizerState, ShadowMapRasterizerState;

			// Depth data
			Scope<Texture2D> DepthBuffer, ShadowPassDepth, SSAONoiseTexture;
			Microsoft::WRL::ComPtr<ID3D11DepthStencilState> DepthEnabledStencilState, DepthDisabledStencilState, DepthSkyboxPassStencilState, ShadowPassDepthStencilState;
			Microsoft::WRL::ComPtr<ID3D11DepthStencilView> DepthStencilView, ShadowPassStencilView;

			// Blend data
			Microsoft::WRL::ComPtr<ID3D11BlendState> GPassBlendState, LPassBlendState, AtmospherePassBlendState, PostProcessBlendState, UIBlendState;

			//Particle Data
			Microsoft::WRL::ComPtr<ID3D11Buffer> ParticleIndexBuffer;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ParticlesSRV;
			size_t NrOfParticlesToRender;
		};

	protected:
		static Scope<RendererData> sRendererData;

	public:
		static void Init(uint32_t width, uint32_t height);
		static void Shutdown();

		static void OnWindowResize(uint32_t width, uint32_t height);
		static void OnViewportResize(uint32_t width, uint32_t height);

		static void BeginScene(const Scene* scene, Camera& camera, const DirectX::XMFLOAT4 cameraPos);
		static void EndScene(const bool debugActivated, const bool shadows, const bool SSAO, const bool dynamicIBL);

		static void CreateDepthBuffer(uint32_t width, uint32_t height);
		static void CreateDepthStencilView();
		static void CreateDepthStencilStates();
		static void CreateBlendStates();
		static void CreateRasterizerStates();

		// SSAO
		static void GenerateSampleKernel();
		static void GenerateNoiseTexture();

		static void SetUpAtmosphericScatteringMatrices();

		static void Submit(const Ref<IndexBuffer>& indexBuffer, const Ref<Shader> shader, const Ref<ShaderLayout> bufferLayout, const Ref<VertexBuffer> vertexBuffer, const DirectX::XMMATRIX& transform);
		static void SubmitSkybox(const DirectX::XMFLOAT4& cameraPos, const DirectX::XMFLOAT4X4& viewMatrix, const DirectX::XMFLOAT4X4& projectionMatrix, float intensity, float LOD);
		static void SubmitMesh(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, const int entityID, bool wireframe = false, int noWorldTransform = 0, PlanetComponent::GPUData* planetData = nullptr, bool atmosphere = false);
		static void SubmitSelecetedMesh(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, bool wireframe = false);

		static void DrawFullscreenQuad();

		static void ClearDrawList();

		static Ref<TextureCube> CreateEnvironmentMap(const std::string& filepath);

		// Deffered Rendering
		static void GeometryPass();
		static void ShadowPass();
		static void LightningPass();
		static void ParticlesPass();
		static void SSAOPass();

		// Post Processes
		static void SkyboxPass();
		static void AtmospherePass(const bool dynamicIBL);
		static void PostProcessPass();

		static Ref<RenderTarget>& GetGPassPositionRT() { return sRendererData->GPassPositionRT; }
		static Ref<RenderTarget>& GetGPassNormalRT() { return sRendererData->GPassNormalRT; }
		static Ref<RenderTarget>& GetGPassAlbedoMetallicRT() { return sRendererData->GPassAlbedoMetallicRT; }
		static Ref<RenderTarget>& GetGPassRoughnessAORT() { return sRendererData->GPassRoughnessAORT; }
		static Ref<RenderTarget>& GetGPassPickingRT() { return sRendererData->GPassPickingRT; }

		static Ref<RenderTarget>& GetAtmosphericScatteringRT() { return sRendererData->AtmospherePassRT; }

		static Ref<RenderTarget>& GetSSAORT() { return sRendererData->SSAORT; }

		static Ref<RenderTarget>& GetLPassRT() { return sRendererData->LPassRT; }

		static Ref<RenderTarget>& GetFinalRT() { return sRendererData->FinalRT; }

		static void EnableAtmosphere(bool atmosphere) { sRendererData->PlanetData.Atmosphere = atmosphere; }

		static void SetParticlesIndexBuffer(Microsoft::WRL::ComPtr<ID3D11Buffer>& indexBuffer) { sRendererData->ParticleIndexBuffer = indexBuffer; }
		static void SetParticlesSRV(Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv) { sRendererData->ParticlesSRV = srv; }
		static void SetNrOfParticles(size_t particles) { sRendererData->NrOfParticlesToRender = particles; }

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

		static void GeneratePrefilteredEnvMap(int faceIndex);
		static void GenerateIrradianceCubemap(int faceIndex);

	};
}