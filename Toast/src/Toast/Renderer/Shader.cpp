#include "tpch.h"
#include "tpch.h"
#include "Shader.h"

#include "Renderer.h"

#include "Platform/DirectX/DirectXShader.h"

namespace Toast {

	Shader* Shader::Create(const std::string& filepath)
	{
		switch (RendererAPI::GetAPI())
		{
			case RendererAPI::API::None:			TOAST_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::DirectX:			return new DirectXShader(filepath);
		}

		TOAST_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}