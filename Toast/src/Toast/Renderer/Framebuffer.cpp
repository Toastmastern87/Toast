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

	bool Framebuffer::IsIntegerFormat(TextureFormat format)
	{
		switch (format)
		{
		case TextureFormat::R32_SINT:
			return true;
		default:
			return false;
		}
	}

	int Framebuffer::ReadPixel(uint32_t x, uint32_t y, uint32_t RTIdx)
	{
		HRESULT result;
		D3D11_TEXTURE2D_DESC textureDesc = {};
		D3D11_TEXTURE2D_DESC sourceDesc;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> stagedTexture;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> stagedSRV;

		D3D11_BOX srcBox = { x, y, 0, x + 1, y + 1, 1 };

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		mColorTargets[RTIdx]->GetTexture()->GetDesc(&sourceDesc);

		// Coordinates out of bounds
		if (x >= sourceDesc.Width || y >= sourceDesc.Height)
			return 0;

		textureDesc.Width = 1;
		textureDesc.Height = 1;
		textureDesc.MipLevels = 1;
		textureDesc.ArraySize = 1;
		textureDesc.Format = (DXGI_FORMAT)TextureFormat::R32_SINT;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Usage = D3D11_USAGE_STAGING;
		textureDesc.BindFlags = 0;
		textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		textureDesc.MiscFlags = 0;
		textureDesc.Format = sourceDesc.Format;

		result = device->CreateTexture2D(&textureDesc, nullptr, &stagedTexture);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create 2D texture!");
		
		deviceContext->CopySubresourceRegion(stagedTexture.Get(), 0, 0, 0, 0, mColorTargets[RTIdx]->GetTexture().Get(), 0, &srcBox);
		
		D3D11_MAPPED_SUBRESOURCE msr;
		HRESULT hr = deviceContext->Map(stagedTexture.Get(), 0, D3D11_MAP_READ, 0, &msr);
		if (FAILED(hr))
			return 0;
		int* pixelValue = reinterpret_cast<int*>(msr.pData);
		deviceContext->Unmap(stagedTexture.Get(), 0);

		return *pixelValue;
	}

}