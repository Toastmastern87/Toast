#pragma once


#include "Toast/Core/Base.h"
#include "Toast/Renderer/Formats.h"

#include <d3d11.h>
#include <wrl.h>
#include <DirectXMath.h>

namespace Toast {

	struct FramebufferSpecification
	{
		uint32_t Width = 0, Height = 0;

		struct BufferDesc
		{
			FormatCode Format;
			BindFlag BindFlags;

			BufferDesc(FormatCode f, BindFlag b) : Format(f), BindFlags(b)
			{
			}

			BufferDesc() = default;
		};

		std::vector<BufferDesc> BuffersDesc;
		uint32_t Samples = 1;

		bool SwapChainTarget = false;
	};

	class Framebuffer
	{
	public:
		Framebuffer(const FramebufferSpecification& spec);
		~Framebuffer();

		void Bind() const;

		void Resize(uint32_t width, uint32_t height);

		void* GetColorAttachmentID() const { return (void*)mShaderResourceView.Get(); }
		void* GetDepthAttachmentID() const { return (void*)mDepthStencilView.Get(); }

		const FramebufferSpecification& GetSpecification() const { return mSpecification; }

		void Invalidate();
		void Clean();
		virtual void Clear(const DirectX::XMFLOAT4 clearColor);
	private:
		bool IsDepthFormat(const FormatCode format);
		bool CreateDepthView(FramebufferSpecification::BufferDesc desc);
		void CreateColorView(FramebufferSpecification::BufferDesc desc);
		void CreateSwapChainView();
	private:
		FramebufferSpecification mSpecification;

		//Render Target
		Microsoft::WRL::ComPtr<ID3D11Texture2D> mRenderTargetTexture;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> mRenderTargetView;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mShaderResourceView;
		D3D11_VIEWPORT mViewport;

		//Depth
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> mDepthStencilView;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> mDepthStencilState;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> mDepthStencilBuffer;
		bool mDepthView = false;
	};
}