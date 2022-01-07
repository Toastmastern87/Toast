#pragma once

#include "Toast/Renderer/RenderCommand.h"
#include "Toast/Renderer/OrthographicCamera.h"
#include "Toast/Renderer/Mesh.h"
#include "Toast/Renderer/SceneEnvironment.h"
#include "Toast/Renderer/Framebuffer.h"

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
				: Mesh(mesh), Transform(transform), Wireframe(wireframe), EntityID(entityID), PlanetData(planetData){}
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
			DirectX::XMMATRIX ViewMatrix;
			DirectX::XMMATRIX ProjectionMatrix;

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

			Ref<Framebuffer> BaseFramebuffer, PickingFramebuffer, OutlineFramebuffer;
			std::vector<DrawCommand> MeshDrawList, MeshSelectedDrawList;

			Ref<ConstantBuffer> CameraCBuffer, LightningCBuffer, EnvironmentCBuffer;
			Buffer CameraBuffer, LightningBuffer, EnvironmentBuffer;
		};

	protected:
		static Scope<RendererData> sRendererData;

	public:
		static void Init();
		static void Shutdown();

		static void OnWindowResize(uint32_t width, uint32_t height);

		static void BeginScene(const Scene* scene, const Camera& camera, const DirectX::XMMATRIX& viewMatrix, const DirectX::XMFLOAT4 cameraPos);
		static void EndScene(const bool debugActivated);

		static void Submit(const Ref<IndexBuffer>& indexBuffer, const Ref<Shader> shader, const Ref<ShaderLayout> bufferLayout, const Ref<VertexBuffer> vertexBuffer, const DirectX::XMMATRIX& transform);
		static void SubmitSkybox(const Ref<Mesh> skybox, const DirectX::XMFLOAT4& cameraPos, const DirectX::XMMATRIX& viewMatrix, const DirectX::XMMATRIX& projectionMatrix, float intensity, float LOD);
		static void SubmitMesh(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, const int entityID, bool wireframe = false, PlanetComponent::PlanetGPUData* planetData = nullptr);
		static void SubmitSelecetedMesh(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, bool wireframe = false);

		static void DrawFullscreenQuad();

		static void ClearDrawList();

		static std::pair<Ref<TextureCube>, Ref<TextureCube>> CreateEnvironmentMap(const std::string& filepath);

		static void BaseRenderPass();
		static void PickingRenderPass();
		static void PostProcessPass();

		static Ref<Framebuffer>& GetBaseFramebuffer() { return sRendererData->BaseFramebuffer; }
		static Ref<Framebuffer>& GetPickingFramebuffer() { return sRendererData->PickingFramebuffer; }
		static Ref<Framebuffer>& GetOutlineFramebuffer() { return sRendererData->OutlineFramebuffer; }

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