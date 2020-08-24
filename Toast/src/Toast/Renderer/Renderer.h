#pragma once

#include "Toast/Renderer/RenderCommand.h"

#include "Toast/Renderer/OrthographicCamera.h"

namespace Toast {

	class Renderer 
	{
	public:
		static void Init();
		static void Shutdown();

		static void OnWindowResize(uint32_t width, uint32_t height);

		static void BeginScene(OrthographicCamera& camera);
		static void EndScene();

		static void Submit(const Ref<IndexBuffer>& indexBuffer, const Ref<Shader> shader, const Ref<BufferLayout> bufferLayout, const Ref<VertexBuffer> vertexBuffer, const DirectX::XMMATRIX& transform);

		static RendererAPI::API GetAPI() { return RendererAPI::GetAPI(); }
	private:
		struct SceneData 
		{
			DirectX::XMMATRIX viewProjectionMatrix;
		};

		static Scope<SceneData> mSceneData;
	};
}