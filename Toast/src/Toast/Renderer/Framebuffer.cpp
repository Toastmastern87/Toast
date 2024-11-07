#include "tpch.h"

#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/Framebuffer.h"
#include "Toast/Renderer/RendererAPI.h"


namespace Toast {

	static const uint32_t sMaxFramebufferSize = 8192;

	Framebuffer::Framebuffer(const std::vector<Ref<RenderTarget>>& colors, Ref<RenderTarget> depth, bool swapChainTarget)
		: mColorTargets(colors), mDepthTarget(depth), mSwapChainTarget(swapChainTarget)
	{
		mWidth = 1280;
		mHeight = 720;

		// Initialize blend descriptions
		mBlendDescriptions.resize(mColorTargets.size());
		for (size_t i = 0; i < mColorTargets.size(); ++i)
		{
			TextureFormat format = mColorTargets[i]->GetFormat();
			D3D11_RENDER_TARGET_BLEND_DESC& rtBlendDesc = mBlendDescriptions[i];

			if (IsIntegerFormat(format))
			{
				// Disable blending for integer formats
				rtBlendDesc.BlendEnable = FALSE;
				rtBlendDesc.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
			}
			else
			{
				// Set blending settings for floating-point formats as needed
				rtBlendDesc.BlendEnable = FALSE; // Set to TRUE if blending is needed
				rtBlendDesc.SrcBlend = D3D11_BLEND_ONE;
				rtBlendDesc.DestBlend = D3D11_BLEND_ZERO;
				rtBlendDesc.BlendOp = D3D11_BLEND_OP_ADD;
				rtBlendDesc.SrcBlendAlpha = D3D11_BLEND_ONE;
				rtBlendDesc.DestBlendAlpha = D3D11_BLEND_ZERO;
				rtBlendDesc.BlendOpAlpha = D3D11_BLEND_OP_ADD;
				rtBlendDesc.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
			}
		}

		CreateDepthDisabledState();
		CreateBlendState();

		Invalidate();
	}

	void Framebuffer::Bind() const
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		deviceContext->RSSetViewports(1, &mViewport);

		std::vector<ID3D11RenderTargetView*> renderTargetViews;
		renderTargetViews.reserve(mColorTargets.size());
		for (const auto& colorTarget : mColorTargets)
		{
			renderTargetViews.emplace_back(colorTarget->GetView().Get());
		}

		if (mDepth)
		{
			deviceContext->OMSetRenderTargets(static_cast<UINT>(renderTargetViews.size()), renderTargetViews.data(), mDepthTarget->GetDepthView().Get());
			deviceContext->OMSetDepthStencilState(mDepthTarget->GetDepthState().Get(), 1);
		}
		else 
		{
			deviceContext->OMSetRenderTargets(static_cast<UINT>(renderTargetViews.size()), renderTargetViews.data(), nullptr);
			deviceContext->OMSetDepthStencilState(mDepthDisabledStencilState.Get(), 1);
		}

