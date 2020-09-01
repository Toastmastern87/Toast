#pragma once

#include <d3d11.h>

#include "Toast/Renderer/Framebuffer.h"

namespace Toast {

	class DirectXFramebuffer : public Framebuffer
	{
	public:
		DirectXFramebuffer(const FramebufferSpecification& spec);
		virtual ~DirectXFramebuffer();

		virtual void* ColorAttachmentID() const override { return (void*)mShaderResourceView; }

		ID3D11RenderTargetView** GetRenderTargetView() { return &mRenderTargetView; }

		virtual const FramebufferSpecification& GetSpecification() const override { return mSpecification; }

		void Invalidate();
		virtual void Clear(const float clearColor[4]) override;
	private:
		FramebufferSpecification mSpecification;

		ID3D11Device* mDevice = nullptr;
		ID3D11DeviceContext* mDeviceContext = nullptr;

		ID3D11Texture2D* mRenderTargetTexture = nullptr;
		ID3D11RenderTargetView* mRenderTargetView = nullptr;
		ID3D11ShaderResourceView* mShaderResourceView = nullptr;
	};
}