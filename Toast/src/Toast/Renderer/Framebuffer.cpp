#include "tpch.h"

#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/Framebuffer.h"
#include "Toast/Renderer/RendererAPI.h"


namespace Toast {

	static const uint32_t sMaxFramebufferSize = 8192;

	Framebuffer::Framebuffer(const std::vector<Ref<RenderTarget>>& colors, Ref<RenderTarget> depth, bool swapChainTarget)
		: mColorTargets(colors), mDepthTarget(depth), mSwapChainTarget(swapChainTarget)
	{
		mWidth = 1280;
		mHeight = 720;

		CreateDepthDisabledState();

		Invalidate();
	}

	void Framebuffer::Bind() const
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		deviceContext->RSSetViewports(1, &mViewport);

		std::vector<ID3D11RenderTargetView*> renderTargetViews;
		renderTargetViews.reserve(mColorTargets.size());
		for (const auto& colorTarget : mColorTargets)
		{
			renderTargetViews.emplace_back(colorTarget->GetView().Get());
		}

		if (mDepth)
		{
			deviceContext->OMSetRenderTargets(static_cast<UINT>(renderTargetViews.size()), renderTargetViews.data(), mDepthTarget->GetDepthView().Get());
			deviceContext->OMSetDepthStencilState(mDepthTarget->GetDepthState().Get(), 1);
		}
		else 
		{
			deviceContext->OMSetRenderTargets(static_cast<UINT>(renderTargetViews.size()), renderTargetViews.data(), nullptr);
			deviceContext->OMSetDepthStencilState(mDepthDisabledStencilState.Get(), 1);
		}
	}

	void Framebuffer::CreateDepthDisabledState()
	{
		HRESULT result;
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc;

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));

		depthStencilDesc.DepthEnable = false;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

		depthStencilDesc.StencilEnable = false;
		depthStencilDesc.StencilReadMask = 0x00;
		depthStencilDesc.StencilWriteMask = 0x00;

		depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
		depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

		depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
		depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

		result = device->CreateDepthStencilState(&depthStencilDesc, &mDepthDisabledStencilState);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create depth stencil state");
	}

	void Framebuffer::Invalidate()
	{
		mViewport.TopLeftX = 0.0f;
		mViewport.TopLeftY = 0.0f;
		mViewport.Width = static_cast<float>(mWidth);
		mViewport.Height = static_cast<float>(mHeight);
		mViewport.MinDepth = 0.0f;
		mViewport.MaxDepth = 1.0f;

		if (mDepthTarget)
			mDepth = true;
		else
			mDepth = false;

	}

	void Framebuffer::Clear(const DirectX::XMFLOAT4 clearColor)
	{		
		for (auto& colorTarget : mColorTargets)
			colorTarget->Clear(clearColor);

		if (mDepth)
			mDepthTarget->Clear(clearColor);
	}

	void Framebuffer::Resize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0 || width > sMaxFramebufferSize || height > sMaxFramebufferSize)
		{
			TOAST_CORE_WARN("Attempted to resize framebuffer to %f, %f", width, height);
			return;
		}

		mWidth = width;
		mHeight = height;

		for (auto& colorTarget : mColorTargets)
			colorTarget->Resize(width, height);

		if (mDepthTarget)
			mDepthTarget->Resize(width, height);

		Invalidate();
	}

	int Framebuffer::ReadPixel(uint32_t x, uint32_t y)
	{
		HRESULT result;
		D3D11_TEXTURE2D_DESC textureDesc;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> stagedTexture;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> stagedSRV;

		D3D11_BOX srcBox = { x, y, 0, x + 1, y + 1, 1 };

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

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

		result = device->CreateTexture2D(&textureDesc, nullptr, &stagedTexture);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create 2D texture!");
		
		deviceContext->CopySubresourceRegion(stagedTexture.Get(), 0, 0, 0, 0, mColorTargets[0]->GetTexture().Get(), 0, &srcBox);
		
		D3D11_MAPPED_SUBRESOURCE msr;
		deviceContext->Map(stagedTexture.Get(), 0, D3D11_MAP_READ, 0, &msr);
		int* pixelValue = reinterpret_cast<int*>(msr.pData);
		deviceContext->Unmap(stagedTexture.Get(), 0);

		return *pixelValue;
	}

}