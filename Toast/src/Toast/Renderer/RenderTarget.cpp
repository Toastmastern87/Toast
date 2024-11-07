#include "tpch.h"

#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/RenderTarget.h"
#include "Toast/Renderer/RendererAPI.h"

namespace Toast {

	static const uint32_t sMaxRenderTargetSize = 8192;

	RenderTarget::RenderTarget(RenderTargetType type, uint32_t width, uint32_t height, uint32_t samples, TextureFormat format, bool swapChainTarget)
		: mType(type), mWitdh(width), mHeight(height), mSamples(samples), mFormat(format), mSwapChainTarget(swapChainTarget)
	{
		Init(type, width, height, samples, format, swapChainTarget);
	}

	void RenderTarget::Init(RenderTargetType type, uint32_t width, uint32_t height, uint32_t samples, TextureFormat format, bool swapChainTarget)
	{
		mWitdh = width;
		mHeight = height;

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		HRESULT result;

		if (type == RenderTargetType::Color && !mSwapChainTarget)
		{
			mTexture = CreateScope<Texture2D>((DXGI_FORMAT)format, (DXGI_FORMAT)format, width, height, D3D11_USAGE_DEFAULT, (D3D11_BIND_FLAG)(D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE), samples);

			D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};

			rtvDesc.Format = (DXGI_FORMAT)format;
			rtvDesc.ViewDimension = (samples > 1) ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;
			rtvDesc.Texture2D.MipSlice = 0;

			result = device->CreateRenderTargetView(mTexture->GetTexture().Get(), &rtvDesc, &mRTV);
			TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create render target view!");
		}
		else if (mSwapChainTarget)
		{
			Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;

			RendererAPI* API = RenderCommand::sRendererAPI.get();
			ID3D11Device* device = API->GetDevice();
			IDXGISwapChain* swapChain = API->GetSwapChain();

			swapChain->GetBuffer(0, __uuidof(ID3D11Resource), &backBuffer);
			device->CreateRenderTargetView(backBuffer.Get(), nullptr, &mSwapChainRTV);
		}
		else if (type == RenderTargetType::Depth)
		{
			mTexture = CreateScope<Texture2D>((DXGI_FORMAT)TextureFormat::R32_TYPELESS, (DXGI_FORMAT)TextureFormat::R32_FLOAT, width, height, D3D11_USAGE_DEFAULT, (D3D11_BIND_FLAG)(D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE), samples);

			D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
			dsvDesc.Format = (DXGI_FORMAT)format;
			dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
			dsvDesc.Texture2D.MipSlice = 0;

			result = device->CreateDepthStencilView(mTexture->GetTexture().Get(), &dsvDesc, &mDSV);
			TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create depth stencil view!");

			// Create Depth Stencil State
			D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
			depthStencilDesc.DepthEnable = true;
			depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
			depthStencilDesc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;

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
		}
	}

	void RenderTarget::Clear(const DirectX::XMFLOAT4 clearColor)
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		if(mType == RenderTargetType::Color)
			if (mSwapChainTarget)
			{
				deviceContext->ClearRenderTargetView(mSwapChainRTV.Get(), reinterpret_cast<const float*>(&clearColor));
			}
			else
			{
				deviceContext->ClearRenderTargetView(mRTV.Get(), reinterpret_cast<const float*>(&clearColor));
			}
		else if(mType == RenderTargetType::Depth)
			deviceContext->ClearDepthStencilView(mDSV.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.0f, 0);
			//deviceContext->ClearDepthStencilView(mDepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	}

	void RenderTarget::Clean()
	{
		mTexture.reset();
		mRTV.Reset();
		mDSV.Reset();
		mSwapChainRTV.Reset();
	}

	void RenderTarget::Resize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0 || width > sMaxRenderTargetSize || height > sMaxRenderTargetSize)
		{
			TOAST_CORE_WARN("Attempted to resize render target to %f, %f", width, height);
			return;
		}

		Clean();
		Init(mType, width, height, mSamples, mFormat);
	}

	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> RenderTarget::GetView()
	{
		if (mSwapChainTarget)
			return mSwapChainRTV;
		else
			return mRTV;
	}

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> RenderTarget::GetSRV()
	{
		return mTexture->GetSRV();
	}

	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> RenderTarget::GetDepthView()
	{
		return mDSV;
	}

	void RenderTarget::Unbind()
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		ID3D11RenderTargetView* nullRTV[1] = { nullptr };
		deviceContext->OMSetRenderTargets(1, nullRTV, nullptr);
	}

}