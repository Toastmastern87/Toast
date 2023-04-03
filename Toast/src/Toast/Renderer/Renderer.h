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
			DrawCommand(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, const bool wireframe, const int entityID = 0, PlanetComponent::PlanetGPUData* planetData = nullptr)
				: Mesh(mesh), Transform(transform), Wireframe(wireframe), EntityID(entityID), PlanetData(planetData) {}
		public:
			Ref<Mesh> Mesh;
			DirectX::XMMATRIX Transform;
			bool Wireframe;
			const int EntityID;
			PlanetComponent::PlanetGPUData* PlanetData;
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

			Ref<Framebuffer> BaseFramebuffer, FinalFramebuffer, PickingFramebuffer, OutlineFramebuffer, PostProcessFramebuffer, UIFramebuffer;
			std::vector<DrawCommand> MeshDrawList, MeshSelectedDrawList, MeshColliderDrawList;

			Ref<ConstantBuffer> CameraCBuffer, LightningCBuffer, EnvironmentCBuffer;
			Buffer CameraBuffer, LightningBuffer, EnvironmentBuffer;

			Ref<RenderTarget> BaseRenderTarget, FinalRenderTarget, DepthRenderTarget, PickingRenderTarget, OutlineRenderTarget, PostProcessRenderTarget, UIRenderTarget;
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
		static void SubmitMesh(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, const int entityID, bool wireframe = false, PlanetComponent::PlanetGPUData* planetData = nullptr, bool atmosphere = false);
		static void SubmitSelecetedMesh(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, bool wireframe = false);

		static void DrawFullscreenQuad();

		static void ClearDrawList();

		static std::pair<Ref<TextureCube>, Ref<TextureCube>> CreateEnvironmentMap(const std::string& filepath);

		static void BaseRenderPass();
		static void PickingRenderPass();
		static void PostProcessPass();

		static Ref<RenderTarget>& GetBaseRenderTarget() { return sRendererData->BaseRenderTarget; }
		static Ref<RenderTarget>& GetDepthRenderTarget() { return sRendererData->DepthRenderTarget; }
		static Ref<RenderTarget>& GetFinalRenderTarget() { return sRendererData->FinalRenderTarget; }
		static Ref<RenderTarget>& GetPickingRenderTarget() { return sRendererData->PickingRenderTarget; }
		static Ref<RenderTarget>& GetOutlineRenderTarget() { return sRendererData->OutlineRenderTarget; }
		static Ref<RenderTarget>& GetPostProcessRenderTarget() { return sRendererData->PostProcessRenderTarget; }
		static Ref<RenderTarget>& GetUIRenderTarget() { return sRendererData->UIRenderTarget; }

		static Ref<Framebuffer>& GetBaseFramebuffer() { return sRendererData->BaseFramebuffer; }
		static Ref<Framebuffer>& GetFinalFramebuffer() { return sRendererData->FinalFramebuffer; }
		static Ref<Framebuffer>& GetPickingFramebuffer() { return sRendererData->PickingFramebuffer; }
		static Ref<Framebuffer>& GetOutlineFramebuffer() { return sRendererData->OutlineFramebuffer; }
		static Ref<Framebuffer>& GetPostProcessFramebuffer() { return sRendererData->PostProcessFramebuffer; }
		static Ref<Framebuffer>& GetUIFramebuffer() { return sRendererData->UIFramebuffer; }

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