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

		static void BindBackbuffer()
		{
			sRendererAPI->BindBackbuffer();
		}

		static void DrawIndexed(const Ref<IndexBuffer>& indexBuffer, uint32_t count = 0)
		{
			sRendererAPI->DrawIndexed(indexBuffer, count);
		}

		static void SwapBuffers()
		{
			sRendererAPI->SwapBuffers();
		}

		static void ResizeViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
		{
			sRendererAPI->ResizeViewport(x, y, width, height);
		}

		static void CleanUp()
		{
			sRendererAPI->CleanUp();
		}

	public:
		static Scope<RendererAPI> sRendererAPI;
	};
}