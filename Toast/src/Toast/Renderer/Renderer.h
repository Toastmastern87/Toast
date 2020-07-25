#pragma once

#include "RenderCommand.h"

#include "OrthographicCamera.h"

namespace Toast {

	class Renderer 
	{
	public:
		static void BeginScene(OrthographicCamera& camera);
		static void EndScene();

		static void Submit(const std::shared_ptr<IndexBuffer>& indexBuffer, const std::shared_ptr<Shader> shader, const std::shared_ptr<BufferLayout> bufferLayout, const std::shared_ptr<VertexBuffer> vertexBuffer, const DirectX::XMMATRIX& transform);

		inline static RendererAPI::API GetAPI() { return RendererAPI::GetAPI(); }
	private:
		struct SceneData 
		{
			DirectX::XMMATRIX viewProjectionMatrix;
		};

		static SceneData* mSceneData;
	};
}