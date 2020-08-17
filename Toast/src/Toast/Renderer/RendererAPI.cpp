#include "tpch.h"
#include "Toast/Renderer/RendererAPI.h"

#include "Platform/DirectX/DirectXRendererAPI.h"

namespace Toast {

	RendererAPI::API RendererAPI::sAPI = RendererAPI::API::DirectX;

	Scope<RendererAPI> RendererAPI::Create() 
	{
		switch (sAPI)
		{
			case RendererAPI::API::None:		TOAST_CORE_ASSERT(false, "RendererAPI::None is currently not supported"); return nullptr;
			case RendererAPI::API::DirectX:		return CreateScope<DirectXRendererAPI>();
		}

		TOAST_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}