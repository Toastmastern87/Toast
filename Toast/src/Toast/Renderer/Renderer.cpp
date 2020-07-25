#include "tpch.h"
#include "Renderer.h"

namespace Toast {

	Renderer::SceneData* Renderer::mSceneData = new Renderer::SceneData;

	void Renderer::BeginScene(OrthographicCamera& camera)
	{
		mSceneData->viewProjectionMatrix = camera.GetViewProjectionMatrix();
	}

	void Renderer::EndScene()
	{

	}

	void Renderer::Submit(const std::shared_ptr<IndexBuffer>& indexBuffer, const std::shared_ptr<Shader> shader, const std::shared_ptr<BufferLayout> bufferLayout, const std::shared_ptr<VertexBuffer> vertexBuffer, const DirectX::XMMATRIX& transform)
	{
		bufferLayout->Bind();
		vertexBuffer->Bind();
		indexBuffer->Bind();
		shader->Bind();
		shader->UploadConstantBuffer("Camera", mSceneData->viewProjectionMatrix);
		shader->UploadConstantBuffer("Transform", transform);

		RenderCommand::DrawIndexed(indexBuffer);
	}
}