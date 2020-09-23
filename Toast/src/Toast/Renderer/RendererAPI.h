#pragma once

#include "Toast/Renderer/Buffer.h"
#include "Toast/Renderer/Framebuffer.h"

namespace Toast {

	class RendererAPI 
	{
	public:
		enum class API
		{
			None = 0, DirectX = 1
		};
	
	public:
		virtual ~RendererAPI() = default;

		virtual void Init() = 0;
		virtual void Clear(const DirectX::XMFLOAT4 clearColor) = 0;
		virtual void BindBackbuffer() = 0;
		virtual void DrawIndexed(const Ref<IndexBuffer>& indexBuffer, uint32_t indexCount = 0) = 0;
		virtual void Draw(uint32_t count) = 0;
		virtual void SwapBuffers() = 0;
		virtual void ResizeViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
		virtual void EnableAlphaBlending() = 0;
		virtual void CleanUp() = 0;

		static API GetAPI() { return sAPI; }
		static Scope<RendererAPI> Create();
	private:
		static API sAPI;
	};
}