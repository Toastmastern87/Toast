#pragma once

#include "Toast/Core/Base.h"

#include "Toast/Renderer/Formats.h"
#include "Toast/Renderer/Texture.h"

#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl.h>

namespace Toast {

	enum class RenderTargetType
	{
		Color = 0,
		Depth = 1
	};

	class RenderTarget 
	{
	public:
		RenderTarget(RenderTargetType type, uint32_t width, uint32_t height, uint32_t samples, TextureFormat format, bool swapChainTarget = false);
		~RenderTarget() = default;

		void Init(RenderTargetType type, uint32_t width, uint32_t height, uint32_t samples, TextureFormat format, bool swapChainTarget = false);
		void Clear(const DirectX::XMFLOAT4 clearColor);
		void Clean();
		void Resize(uint32_t width, uint32_t height);

		std::tuple<uint32_t, uint32_t> GetSize() { return { mWitdh, mHeight }; }

		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> GetView();
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetSRV();
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> GetDepthView();

		Microsoft::WRL::ComPtr<ID3D11Texture2D> GetTexture() { return mTexture->GetTexture(); }
		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> GetDepthState() const { return mDepthStencilState; }

		TextureFormat GetFormat() { return mFormat; }

		void Unbind();
	private:
		RenderTargetType mType;
		uint32_t mWitdh, mHeight, mSamples;
		TextureFormat mFormat, mDepthFormat;

		bool mSwapChainTarget = false;

		Scope<Texture2D> mTexture;

		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> mRTV;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> mDSV;

		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> mSwapChainRTV;

		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> mDepthStencilState;
	};
}