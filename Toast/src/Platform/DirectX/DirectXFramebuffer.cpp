#include "tpch.h"
#include "DirectXFramebuffer.h"

#include "DirectXRendererAPI.h"
#include "Toast/Renderer/Renderer.h"

namespace Toast {

	DirectXFramebuffer::DirectXFramebuffer(const FramebufferSpecification& spec)
		: mSpecification(spec)
	{
		DirectXRendererAPI API = static_cast<DirectXRendererAPI&>(*RenderCommand::sRendererAPI);
		mDevice = API.GetDevice();
		mDeviceContext = API.GetDeviceContext();

		Invalidate();
	}

	DirectXFramebuffer::~DirectXFramebuffer()
	{
		CLEAN(mRenderTargetTexture);
		CLEAN(mRenderTargetView);
		CLEAN(mShaderResourceView);
	}

	void DirectXFramebuffer::Invalidate()
	{
		HRESULT result;
		D3D11_TEXTURE2D_DESC textureDesc;
		D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
		D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;

		textureDesc.Width = mSpecification.Width;
		textureDesc.Height = mSpecification.Height;
		textureDesc.MipLevels = 1;
		textureDesc.ArraySize = 1;
		textureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Usage = D3D11_USAGE_DEFAULT;
		textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		textureDesc.CPUAccessFlags = 0;
		textureDesc.MiscFlags = 0;

		result = mDevice->CreateTexture2D(&textureDesc, NULL, &mRenderTargetTexture);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create 2D texture!");

		renderTargetViewDesc.Format = textureDesc.Format;
		renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		renderTargetViewDesc.Texture2D.MipSlice = 0;

		result = mDevice->CreateRenderTargetView(mRenderTargetTexture, &renderTargetViewDesc, &mRenderTargetView);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create render target view!");

		shaderResourceViewDesc.Format = textureDesc.Format;
		shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
		shaderResourceViewDesc.Texture2D.MipLevels = 1;

		result = mDevice->CreateShaderResourceView(mRenderTargetTexture, &shaderResourceViewDesc, &mShaderResourceView);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create shader resource view!");
	}
}