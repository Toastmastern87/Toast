#include "tpch.h"
#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/Texture.h"

#include "Toast/Core/Application.h"

#include <WICTextureLoader.h>

#include <system_error>

namespace Toast {

	Texture2D::Texture2D(uint32_t width, uint32_t height, uint32_t slot, D3D11_SHADER_TYPE shaderType)
		: mWidth(width), mHeight(height), mShaderSlot(slot), mShaderType(shaderType)
	{
		TOAST_PROFILE_FUNCTION();

		HRESULT result;
		D3D11_TEXTURE2D_DESC textureDesc;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;

		size_t dataSize = mWidth * mHeight * sizeof(uint32_t);
		uint32_t initData = NULL;

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

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

		result = device->CreateTexture2D(&textureDesc, NULL, &texture);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create texture!");

		result = device->CreateShaderResourceView(texture.Get(), 0, &mView);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to load shader resource view!");

		mView->GetResource(&mResource);
	}

	Texture2D::Texture2D(const std::string& path, uint32_t slot, D3D11_SHADER_TYPE shaderType)
		: mPath(path), mShaderSlot(slot), mShaderType(shaderType)
	{
		TOAST_PROFILE_FUNCTION();

		HRESULT result;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> textureInterface;
		D3D11_TEXTURE2D_DESC desc;

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		CreateSampler();

		std::wstring stemp = std::wstring(mPath.begin(), mPath.end());

		result = DirectX::CreateWICTextureFromFile(device, stemp.c_str(), &mResource, &mView);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to load texture!");

		mResource->QueryInterface<ID3D11Texture2D>(&textureInterface);
		textureInterface->GetDesc(&desc);

		mWidth = desc.Width;
		mHeight = desc.Height;
		DXGI_FORMAT channels = desc.Format;
	}

	Texture2D::~Texture2D()
	{
		TOAST_PROFILE_FUNCTION();
	}

	void Texture2D::CreateSampler()
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		D3D11_SAMPLER_DESC samplerDesc;
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
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

		device->CreateSamplerState(&samplerDesc, &mSamplerState);
	}

	void Texture2D::SetData(void* data, uint32_t size)
	{
		TOAST_PROFILE_FUNCTION();

		D3D11_MAPPED_SUBRESOURCE ms;

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		TOAST_CORE_ASSERT(size == (mWidth * mHeight * size), "Data must be entire texture!");
		deviceContext->Map(mResource.Get(), NULL, D3D11_MAP_WRITE_DISCARD, NULL, &ms);
		memcpy(ms.pData, data, size);
		deviceContext->Unmap(mResource.Get(), NULL);
	}

	void Texture2D::Bind() const
	{
		TOAST_PROFILE_FUNCTION();

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		switch (mShaderType) 
		{
		case D3D11_VERTEX_SHADER: 
		{
			deviceContext->VSSetSamplers(0, 1, mSamplerState.GetAddressOf());
			deviceContext->VSSetShaderResources(mShaderSlot, 1, mView.GetAddressOf());

			break;
		}
		case D3D11_PIXEL_SHADER:
		{
			deviceContext->PSSetSamplers(0, 1, mSamplerState.GetAddressOf());
			deviceContext->PSSetShaderResources(mShaderSlot, 1, mView.GetAddressOf());

			break;
		}
		}
	}
}