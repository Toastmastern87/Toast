#include "tpch.h"

#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/RenderTarget.h"
#include "Toast/Renderer/RendererAPI.h"

namespace Toast {

	static const uint32_t sMaxRenderTargetSize = 8192;

	RenderTarget::RenderTarget(RenderTargetType type, uint32_t width, uint32_t height, uint32_t samples, TextureFormat format, bool swapChainTarget, bool blending)
		: mType(type), mWitdh(width), mHeight(height), mSamples(samples), mFormat(format), mSwapChainTarget(swapChainTarget)
	{
		Init(type, width, height, samples, format, swapChainTarget);
	}

	void RenderTarget::Init(RenderTargetType type, uint32_t width, uint32_t height, uint32_t samples, TextureFormat format, bool swapChainTarget, bool blending)
	{
		mWitdh = width;
		mHeight = height;

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		HRESULT result;

		if (type == RenderTargetType::ColorCube)
		{
			mTexture = CreateScope<TextureCube>((DXGI_FORMAT)format, (DXGI_FORMAT)format, width, height, D3D11_USAGE_DEFAULT, (D3D11_BIND_FLAG)(D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE), samples);

			// Create render target views for each face
			for (int i = 0; i < 6; ++i)
			{
				D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
				rtvDesc.Format = (DXGI_FORMAT)format;
				rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
				rtvDesc.Texture2DArray.MipSlice = 0;
				rtvDesc.Texture2DArray.FirstArraySlice = i;
				rtvDesc.Texture2DArray.ArraySize = 1;

				result = device->CreateRenderTargetView(mTexture->GetTexture().Get(), &rtvDesc, &mRTVArray[i]);
				TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create render target view for cube face!");
			}
		}
		else if (type == RenderTargetType::Color && !mSwapChainTarget)
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
			device->CreateRenderTargetView(backBuffer.Get(), nullptr, &mRTV);
		}

		if (IsIntegerFormat(mFormat))
		{
			// Disable blending for integer formats
			mBlendDesc.BlendEnable = FALSE;
			mBlendDesc.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		}
		else
		{
			// Set default blending settings for floating-point formats
			mBlendDesc.BlendEnable = blending; 
			mBlendDesc.SrcBlend = D3D11_BLEND_ONE;
			mBlendDesc.DestBlend = D3D11_BLEND_ZERO;
			mBlendDesc.BlendOp = D3D11_BLEND_OP_ADD;
			mBlendDesc.SrcBlendAlpha = D3D11_BLEND_ONE;
			mBlendDesc.DestBlendAlpha = D3D11_BLEND_ZERO;
			mBlendDesc.BlendOpAlpha = D3D11_BLEND_OP_ADD;
			mBlendDesc.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		}
	}

	void RenderTarget::Clear(const DirectX::XMFLOAT4 clearColor)
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		if(mType == RenderTargetType::Color)
			if (mSwapChainTarget)
			{
				deviceContext->ClearRenderTargetView(mRTV.Get(), reinterpret_cast<const float*>(&clearColor));
			}
			else
			{
				deviceContext->ClearRenderTargetView(mRTV.Get(), reinterpret_cast<const float*>(&clearColor));
			}
	}

	void RenderTarget::Clean()
	{
		mTexture.reset();
		mRTV.Reset();
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

	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> RenderTarget::GetRTV()
	{
		return mRTV;
	}

	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> RenderTarget::GetRTVFace(uint32_t faceIndex)
	{
		TOAST_CORE_ASSERT(mType == RenderTargetType::ColorCube, "Not a cube render target!");
		TOAST_CORE_ASSERT(faceIndex < 6, "Invalid RTV face index!");
		return mRTVArray[faceIndex];
	}

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> RenderTarget::GetSRV()
	{
		return mTexture->GetSRV();
	}

	void RenderTarget::Unbind()
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		ID3D11RenderTargetView* nullRTV[1] = { nullptr };
		deviceContext->OMSetRenderTargets(1, nullRTV, nullptr);
	}

	bool RenderTarget::IsIntegerFormat(TextureFormat format)
	{
		switch (format)
		{
		case TextureFormat::R32_SINT:
			return true;
		default:
			return false;
		}
	}

}