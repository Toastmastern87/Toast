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

	std::vector<ID3D11RenderTargetView*>  Framebuffer::GetColorRenderTargets() const
	{
		std::vector<ID3D11RenderTargetView*> renderTargetViews;
		renderTargetViews.reserve(mColorTargets.size());
		for (const auto& colorTarget : mColorTargets)
		{
			renderTargetViews.emplace_back(colorTarget->GetView().Get());
		}

		return renderTargetViews;
	}

	void Framebuffer::Unbind() const
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		// Create arrays of null ptrs matching the number of render targets
		std::vector<ID3D11RenderTargetView*> nullRTVs(mColorTargets.size(), nullptr);

		// Unbind render targets and depth stencil view
		deviceContext->OMSetRenderTargets(static_cast<UINT>(nullRTVs.size()), nullRTVs.data(), nullptr);

		// Reset the depth stencil state to default (optional)
		deviceContext->OMSetDepthStencilState(nullptr, 1);

		// Reset the blend state to default (optional)
		deviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);

		// Unbind any shader resource views that might be bound to the pipeline (optional)
		// This is important if the render targets are also bound as SRVs in any shader stages

		// Unbind pixel shader SRVs
		ID3D11ShaderResourceView* nullSRVs[16] = { nullptr }; // Adjust the size if you have more than 16 SRVs
		deviceContext->PSSetShaderResources(0, 16, nullSRVs);

		// Unbind vertex shader SRVs (if applicable)
		deviceContext->VSSetShaderResources(0, 16, nullSRVs);

		// Unbind compute shader SRVs (if applicable)
		deviceContext->CSSetShaderResources(0, 16, nullSRVs);
	}

	void Framebuffer::Clear(const DirectX::XMFLOAT4 clearColor)
	{		
		for (auto& colorTarget : mColorTargets)
			colorTarget->Clear(clearColor);
	}

}