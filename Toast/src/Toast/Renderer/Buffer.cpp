#include "tpch.h"
#include "Buffer.h"

#include "Toast/Renderer/Buffer.h"

#include "Platform/DirectX/DirectXBuffer.h"

#include "Toast/Renderer/Renderer.h"

namespace Toast {

	Ref<BufferLayout> BufferLayout::Create(const std::initializer_list<BufferElement>& elements, Ref<Shader> shader)
	{
		switch (RendererAPI::GetAPI())
		{
			case RendererAPI::API::None:			TOAST_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::DirectX:			return CreateRef<DirectXBufferLayout>(elements, shader);
		}

		TOAST_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	Ref<VertexBuffer> VertexBuffer::Create(uint32_t size, uint32_t count)
	{
		switch (RendererAPI::GetAPI())
		{
			case RendererAPI::API::None:			TOAST_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::DirectX:			return CreateRef<DirectXVertexBuffer>(size, count);
		}

		TOAST_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	Ref<VertexBuffer> VertexBuffer::Create(float* vertices, uint32_t size, uint32_t count)
	{
		switch (RendererAPI::GetAPI())
		{
			case RendererAPI::API::None:			TOAST_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::DirectX:			return CreateRef<DirectXVertexBuffer>(vertices, size, count);
		}
			
		TOAST_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	Ref<IndexBuffer> IndexBuffer::Create(uint32_t* indices, uint32_t size)
	{
		switch (RendererAPI::GetAPI())
		{
			case RendererAPI::API::None:			TOAST_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::DirectX:			return CreateRef<DirectXIndexBuffer>(indices, size);
		}

		TOAST_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}