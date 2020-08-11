#include "tpch.h"
#include "RenderCommand.h"

#include "Platform/DirectX/DirectXRendererAPI.h"

namespace Toast {

	Scope<RendererAPI> RenderCommand::sRendererAPI = CreateScope<DirectXRendererAPI>();
}