#pragma once

#include "Toast/Renderer/RendererAPI.h"

namespace Toast {

	class RenderCommand 
	{
	public:
		static void Init()
		{
			sRendererAPI->Init();
		}

		static void Clear(const float clearColor[4])
		{
			sRendererAPI->Clear(clearColor);
		}

		static void SetRenderTargets() 
		{
			sRendererAPI->SetRenderTargets();
		}

		static void DrawIndexed(const Ref<IndexBuffer>& indexBuffer, uint32_t count = 0)
		{
			sRendererAPI->DrawIndexed(indexBuffer, count);
		}

		static void SwapBuffers()
		{
			sRendererAPI->SwapBuffers();
		}

		static void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
		{
			sRendererAPI->SetViewport(x, y, width, height);
		}

		static void CleanUp()
		{
			sRendererAPI->CleanUp();
		}

	public:
		static Scope<RendererAPI> sRendererAPI;
	};
}