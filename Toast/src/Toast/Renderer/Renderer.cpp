#include "tpch.h"
#include "Renderer.h"

namespace Toast {

	void Renderer::BeginScene()
	{

	}

	void Renderer::EndScene()
	{

	}

	void Renderer::Submit(const std::shared_ptr<IndexBuffer>& indexBuffer)
	{
		RenderCommand::DrawIndexed(indexBuffer);
	}
}