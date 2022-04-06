#pragma once

#include "Toast/Core/Base.h"

#include "Toast/Renderer/Formats.h"

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
		RenderTarget(RenderTargetType type, uint32_t width, uint32_t height, uint32_t samples, TextureFormat format, TextureFormat depthFormat = TextureFormat::None, bool swapChainTarget = false);
		~RenderTarget() = default;

		void Init(RenderTargetType type, uint32_t width, uint32_t height, uint32_t samples, TextureFormat format, TextureFormat depthFormat = TextureFormat::None, bool swapChainTarget = false);
		void Clear(const DirectX::XMFLOAT4 clearColor);
		void Clean();
		void Resize(uint32_t width, uint32_t height);

		std::tuple<uint32_t, uint32_t> GetSize() { return { mWitdh, mHeight }; }

		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> GetView() { return mTargetView; }
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetSRV() { return mTargetSRV; }
		Microsoft::WRL::ComPtr<ID3D11Texture2D> GetTexture() { return mTargetTexture; }
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> GetDepthView();
		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> GetDepthState();

	private:
		RenderTargetType mType;
		uint32_t mWitdh, mHeight, mSamples;
		TextureFormat mFormat, mDepthFormat;

		bool mSwapChainTarget = false;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> mTargetTexture;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> mTargetView;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mTargetSRV;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> mDepthStencilView;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> mDepthStencilState;
	};
}