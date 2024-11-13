#pragma once

#include "Toast/Renderer/RenderCommand.h"
#include "Toast/Renderer/OrthographicCamera.h"
#include "Toast/Renderer/Mesh.h"
#include "Toast/Renderer/SceneEnvironment.h"
#include "Toast/Renderer/Framebuffer.h"
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

			struct SceneInfo
			{
				Environment SceneEnvironment;
				float SceneEnvironmentIntensity;
				LightEnvironment SceneLightEnvironment;

				struct SkyboxInfo
				{
					Ref<Mesh> Skybox;
					float Intensity;
					float LOD;
				} SkyboxData;

			} SceneData;

			struct PlanetInfo
			{
				bool Atmosphere = false;
			} PlanetData;

			//Ref<Framebuffer> BaseFramebuffer, FinalFramebuffer, OutlineFramebuffer, PostProcessFramebuffer;
			std::vector<DrawCommand> MeshDrawList, MeshSelectedDrawList, MeshColliderDrawList;

			Ref<ConstantBuffer> CameraCBuffer, LightningCBuffer, EnvironmentCBuffer, RenderSettingsCBuffer, AtmosphereCBuffer, ModelCBuffer, MaterialCBuffer;
			Buffer CameraBuffer, LightningBuffer, EnvironmentBuffer, RenderSettingsBuffer, AtmosphereBuffer, ModelBuffer, MaterialBuffer;

			//Ref<RenderTarget> BaseRenderTarget, FinalRenderTarget, OutlineRenderTarget, PostProcessRenderTarget;

			// Geometry Pass
			Ref<Framebuffer> GPassFramebuffer;
			Ref<RenderTarget> GPassPositionRT, GPassNormalRT, GPassAlbedoMetallicRT, GPassRoughnessAORT, GPassPickingRT, GPassDepthRT;
			
			// Lightning Pass
			Ref<Framebuffer> LPassFramebuffer;
			Ref<RenderTarget> LPassFinalRT;
		};

	protected:
		static Scope<RendererData> sRendererData;

	public:
		static void Init();
		static void Shutdown();

		static void OnWindowResize(uint32_t width, uint32_t height);

		static void BeginScene(const Scene* scene, Camera& camera, const DirectX::XMFLOAT4 cameraPos);
		static void EndScene(const bool debugActivated);

		static void Submit(const Ref<IndexBuffer>& indexBuffer, const Ref<Shader> shader, const Ref<ShaderLayout> bufferLayout, const Ref<VertexBuffer> vertexBuffer, const DirectX::XMMATRIX& transform);
		static void SubmitSkybox(const Ref<Mesh> skybox, const DirectX::XMFLOAT4& cameraPos, const DirectX::XMFLOAT4X4& viewMatrix, const DirectX::XMFLOAT4X4& projectionMatrix, float intensity, float LOD);
		static void SubmitMesh(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, const int entityID, bool wireframe = false, int noWorldTransform = 0, PlanetComponent::GPUData* planetData = nullptr, bool atmosphere = false);
		static void SubmitSelecetedMesh(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, bool wireframe = false);

		static void DrawFullscreenQuad();

		static void ClearDrawList();

		static std::pair<Ref<TextureCube>, Ref<TextureCube>> CreateEnvironmentMap(const std::string& filepath);

		// Deffered Rendering
		static void GeometryPass();
		static void LightningPass();

		static Ref<RenderTarget>& GetDepthRT() { return sRendererData->GPassDepthRT; }
		static Ref<RenderTarget>& GetGPassPositionRT() { return sRendererData->GPassPositionRT; }
		static Ref<RenderTarget>& GetGPassNormalRT() { return sRendererData->GPassNormalRT; }
		static Ref<RenderTarget>& GetGPassAlbedoMetallicRT() { return sRendererData->GPassAlbedoMetallicRT; }
		static Ref<RenderTarget>& GetGPassRoughnessAORT() { return sRendererData->GPassRoughnessAORT; }
		static Ref<RenderTarget>& GetGPassPickingRT() { return sRendererData->GPassPickingRT; }
		static Ref<RenderTarget>& GetDepthRenderTarget() { return sRendererData->GPassDepthRT; }

		static Ref<RenderTarget>& GetLPassRenderTarget() { return sRendererData->LPassFinalRT; }

		//static Ref<RenderTarget>& GetBaseRenderTarget() { return sRendererData->BaseRenderTarget; }
		//static Ref<RenderTarget>& GetFinalRenderTarget() { return sRendererData->FinalRenderTarget; }
		//static Ref<RenderTarget>& GetOutlineRenderTarget() { return sRendererData->OutlineRenderTarget; }
		//static Ref<RenderTarget>& GetPostProcessRenderTarget() { return sRendererData->PostProcessRenderTarget; }

		static Ref<Framebuffer>& GetGPassFramebuffer() { return sRendererData->GPassFramebuffer; }
		static Ref<Framebuffer>& GetLPassFramebuffer() { return sRendererData->LPassFramebuffer; }

		//static Ref<Framebuffer>& GetBaseFramebuffer() { return sRendererData->BaseFramebuffer; }
		//static Ref<Framebuffer>& GetFinalFramebuffer() { return sRendererData->FinalFramebuffer; }
		//static Ref<Framebuffer>& GetPickingFramebuffer() { return sRendererData->PickingFramebuffer; }
		//static Ref<Framebuffer>& GetOutlineFramebuffer() { return sRendererData->OutlineFramebuffer; }
		//static Ref<Framebuffer>& GetPostProcessFramebuffer() { return sRendererData->PostProcessFramebuffer; }

		static void EnableAtmosphere(bool atmosphere) { sRendererData->PlanetData.Atmosphere = atmosphere; }

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

	};
}