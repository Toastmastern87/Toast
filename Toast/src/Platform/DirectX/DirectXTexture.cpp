#include "tpch.h"
#include "DirectXTexture.h"

#include "DirectXRendererAPI.h"
#include "Toast/Renderer/Renderer.h"

#include "Toast/Core/Application.h"

#include <WICTextureLoader.h>

#include <system_error>

namespace Toast {

	DirectXTexture2D::DirectXTexture2D(uint32_t width, uint32_t height, uint32_t slot)
		: mWidth(width), mHeight(height), mShaderSlot(slot)
	{
		TOAST_PROFILE_FUNCTION();

		HRESULT result;
		D3D11_TEXTURE2D_DESC textureDesc;
		ID3D11Texture2D* texture;

		size_t dataSize = mWidth * mHeight * sizeof(uint32_t);
		uint32_t initData = NULL;

		DirectXRendererAPI API = static_cast<DirectXRendererAPI&>(*RenderCommand::sRendererAPI);
		mDevice = API.GetDevice();
		mDeviceContext = API.GetDeviceContext();

		CreateSampler();

		textureDesc.ArraySize = 1;
		textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		textureDesc.Usage = D3D11_USAGE_DYNAMIC;
		textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureDesc.Height = mHeight;
		textureDesc.Width = mWidth;
		textureDesc.MipLevels = 1;
		textureDesc.MiscFlags = 0;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;

		result = mDevice->CreateTexture2D(&textureDesc, NULL, &texture);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create texture!");

		result = mDevice->CreateShaderResourceView(texture, 0, &mView);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to load shader resource view!");

		mView->GetResource(&mResource);
	}

	DirectXTexture2D::DirectXTexture2D(const std::string& path, uint32_t slot)
		: mPath(path), mShaderSlot(slot)
	{
		TOAST_PROFILE_FUNCTION();

		HRESULT result;

		DirectXRendererAPI API = static_cast<DirectXRendererAPI&>(*RenderCommand::sRendererAPI);
		mDevice = API.GetDevice();
		mDeviceContext = API.GetDeviceContext();

		CreateSampler();

		std::wstring stemp = std::wstring(mPath.begin(), mPath.end());

		result = DirectX::CreateWICTextureFromFile(mDevice, stemp.c_str(), &mResource, &mView);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to load texture!");

		ID3D11Texture2D* textureInterface;
		D3D11_TEXTURE2D_DESC desc;
		mResource->QueryInterface<ID3D11Texture2D>(&textureInterface);
		textureInterface->GetDesc(&desc);

		mWidth = desc.Width;
		mHeight = desc.Height;
		DXGI_FORMAT channels = desc.Format;

		CLEAN(textureInterface);
	}

	DirectXTexture2D::~DirectXTexture2D() 
	{
		TOAST_PROFILE_FUNCTION();

		CLEAN(mResource);
		CLEAN(mView);
		CLEAN(mSamplerState);
	}

	void DirectXTexture2D::CreateSampler() 
	{
		D3D11_SAMPLER_DESC samplerDesc;
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.MipLODBias = 0.0f;
		samplerDesc.MaxAnisotropy = 1;
		samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		samplerDesc.BorderColor[0] = 0;
		samplerDesc.BorderColor[1] = 0;
		samplerDesc.BorderColor[2] = 0;
		samplerDesc.BorderColor[3] = 0;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

		mDevice->CreateSamplerState(&samplerDesc, &mSamplerState);
	}

	void DirectXTexture2D::SetData(void* data, uint32_t size)
	{
		TOAST_PROFILE_FUNCTION();

		D3D11_MAPPED_SUBRESOURCE ms;

		TOAST_CORE_ASSERT(size == (mWidth * mHeight * size), "Data must be entire texture!");
		mDeviceContext->Map(mResource, NULL, D3D11_MAP_WRITE_DISCARD, NULL, &ms);
		memcpy(ms.pData, data, size);
		mDeviceContext->Unmap(mResource, NULL);
	}

	void DirectXTexture2D::Bind() const 
	{
		TOAST_PROFILE_FUNCTION();

		mDeviceContext->PSSetSamplers(0, 1, &mSamplerState);
		mDeviceContext->PSSetShaderResources(mShaderSlot, 1, &mView);
	}
}