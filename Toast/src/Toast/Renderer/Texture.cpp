#include "tpch.h"
#include "Texture.h"

#include "Renderer.h"
#include "Platform/DirectX/DirectXTexture.h"

namespace Toast {

	Ref<Texture2D> Texture2D::Create(const std::string& path) 
	{
		switch (RendererAPI::GetAPI())
		{
			case RendererAPI::API::None:			TOAST_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::DirectX:			return std::make_shared<DirectXTexture2D>(path);
		}

		TOAST_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}