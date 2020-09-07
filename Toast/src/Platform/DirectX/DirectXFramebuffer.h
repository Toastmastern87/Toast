#pragma once

#include <d3d11.h>
#include <wrl.h>

#include "Toast/Renderer/Framebuffer.h"

namespace Toast {

	class DirectXFramebuffer : public Framebuffer
	{
	public:
		DirectXFramebuffer(const FramebufferSpecification& spec);
		virtual ~DirectXFramebuffer();

		virtual void Bind() const override;

		virtual void Resize(uint32_t width, uint32_t height) override;

		virtual void* GetColorAttachmentID() const override { return (void*)mShaderResourceView.Get(); }
		virtual void* GetDepthAttachmentID() const override { return (void*)mDepthStencilView.Get(); }

		virtual const FramebufferSpecification& GetSpecification() const override { return mSpecification; }

		void Invalidate();
		void Clean();
		virtual void Clear(const float clearColor[4]) override;
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