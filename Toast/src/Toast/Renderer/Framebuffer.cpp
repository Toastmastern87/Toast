#include "tpch.h"
#include "Framebuffer.h"

#include "Toast/Renderer/Renderer.h"

#include "Platform/DirectX/DirectXFramebuffer.h"

namespace Toast {

	Ref<Framebuffer> Framebuffer::Create(const FramebufferSpecification& spec)
	{
		switch (RendererAPI::GetAPI())
		{
			case RendererAPI::API::None:			TOAST_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::DirectX:			return CreateRef<DirectXFramebuffer>(spec);
		}

		TOAST_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}