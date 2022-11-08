#include "tpch.h"

#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/RenderTarget.h"
#include "Toast/Renderer/RendererAPI.h"

namespace Toast {

	static const uint32_t sMaxRenderTargetSize = 8192;

	RenderTarget::RenderTarget(RenderTargetType type, uint32_t width, uint32_t height, uint32_t samples, TextureFormat format, TextureFormat depthFormat, bool swapChainTarget)
		: mType(type), mWitdh(width), mHeight(height), mSamples(samples), mFormat(format), mDepthFormat(depthFormat), mSwapChainTarget(swapChainTarget)
	{
		Init(type, width, height, samples, format, depthFormat, swapChainTarget);
	}

	void RenderTarget::Init(RenderTargetType type, uint32_t width, uint32_t height, uint32_t samples, TextureFormat format, TextureFormat depthFormat, bool swapChainTarget)
	{
		HRESULT result;
		D3D11_TEXTURE2D_DESC textureDesc;
		D3D11_RENDER_TARGET_VIEW_DESC targetViewDesc;
		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
		D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;

		mWitdh = width;
		mHeight = height;

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();
		textureDesc.Width = width;
		textureDesc.Height = height;
		textureDesc.MipLevels = 1;
		textureDesc.ArraySize = 1;
		textureDesc.Format = (DXGI_FORMAT)format;
		textureDesc.SampleDesc.Count = samples;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Usage = D3D11_USAGE_DEFAULT;
		textureDesc.BindFlags = type == RenderTargetType::Color ? (UINT)(TOAST_BIND_RENDER_TARGET | TOAST_BIND_SHADER_RESOURCE) : (UINT)(TOAST_BIND_DEPTH_STENCIL | TOAST_BIND_SHADER_RESOURCE);
		textureDesc.CPUAccessFlags = 0;
		textureDesc.MiscFlags = 0;

		result = device->CreateTexture2D(&textureDesc, nullptr, mTargetTexture.GetAddressOf());
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create 2D texture!");

		if (type == RenderTargetType::Color && !mSwapChainTarget)
		{
			targetViewDesc.Format = (DXGI_FORMAT)format;
			targetViewDesc.ViewDimension = samples == 4 ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;
			targetViewDesc.Texture2D.MipSlice = 0;

			result = device->CreateRenderTargetView(mTargetTexture.Get(), &targetViewDesc, &mTargetView);
			TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create render target view!");

			SRVDesc.Format = (DXGI_FORMAT)format;
			SRVDesc.ViewDimension = samples == 4 ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D;
			SRVDesc.Texture2D.MostDetailedMip = 0;
			SRVDesc.Texture2D.MipLevels = 1;

			result = device->CreateShaderResourceView(mTargetTexture.Get(), &SRVDesc, &mTargetSRV);
			TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create shader resource view!");
		}
		else if (mSwapChainTarget)
		{
			Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;

			RendererAPI* API = RenderCommand::sRendererAPI.get();
			ID3D11Device* device = API->GetDevice();
			IDXGISwapChain* swapChain = API->GetSwapChain();

			swapChain->GetBuffer(0, __uuidof(ID3D11Resource), &backBuffer);
			device->CreateRenderTargetView(backBuffer.Get(), nullptr, &mTargetView);
		}
		else if (type == RenderTargetType::Depth)
		{
			ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));

			depthStencilDesc.DepthEnable = true;
			depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
			depthStencilDesc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
			//depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL; 

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

			depthStencilViewDesc.Format = (DXGI_FORMAT)depthFormat;
			depthStencilViewDesc.ViewDimension = samples == 4 ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;
			depthStencilViewDesc.Texture2D.MipSlice = 0;
			
			result = device->CreateDepthStencilView(mTargetTexture.Get(), &depthStencilViewDesc, &mDepthStencilView);
			TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create depth stencil view");

			SRVDesc.Format = (DXGI_FORMAT)TextureFormat::R32_FLOAT;
			SRVDesc.ViewDimension = samples == 4 ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D;
			SRVDesc.Texture2D.MostDetailedMip = 0;
			SRVDesc.Texture2D.MipLevels = 1;

			result = device->CreateShaderResourceView(mTargetTexture.Get(), &SRVDesc, &mTargetSRV);
			TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create shader resource view!");
		}
	}

	void RenderTarget::Clear(const DirectX::XMFLOAT4 clearColor)
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		if(mType == RenderTargetType::Color)
			deviceContext->ClearRenderTargetView(mTargetView.Get(), (float*)&clearColor);
		else if(mType == RenderTargetType::Depth)
			deviceContext->ClearDepthStencilView(mDepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.0f, 0);
			//deviceContext->ClearDepthStencilView(mDepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	}

	void RenderTarget::Clean()
	{
		mTargetTexture.Reset();
		mTargetView.Reset();
		mTargetSRV.Reset();

		mDepthStencilView.Reset();
		mDepthStencilState.Reset();
	}

	void RenderTarget::Resize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0 || width > sMaxRenderTargetSize || height > sMaxRenderTargetSize)
		{
			TOAST_CORE_WARN("Attempted to resize render target to %f, %f", width, height);
			return;
		}

		Clean();
		Init(mType, width, height, mSamples, mFormat, mDepthFormat);
	}

	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> RenderTarget::GetDepthView()
	{
		if (mType == RenderTargetType::Depth)
			return mDepthStencilView;
		else
			return nullptr;
	}

	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> RenderTarget::GetDepthState()
	{
		if (mType == RenderTargetType::Depth)
			return mDepthStencilState;
		else
			return nullptr;
	}

}