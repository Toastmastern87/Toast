#include "tpch.h"
#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/Framebuffer.h"
#include "Toast/Renderer/RendererAPI.h"


namespace Toast {

	static const uint32_t sMaxFramebufferSize = 8192;

	namespace Utils {

		static void AttachColorTexture(FramebufferTextureSpecification textureSpec, FramebufferSpecification framebufferSpec, FramebufferColorAttachment* outColorAttachment)
		{
			HRESULT result;
			D3D11_TEXTURE2D_DESC textureDesc;
			D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
			D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;

			RendererAPI* API = RenderCommand::sRendererAPI.get();
			ID3D11Device* device = API->GetDevice();

			textureDesc.Width = framebufferSpec.Width;
			textureDesc.Height = framebufferSpec.Height;
			textureDesc.MipLevels = 1;
			textureDesc.ArraySize = 1;
			textureDesc.Format = (DXGI_FORMAT)textureSpec.TextureFormat;
			textureDesc.SampleDesc.Count = 1;
			textureDesc.SampleDesc.Quality = 0;
			textureDesc.Usage = D3D11_USAGE_DEFAULT;
			textureDesc.BindFlags = (UINT)(TOAST_BIND_RENDER_TARGET | TOAST_BIND_SHADER_RESOURCE);
			textureDesc.CPUAccessFlags = 0;
			textureDesc.MiscFlags = 0;

			result = device->CreateTexture2D(&textureDesc, nullptr, &outColorAttachment->RenderTargetTexture);
			TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create 2D texture!");

			renderTargetViewDesc.Format = (DXGI_FORMAT)textureSpec.TextureFormat;
			renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			renderTargetViewDesc.Texture2D.MipSlice = 0;

			result = device->CreateRenderTargetView(outColorAttachment->RenderTargetTexture.Get(), &renderTargetViewDesc, &outColorAttachment->RenderTargetView);
			TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create render target view!");

			shaderResourceViewDesc.Format = (DXGI_FORMAT)textureSpec.TextureFormat;
			shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
			shaderResourceViewDesc.Texture2D.MipLevels = 1;

			result = device->CreateShaderResourceView(outColorAttachment->RenderTargetTexture.Get(), &shaderResourceViewDesc, &outColorAttachment->ShaderResourceView);
			TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create shader resource view!");
		}

		static void AttachDepthTexture(FramebufferTextureSpecification textureSpec, FramebufferSpecification framebufferSpec, FramebufferDepthAttachment* outDepthAttachment)
		{
			HRESULT result;
			D3D11_TEXTURE2D_DESC textureDesc;
			D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
			D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;

			RendererAPI* API = RenderCommand::sRendererAPI.get();
			ID3D11Device* device = API->GetDevice();

			textureDesc.Width = framebufferSpec.Width;
			textureDesc.Height = framebufferSpec.Height;
			textureDesc.MipLevels = 1;
			textureDesc.ArraySize = 1;
			textureDesc.Format = (DXGI_FORMAT)textureSpec.TextureFormat;
			textureDesc.SampleDesc.Count = 1;
			textureDesc.SampleDesc.Quality = 0;
			textureDesc.Usage = D3D11_USAGE_DEFAULT;
			textureDesc.BindFlags = (UINT)TOAST_BIND_DEPTH_STENCIL;
			textureDesc.CPUAccessFlags = 0;
			textureDesc.MiscFlags = 0;

			result = device->CreateTexture2D(&textureDesc, nullptr, &outDepthAttachment->DepthStencilBuffer);
			TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create back buffer");

			ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));

			depthStencilDesc.DepthEnable = true;
			depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
			depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

			depthStencilDesc.StencilEnable = true;
			depthStencilDesc.StencilReadMask = 0xFF;
			depthStencilDesc.StencilWriteMask = 0xFF;

			depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
			depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
			depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
			depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

			depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
			depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
			depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
			depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

			result = device->CreateDepthStencilState(&depthStencilDesc, &outDepthAttachment->DepthStencilState);
			TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create depth stencil state");

			ZeroMemory(&depthStencilViewDesc, sizeof(depthStencilViewDesc));

			depthStencilViewDesc.Format = (DXGI_FORMAT)textureSpec.TextureFormat;
			depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
			depthStencilViewDesc.Texture2D.MipSlice = 0;

