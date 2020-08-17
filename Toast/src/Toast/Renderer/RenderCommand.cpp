#include "tpch.h"
#include "Toast/Renderer/RenderCommand.h"

#include "Platform/DirectX/DirectXRendererAPI.h"

namespace Toast {

	Scope<RendererAPI> RenderCommand::sRendererAPI = RendererAPI::Create();
}