#include "tpch.h"
#include "Renderer.h"

#include "Platform/DirectX/DirectXContext.h"

namespace Toast {

	GraphicsContext* GraphicsContext::Create(HWND windowHandle, UINT width, UINT height)
	{
		switch (RendererAPI::GetAPI())
		{
			case RendererAPI::API::None:			TOAST_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::DirectX:			return new DirectXContext(windowHandle, width, height);
		}

		TOAST_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}