#pragma once

#include "RenderCommand.h"

#include "OrthographicCamera.h"

namespace Toast {

	class Renderer 
	{
	public:
		static void Init();

		static void BeginScene(OrthographicCamera& camera);
		static void EndScene();

		static void Submit(const Ref<IndexBuffer>& indexBuffer, const Ref<Shader> shader, const Ref<BufferLayout> bufferLayout, const Ref<VertexBuffer> vertexBuffer, const DirectX::XMMATRIX& transform);

		inline static RendererAPI::API GetAPI() { return RendererAPI::GetAPI(); }
	private:
		struct SceneData 
		{
			DirectX::XMMATRIX viewProjectionMatrix;
		};

		static SceneData* mSceneData;
	};
}