#pragma once

#include "Toast/Renderer/RenderCommand.h"

#include "Toast/Renderer/OrthographicCamera.h"
#include "Toast/Scene/SceneCamera.h"
#include "Toast/Renderer/Mesh.h"
#include "Toast/Scene/Components.h"

namespace Toast {

	class Renderer 
	{
	public:
		static void Init();
		static void Shutdown();

		static void OnWindowResize(uint32_t width, uint32_t height);

		static void BeginScene(const Camera& camera, const DirectX::XMMATRIX& viewMatrix, const DirectX::XMFLOAT4 cameraPos);
		static void EndScene();

		static void Submit(const Ref<IndexBuffer>& indexBuffer, const Ref<Shader> shader, const Ref<BufferLayout> bufferLayout, const Ref<VertexBuffer> vertexBuffer, const DirectX::XMMATRIX& transform);
		static void SubmitMesh(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, bool wireframe = false);
		static void SubmitPlanet(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, PlanetComponent::PlanetGPUData planetData, PlanetComponent::MorphGPUData morphData, bool wireframe = false);

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
		struct SceneData 
		{
			DirectX::XMMATRIX viewMatrix, projectionMatrix, inverseViewMatrix;
			DirectX::XMFLOAT4 cameraPos;
		};

		static Scope<SceneData> mSceneData;
	};
}