		// Bind the blend state
		const float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		deviceContext->OMSetBlendState(mBlendState.Get(), blendFactor, 0xFFFFFFFF);
	}

	void Framebuffer::Unbind() const
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		// Create arrays of null ptrs matching the number of render targets
		std::vector<ID3D11RenderTargetView*> nullRTVs(mColorTargets.size(), nullptr);

		// Unbind render targets and depth stencil view
		deviceContext->OMSetRenderTargets(static_cast<UINT>(nullRTVs.size()), nullRTVs.data(), nullptr);

		// Reset the depth stencil state to default (optional)
		deviceContext->OMSetDepthStencilState(nullptr, 1);

		// Reset the blend state to default (optional)
		deviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);

		// Unbind any shader resource views that might be bound to the pipeline (optional)
		// This is important if the render targets are also bound as SRVs in any shader stages

		// Unbind pixel shader SRVs
		ID3D11ShaderResourceView* nullSRVs[16] = { nullptr }; // Adjust the size if you have more than 16 SRVs
		deviceContext->PSSetShaderResources(0, 16, nullSRVs);

		// Unbind vertex shader SRVs (if applicable)
		deviceContext->VSSetShaderResources(0, 16, nullSRVs);

		// Unbind compute shader SRVs (if applicable)
		deviceContext->CSSetShaderResources(0, 16, nullSRVs);
	}

	void Framebuffer::CreateDepthDisabledState()
	{
		HRESULT result;
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc;

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));

		depthStencilDesc.DepthEnable = false;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

		depthStencilDesc.StencilEnable = false;
		depthStencilDesc.StencilReadMask = 0x00;
		depthStencilDesc.StencilWriteMask = 0x00;

		depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
		depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

		depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
		depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

		result = device->CreateDepthStencilState(&depthStencilDesc, &mDepthDisabledStencilState);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create depth stencil state");
	}

	void Framebuffer::Invalidate()
	{
		mViewport.TopLeftX = 0.0f;
		mViewport.TopLeftY = 0.0f;
		mViewport.Width = static_cast<float>(mWidth);
		mViewport.Height = static_cast<float>(mHeight);
		mViewport.MinDepth = 0.0f;
		mViewport.MaxDepth = 1.0f;

		if (mDepthTarget)
			mDepth = true;
		else
			mDepth = false;
	}

	void Framebuffer::Clear(const DirectX::XMFLOAT4 clearColor)
	{		
		for (auto& colorTarget : mColorTargets)
			colorTarget->Clear(clearColor);

		if (mDepth)
			mDepthTarget->Clear(clearColor);
	}

	void Framebuffer::CreateBlendState()
	{
		HRESULT result;
		D3D11_BLEND_DESC blendDesc = {};
		blendDesc.AlphaToCoverageEnable = FALSE;
		blendDesc.IndependentBlendEnable = TRUE; 

		if (mBlendDescriptions.size() != mColorTargets.size())
		{
			mBlendDescriptions.resize(mColorTargets.size());
			// Initialize default blend descriptions
			for (size_t i = 0; i < mBlendDescriptions.size(); i++)
			{
				D3D11_RENDER_TARGET_BLEND_DESC& rtBlendDesc = mBlendDescriptions[i];
				rtBlendDesc.BlendEnable = FALSE; // Default to no blending
				rtBlendDesc.SrcBlend = D3D11_BLEND_ONE;
				rtBlendDesc.DestBlend = D3D11_BLEND_ZERO;
				rtBlendDesc.BlendOp = D3D11_BLEND_OP_ADD;
				rtBlendDesc.SrcBlendAlpha = D3D11_BLEND_ONE;
				rtBlendDesc.DestBlendAlpha = D3D11_BLEND_ZERO;
				rtBlendDesc.BlendOpAlpha = D3D11_BLEND_OP_ADD;
				rtBlendDesc.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
			}
		}

		// Copy blend descriptions into the blend state
		for (size_t i = 0; i < mBlendDescriptions.size(); ++i)
			blendDesc.RenderTarget[i] = mBlendDescriptions[i];

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		result = device->CreateBlendState(&blendDesc, &mBlendState);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create blend state");
	}

	bool Framebuffer::IsIntegerFormat(TextureFormat format)
	{
		switch (format)
		{
		case TextureFormat::R32_SINT:
			return true;
		default:
			return false;
		}
	}

	void Framebuffer::Resize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0 || width > sMaxFramebufferSize || height > sMaxFramebufferSize)
		{
			TOAST_CORE_WARN("Attempted to resize framebuffer to %f, %f", width, height);
			return;
		}

		mWidth = width;
		mHeight = height;

		for (auto& colorTarget : mColorTargets)
			colorTarget->Resize(width, height);

		if (mDepthTarget)
			mDepthTarget->Resize(width, height);

		Invalidate();
	}

	int Framebuffer::ReadPixel(uint32_t x, uint32_t y)
	{
		HRESULT result;
		D3D11_TEXTURE2D_DESC textureDesc = {};
		D3D11_TEXTURE2D_DESC sourceDesc;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> stagedTexture;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> stagedSRV;

		D3D11_BOX srcBox = { x, y, 0, x + 1, y + 1, 1 };

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		mColorTargets[0]->GetTexture()->GetDesc(&sourceDesc);

		// Coordinates out of bounds
		if (x >= sourceDesc.Width || y >= sourceDesc.Height)
			return 0;

		textureDesc.Width = 1;
		textureDesc.Height = 1;
		textureDesc.MipLevels = 1;
		textureDesc.ArraySize = 1;
		textureDesc.Format = (DXGI_FORMAT)TextureFormat::R32_SINT;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Usage = D3D11_USAGE_STAGING;
		textureDesc.BindFlags = 0;
		textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		textureDesc.MiscFlags = 0;
		textureDesc.Format = sourceDesc.Format;

		result = device->CreateTexture2D(&textureDesc, nullptr, &stagedTexture);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create 2D texture!");
		
		deviceContext->CopySubresourceRegion(stagedTexture.Get(), 0, 0, 0, 0, mColorTargets[0]->GetTexture().Get(), 0, &srcBox);
		
		D3D11_MAPPED_SUBRESOURCE msr;
		HRESULT hr = deviceContext->Map(stagedTexture.Get(), 0, D3D11_MAP_READ, 0, &msr);
		if (FAILED(hr))
			return 0;
		int* pixelValue = reinterpret_cast<int*>(msr.pData);
		deviceContext->Unmap(stagedTexture.Get(), 0);

		return *pixelValue;
	}

}