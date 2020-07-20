#include "tpch.h"
#include "tpch.h"
#include "Shader.h"

#include "Renderer.h"

#include "Platform/DirectX/DirectXShader.h"

namespace Toast {

	Shader* Shader::Create(const std::string& vertexSrc, const std::string& pixelSrc)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::None:			TOAST_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::DirectX:		return new DirectXShader(vertexSrc, pixelSrc);
		}

		TOAST_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}