#include "tpch.h"
#include "Toast/Renderer/RenderCommand.h"
#include "Toast/Renderer/RendererAPI.h"

namespace Toast {

	Scope<RendererAPI> RenderCommand::sRendererAPI = CreateScope<RendererAPI>();
}