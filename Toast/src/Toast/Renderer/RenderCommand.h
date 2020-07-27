#pragma once

#include "RendererAPI.h"

namespace Toast {

	class RenderCommand 
	{
	public:
		inline static void Clear(const float clearColor[4])
		{
			sRendererAPI->Clear(clearColor);
		}

		inline static void SetRenderTargets() 
		{
			sRendererAPI->SetRenderTargets();
		}

		inline static void DrawIndexed(const Ref<IndexBuffer>& indexBuffer)
		{
			sRendererAPI->DrawIndexed(indexBuffer);
		}

	private:
		static RendererAPI* sRendererAPI;
	};
}