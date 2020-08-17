#pragma once

#include "Toast/Renderer/Buffer.h"

namespace Toast {

	class RendererAPI 
	{
	public:
		enum class API
		{
			None = 0, DirectX = 1
		};
	
	public:
		virtual void Init() = 0;
		virtual void Clear(const float clearColor[4]) = 0;
		virtual void SetRenderTargets() = 0;
		virtual void DrawIndexed(const Ref<IndexBuffer>& indexBuffer) = 0;
		virtual void SwapBuffers() = 0;
		virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
		virtual void EnableAlphaBlending() = 0;
		virtual void EnableDepthTesting() = 0;
		virtual void CleanUp() = 0;

		inline static API GetAPI() { return sAPI; }
		static Scope<RendererAPI> Create();
	private:
		static API sAPI;
	};
}