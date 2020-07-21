#include "tpch.h"
#include "RenderCommand.h"

#include "Platform/DirectX/DirectXRendererAPI.h"

namespace Toast {

	RendererAPI* RenderCommand::sRendererAPI = new DirectXRendererAPI;
}