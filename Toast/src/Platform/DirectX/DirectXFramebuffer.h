#pragma once

#include <d3d11.h>

#include "Toast/Renderer/Framebuffer.h"

namespace Toast {

	class DirectXFramebuffer : public Framebuffer
	{
	public:
		DirectXFramebuffer(const FramebufferSpecification& spec);
		virtual ~DirectXFramebuffer();

		virtual void Bind() const override;

		virtual void Resize(uint32_t width, uint32_t height) override;

		virtual void* GetColorAttachmentID() const override { return (void*)mShaderResourceView; }
		virtual void* GetDepthAttachmentID() const override { return (void*)mDepthStencilView; }

		virtual const FramebufferSpecification& GetSpecification() const override { return mSpecification; }

		void Invalidate();
		void Clean();
		virtual void Clear(const float clearColor[4]) override;
	private:
		bool IsDepthFormat(const FormatCode format);
		bool CreateDepthView(FramebufferSpecification::BufferDesc* desc);
		void CreateColorView(FramebufferSpecification::BufferDesc* desc);
		void CreateSwapChainView();
	private:
		FramebufferSpecification mSpecification;

		ID3D11Device* mDevice = nullptr;
		ID3D11DeviceContext* mDeviceContext = nullptr;

		//Render Target
		ID3D11Texture2D* mRenderTargetTexture = nullptr;
		ID3D11RenderTargetView* mRenderTargetView = nullptr;
		ID3D11ShaderResourceView* mShaderResourceView = nullptr;
		D3D11_VIEWPORT mViewport;

		//Depth
		ID3D11DepthStencilView* mDepthStencilView = nullptr;
		ID3D11DepthStencilState* mDepthStencilState = nullptr;
		ID3D11Texture2D* mDepthStencilBuffer = nullptr;
		bool mDepthView = false;
	};
}