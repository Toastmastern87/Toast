#include "tpch.h"

#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/Framebuffer.h"
#include "Toast/Renderer/RendererAPI.h"

namespace Toast {

	static const uint32_t sMaxFramebufferSize = 8192;

	Framebuffer::Framebuffer(const std::vector<Ref<RenderTarget>>& colors, bool swapChainTarget)
		: mColorTargets(colors), mSwapChainTarget(swapChainTarget)
	{
		mWidth = 1280;
		mHeight = 720;
	}

	void Framebuffer::Clear(const DirectX::XMFLOAT4 clearColor)
	{		
		for (auto& colorTarget : mColorTargets)
			colorTarget->Clear(clearColor);
	}

}