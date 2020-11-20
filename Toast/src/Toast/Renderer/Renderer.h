#pragma once

#include "Toast/Renderer/RenderCommand.h"

#include "Toast/Renderer/OrthographicCamera.h"
#include "Toast/Scene/SceneCamera.h"
#include "Toast/Renderer/Mesh.h"

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
		static void SubmitPlanet(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, std::vector<float> distanceLUT = {}, DirectX::XMFLOAT4 morphRange = { 0.5f, 0.5f, 0.5f, 0.5f }, bool wireframe = false);

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