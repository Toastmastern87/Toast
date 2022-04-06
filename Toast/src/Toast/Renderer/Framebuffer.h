#pragma once

#include "Toast/Core/Base.h"
#include "Toast/Renderer/RenderTarget.h"
#include "Toast/Renderer/Formats.h"

#include <d3d11.h>
#include <wrl.h>
#include <DirectXMath.h>

namespace Toast {

	class Framebuffer
	{
	public:
		Framebuffer(Ref<RenderTarget> color, Ref<RenderTarget> depth = nullptr, bool swapChainTarget = false);
		~Framebuffer() = default;

		void CreateDepthDisabledState();

		void Bind() const;
		void Invalidate();

		void Resize(uint32_t width, uint32_t height);
		int ReadPixel(uint32_t x, uint32_t y);

		void DisableDepth() { mDepth = false; }
		void EnableDepth() { mDepth = true; }

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetSRV(uint32_t index = 0) const { return mColorTarget->GetSRV(); }
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetDepthSRV(uint32_t index = 0) const { return mDepthTarget->GetSRV(); }
		Microsoft::WRL::ComPtr<ID3D11Texture2D> GetRTTexture(uint32_t index = 0) const { return mColorTarget->GetTexture(); }


		virtual void Clear(const DirectX::XMFLOAT4 clearColor);
	private:
		bool mSwapChainTarget = false, mDepth;
		uint32_t mWidth, mHeight;

		Ref<RenderTarget> mColorTarget;
		Ref<RenderTarget> mDepthTarget;

		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> mDepthDisabledStencilState;

		D3D11_VIEWPORT mViewport;
	};
}