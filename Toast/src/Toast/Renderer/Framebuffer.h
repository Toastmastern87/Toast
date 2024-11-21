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
		Framebuffer(const std::vector<Ref<RenderTarget>>& colors, bool swapChainTarget = false);
		~Framebuffer() = default;

		std::vector<ID3D11RenderTargetView*>  GetColorRenderTargets() const;
		std::vector<Ref<RenderTarget>>& GetRenderTargets() { return mColorTargets;  };

		void Unbind() const;

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetSRV(uint32_t index = 0) const { return mColorTargets[index]->GetSRV(); }
		Microsoft::WRL::ComPtr<ID3D11Texture2D> GetRTTexture(uint32_t index = 0) const { return mColorTargets[index]->GetTexture(); }

		virtual void Clear(const DirectX::XMFLOAT4 clearColor);
	private:
		bool mSwapChainTarget = false, mDepth;
		uint32_t mWidth, mHeight;

		std::vector<Ref<RenderTarget>> mColorTargets;
	};
}