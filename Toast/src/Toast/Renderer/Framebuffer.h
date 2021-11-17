#pragma once


#include "Toast/Core/Base.h"
#include "Toast/Renderer/Formats.h"

#include <d3d11.h>
#include <wrl.h>
#include <DirectXMath.h>

namespace Toast {

	enum class FramebufferTextureFormat
	{
		None = 0,

		// Color
		R32G32B32A32_FLOAT = 2,
		R8G8B8A8_UNORM = 28,
		R32_SINT = 43,

		// Depth/stencil
		D24_UNORM_S8_UINT = 45,

		// Default
		Depth = D24_UNORM_S8_UINT
	};

	struct FramebufferTextureSpecification 
	{
		FramebufferTextureSpecification() = default;
		FramebufferTextureSpecification(FramebufferTextureFormat format)
			: TextureFormat(format) {}

		FramebufferTextureFormat TextureFormat = FramebufferTextureFormat::None;
		// TODO : filtering/wrap
	};

	struct FramebufferAttachmentSpecification 
	{
		FramebufferAttachmentSpecification() = default;
		FramebufferAttachmentSpecification(std::initializer_list<FramebufferTextureSpecification> attachments)
			: Attachments(attachments) {}

		std::vector<FramebufferTextureSpecification> Attachments;
	};

	struct FramebufferSpecification
	{
		uint32_t Width = 0, Height = 0;
		FramebufferAttachmentSpecification Attachments;
		uint32_t Samples = 1;

		bool SwapChainTarget = false;
	};

	struct FramebufferColorAttachment 
	{
		Microsoft::WRL::ComPtr<ID3D11Texture2D> RenderTargetTexture;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> RenderTargetView;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ShaderResourceView;
	};

	struct FramebufferDepthAttachment 
	{
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> DepthStencilView;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> DepthStencilState;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> DepthStencilBuffer;
	};

	class Framebuffer
	{
	public:
		Framebuffer(const FramebufferSpecification& spec);
		~Framebuffer() = default;

		void Invalidate();

		void Bind() const;

		void Resize(uint32_t width, uint32_t height);
		int ReadPixel(uint32_t attachmentIndex, uint32_t x, uint32_t y);

		void* GetColorAttachmentID(uint32_t index = 0) const { return (void*)mColorAttachments[index].ShaderResourceView.Get(); }
		void* GetColorAttachmentIDNonMS(uint32_t index = 0);
		void* GetDepthAttachmentID() const { return (void*)mDepthAttachment.DepthStencilView.Get(); }

		const FramebufferSpecification& GetSpecification() const { return mSpecification; }

		void Clean();
		virtual void Clear(const DirectX::XMFLOAT4 clearColor);
	private:
		FramebufferSpecification mSpecification;

		std::vector<FramebufferTextureSpecification> mColorAttachmentSpecifications;
		std::vector<FramebufferColorAttachment> mColorAttachments;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mShaderResourceView;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> mRenderTargetView;

		//Depth
		FramebufferDepthAttachment mDepthAttachment;
		FramebufferTextureSpecification mDepthAttachmentSpecification = FramebufferTextureFormat::None;

		D3D11_VIEWPORT mViewport;
	};
}