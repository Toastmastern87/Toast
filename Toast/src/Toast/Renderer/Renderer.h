#pragma once

#include "Toast/Renderer/RenderCommand.h"
#include "Toast/Renderer/OrthographicCamera.h"
#include "Toast/Renderer/Mesh.h"
#include "Toast/Renderer/SceneEnvironment.h"

#include "Toast/Scene/Scene.h"
#include "Toast/Scene/SceneCamera.h"
#include "Toast/Scene/Components.h"

namespace Toast {

	class Renderer 
	{
	public:
		static void Init();
		static void Shutdown();

		static void OnWindowResize(uint32_t width, uint32_t height);

		static void BeginScene(const Scene* scene, const Camera& camera, const DirectX::XMMATRIX& viewMatrix, const DirectX::XMFLOAT4 cameraPos);
		static void EndScene();

		static void Submit(const Ref<IndexBuffer>& indexBuffer, const Ref<Shader> shader, const Ref<BufferLayout> bufferLayout, const Ref<VertexBuffer> vertexBuffer, const DirectX::XMMATRIX& transform);
		static void SubmitSkybox(const Ref<Mesh> skybox, const DirectX::XMFLOAT4& cameraPos, const DirectX::XMMATRIX& viewMatrix, const DirectX::XMMATRIX& projectionMatrix, float intensity, float LOD);
		static void SubmitMesh(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, const int entityID, bool wireframe = false);
		static void SubmitPlanet(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, const int entityID, PlanetComponent::PlanetGPUData planetData, bool wireframe = false);

		static std::pair<Ref<TextureCube>, Ref<TextureCube>> CreateEnvironmentMap(const std::string& filepath);

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
	private:
		struct SceneRendererData 
		{
			struct SceneInfo
			{
				Environment SceneEnvironment;
				float SceneEnvironmentIntensity;
				LightEnvironment SceneLightEnvironment;

			} SceneData;

			DirectX::XMMATRIX viewMatrix, projectionMatrix, inverseViewMatrix;
			DirectX::XMFLOAT4 cameraPos;
		};

		static Scope<SceneRendererData> mSceneRendererData;
	};
}