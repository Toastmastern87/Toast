#include "tpch.h"
#include "Buffer.h"

#include "Renderer.h"

#include "Platform/DirectX/DirectXBuffer.h"

namespace Toast {

	VertexBuffer* VertexBuffer::Create(float* vertices, uint32_t size)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::None:			TOAST_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::DirectX:		return new DirectXVertexBuffer(vertices, size);
		}
			
		TOAST_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	IndexBuffer* IndexBuffer::Create(uint32_t* indices, uint32_t size)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::None:			TOAST_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
		case RendererAPI::DirectX:		return new DirectXIndexBuffer(indices, size);
		}

		TOAST_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}