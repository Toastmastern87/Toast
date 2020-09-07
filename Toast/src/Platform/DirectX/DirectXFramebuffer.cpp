#include "tpch.h"
#include "DirectXFramebuffer.h"

#include "DirectXRendererAPI.h"
#include "Toast/Renderer/Renderer.h"

namespace Toast {

	DirectXFramebuffer::DirectXFramebuffer(const FramebufferSpecification& spec)
		: mSpecification(spec)
	{
		Invalidate();
	}

	DirectXFramebuffer::~DirectXFramebuffer()
	{

	}

	void DirectXFramebuffer::Bind() const
	{
		DirectXRendererAPI* API = static_cast<DirectXRendererAPI*>(RenderCommand::sRendererAPI.get());
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		deviceContext->RSSetViewports(1, &mViewport);

		deviceContext->OMSetRenderTargets(1, mRenderTargetView.GetAddressOf(), mDepthStencilView.Get());

		if(mDepthView)
			deviceContext->OMSetDepthStencilState(mDepthStencilState.Get(), 1);
	}

	void DirectXFramebuffer::Invalidate()
	{
		Clean();

		mViewport.TopLeftX = 0.0f;
		mViewport.TopLeftY = 0.0f;
		mViewport.Width = static_cast<float>(mSpecification.Width);
		mViewport.Height = static_cast<float>(mSpecification.Height);
		mViewport.MinDepth = 0.0f;
		mViewport.MaxDepth = 1.0f;

		for (FramebufferSpecification::BufferDesc desc : mSpecification.BuffersDesc)
		{
			if (IsDepthFormat(desc.Format))
				mDepthView = CreateDepthView(desc);
			else
				if (mSpecification.SwapChainTarget)
					CreateSwapChainView();
				else
					CreateColorView(desc);
		}
	}

	void DirectXFramebuffer::Clean()
	{
		mRenderTargetTexture.Reset();
		mRenderTargetView.Reset();
		mShaderResourceView.Reset();

		mDepthStencilView.Reset();
		mDepthStencilState.Reset();
		mDepthStencilBuffer.Reset();
	}

	void DirectXFramebuffer::Clear(const float clearColor[4])
	{
		DirectXRendererAPI* API = static_cast<DirectXRendererAPI*>(RenderCommand::sRendererAPI.get());
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		deviceContext->ClearRenderTargetView(mRenderTargetView.Get(), clearColor);

		if(mDepthView)
			deviceContext->ClearDepthStencilView(mDepthStencilView.Get(), D3D11_CLEAR_FLAG::D3D11_CLEAR_DEPTH, 1.0f, 0);
	}

	bool DirectXFramebuffer::IsDepthFormat(const FormatCode format)
	{
		if (format == FormatCode::D24_UNORM_S8_UINT)
			return true;
		else
			return false;
	}

	bool DirectXFramebuffer::CreateDepthView(FramebufferSpecification::BufferDesc desc)
	{
		HRESULT result;
		D3D11_TEXTURE2D_DESC textureDesc;
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
		D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;

		DirectXRendererAPI* API = static_cast<DirectXRendererAPI*>(RenderCommand::sRendererAPI.get());
		ID3D11Device* device = API->GetDevice();

		textureDesc.Width = mSpecification.Width;
		textureDesc.Height = mSpecification.Height;
		textureDesc.MipLevels = 1;
		textureDesc.ArraySize = 1;
		textureDesc.Format = (DXGI_FORMAT)desc.Format;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Usage = D3D11_USAGE_DEFAULT;
		textureDesc.BindFlags = (UINT)desc.BindFlags;
		textureDesc.CPUAccessFlags = 0;
		textureDesc.MiscFlags = 0;

		result = device->CreateTexture2D(&textureDesc, nullptr, &mDepthStencilBuffer);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create back buffer");

		ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));

		depthStencilDesc.DepthEnable = true;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;

		depthStencilDesc.StencilEnable = true;
		depthStencilDesc.StencilReadMask = 0xFF;
		depthStencilDesc.StencilWriteMask = 0xFF;

		depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
		depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

		depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
		depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

		result = device->CreateDepthStencilState(&depthStencilDesc, &mDepthStencilState);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create depth stencil state");

		ZeroMemory(&depthStencilViewDesc, sizeof(depthStencilViewDesc));

		depthStencilViewDesc.Format = (DXGI_FORMAT)desc.Format;
		depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		depthStencilViewDesc.Texture2D.MipSlice = 0;

		result = device->CreateDepthStencilView(mDepthStencilBuffer.Get(), &depthStencilViewDesc, &mDepthStencilView);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create depth stencil view");

		return true;
	}

	void DirectXFramebuffer::CreateColorView(FramebufferSpecification::BufferDesc desc)
	{
		HRESULT result;
		D3D11_TEXTURE2D_DESC textureDesc;
		D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
		D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;

		DirectXRendererAPI* API = static_cast<DirectXRendererAPI*>(RenderCommand::sRendererAPI.get());
		ID3D11Device* device = API->GetDevice();

		textureDesc.Width = mSpecification.Width;
		textureDesc.Height = mSpecification.Height;
		textureDesc.MipLevels = 1;
		textureDesc.ArraySize = 1;
		textureDesc.Format = (DXGI_FORMAT)desc.Format;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Usage = D3D11_USAGE_DEFAULT;
		textureDesc.BindFlags = (UINT)desc.BindFlags;
		textureDesc.CPUAccessFlags = 0;
		textureDesc.MiscFlags = 0;

		result = device->CreateTexture2D(&textureDesc, NULL, &mRenderTargetTexture);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create 2D texture!");

		renderTargetViewDesc.Format = (DXGI_FORMAT)desc.Format;
		renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		renderTargetViewDesc.Texture2D.MipSlice = 0;

		result = device->CreateRenderTargetView(mRenderTargetTexture.Get(), &renderTargetViewDesc, &mRenderTargetView);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create render target view!");

		shaderResourceViewDesc.Format = (DXGI_FORMAT)desc.Format;
		shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
		shaderResourceViewDesc.Texture2D.MipLevels = 1;

		result = device->CreateShaderResourceView(mRenderTargetTexture.Get(), &shaderResourceViewDesc, &mShaderResourceView);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create shader resource view!");
	}

	void DirectXFramebuffer::CreateSwapChainView()
	{
		Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;

		DirectXRendererAPI* API = static_cast<DirectXRendererAPI*>(RenderCommand::sRendererAPI.get());
		ID3D11Device* device = API->GetDevice();
		IDXGISwapChain* swapChain = API->GetSwapChain();

		swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
		device->CreateRenderTargetView(backBuffer.Get(), NULL, &mRenderTargetView);
	}

	void DirectXFramebuffer::Resize(uint32_t width, uint32_t height)
	{
		mSpecification.Width = width;
		mSpecification.Height = height;

		Invalidate();
	}
}