			result = device->CreateDepthStencilView(outDepthAttachment->DepthStencilBuffer.Get(), &depthStencilViewDesc, &outDepthAttachment->DepthStencilView);
			TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create depth stencil view");
		}

		static void AttachToSwapchain(FramebufferColorAttachment* outColorAttachment)
		{
			Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;

			RendererAPI* API = RenderCommand::sRendererAPI.get();
			ID3D11Device* device = API->GetDevice();
			IDXGISwapChain* swapChain = API->GetSwapChain();

			swapChain->GetBuffer(0, __uuidof(ID3D11Resource), &backBuffer);
			device->CreateRenderTargetView(backBuffer.Get(), nullptr, &outColorAttachment->RenderTargetView);
		}

		static bool IsDepthFormat(FramebufferTextureFormat format)
		{
			switch (format)
			{
				case FramebufferTextureFormat::D24_UNORM_S8_UINT:	return true;
			}

			return false;
		}
	}

	Framebuffer::Framebuffer(const FramebufferSpecification& spec)
		: mSpecification(spec)
	{
		for (auto format : mSpecification.Attachments.Attachments)
		{
			if (!Utils::IsDepthFormat(format.TextureFormat)) 
			{
				TOAST_CORE_INFO("Attaching Color texture to framebuffer: {0}, ID: {1}", format.TextureFormat, mColorAttachmentSpecifications.size());
				mColorAttachmentSpecifications.emplace_back(format);
			}
			else
				mDepthAttachmentSpecification = format;
		}

		Invalidate();
	}

	void Framebuffer::Bind() const
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		deviceContext->RSSetViewports(1, &mViewport);

		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> pRenderViews[2];
		for (int i = 0; i < mColorAttachments.size(); i++)
		{
			pRenderViews[i] = mColorAttachments[i].RenderTargetView;
		}

		deviceContext->OMSetRenderTargets(mColorAttachments.size(), pRenderViews[0].GetAddressOf(), mDepthAttachment.DepthStencilView.Get());

		if (mDepthAttachmentSpecification.TextureFormat != FramebufferTextureFormat::None)
			deviceContext->OMSetDepthStencilState(mDepthAttachment.DepthStencilState.Get(), 1);
	}

	void Framebuffer::Invalidate()
	{
		Clean();

		mViewport.TopLeftX = 0.0f;
		mViewport.TopLeftY = 0.0f;
		mViewport.Width = static_cast<float>(mSpecification.Width);
		mViewport.Height = static_cast<float>(mSpecification.Height);
		mViewport.MinDepth = 0.0f;
		mViewport.MaxDepth = 1.0f;

		bool multiSample = mSpecification.Samples > 1;

		if (mColorAttachmentSpecifications.size())
		{
			mColorAttachments.resize(mColorAttachmentSpecifications.size());

			// Attachments
			for (size_t i = 0; i < mColorAttachments.size(); i++)
			{
				if (mSpecification.SwapChainTarget)
				{
					Utils::AttachToSwapchain(&mColorAttachments[i]);

					break;
				}

				switch (mColorAttachmentSpecifications[i].TextureFormat)
				{
					case FramebufferTextureFormat::R32G32B32A32_FLOAT: 
					{
						TOAST_CORE_INFO("Creating R32G32B32A32_FLOAT texture");
						Utils::AttachColorTexture(mColorAttachmentSpecifications[i], mSpecification, &mColorAttachments[i]);
						break;
					}
					case FramebufferTextureFormat::R8G8B8A8_UNORM:
					{
						TOAST_CORE_INFO("Creating R8G8B8A8_UNORM texture");
						Utils::AttachColorTexture(mColorAttachmentSpecifications[i], mSpecification, &mColorAttachments[i]);
						break;
					}
				}
			}
		}

		if (mDepthAttachmentSpecification.TextureFormat != FramebufferTextureFormat::None)
		{
			switch (mDepthAttachmentSpecification.TextureFormat)
			{
			case FramebufferTextureFormat::D24_UNORM_S8_UINT:
				Utils::AttachDepthTexture(mDepthAttachmentSpecification, mSpecification, &mDepthAttachment);
				break;
			}
		}
	}

	void Framebuffer::Clean()
	{
		for (size_t i = 0; i < mColorAttachments.size(); i++)
		{
			mColorAttachments[i].RenderTargetTexture.Reset();
			mColorAttachments[i].RenderTargetView.Reset();
			mColorAttachments[i].ShaderResourceView.Reset();
		}
		mColorAttachments.clear();

		mDepthAttachment.DepthStencilView.Reset();
		mDepthAttachment.DepthStencilState.Reset();
		mDepthAttachment.DepthStencilBuffer.Reset();
	}

	void Framebuffer::Clear(const DirectX::XMFLOAT4 clearColor)
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		for (size_t i = 0; i < mColorAttachments.size(); i++)
			deviceContext->ClearRenderTargetView(mColorAttachments[i].RenderTargetView.Get(), (float*)&clearColor);

		if (mDepthAttachmentSpecification.TextureFormat != FramebufferTextureFormat::None)
			deviceContext->ClearDepthStencilView(mDepthAttachment.DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	}

	void Framebuffer::Resize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0 || width > sMaxFramebufferSize || height > sMaxFramebufferSize)
		{
			TOAST_CORE_WARN("Attempted to resize framebuffer to {0}, {1}", width, height);
			return;
		}

		mSpecification.Width = width;
		mSpecification.Height = height;

		Invalidate();
	}
}