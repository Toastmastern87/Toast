#include "tpch.h"
#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/Texture.h"

#include "Toast/Core/Application.h"

#include <WICTextureLoader.h>

#include <system_error>

namespace Toast {

	////////////////////////////////////////////////////////////////////////////////////////  
	//     TEXTURE2D     ///////////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	Texture2D::Texture2D(uint32_t width, uint32_t height, uint32_t slot, D3D11_SHADER_TYPE shaderType, DXGI_FORMAT format)
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

		textureDesc.ArraySize = 1;
		textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		textureDesc.Usage = D3D11_USAGE_DYNAMIC;
		textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		textureDesc.Format = format;
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

	Texture2D::Texture2D(const std::string& path, uint32_t slot, D3D11_SHADER_TYPE shaderType, DXGI_FORMAT format)
		: mPath(path), mShaderSlot(slot), mShaderType(shaderType)
	{
		TOAST_PROFILE_FUNCTION();

		HRESULT result;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> textureInterface;
		D3D11_TEXTURE2D_DESC desc;

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

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

	void Texture2D::CreateSampler(D3D11_FILTER filter, D3D11_TEXTURE_ADDRESS_MODE addressMode)
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		D3D11_SAMPLER_DESC desc;
		desc.Filter = filter;
		desc.AddressU = addressMode;
		desc.AddressV = addressMode;
		desc.AddressW = addressMode;
		desc.MipLODBias = 0.0f;
		desc.MaxAnisotropy = 1;
		desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		desc.BorderColor[0] = 0;
		desc.BorderColor[1] = 0;
		desc.BorderColor[2] = 0;
		desc.BorderColor[3] = 0;
		desc.MinLOD = 0;
		desc.MaxLOD = D3D11_FLOAT32_MAX;

		device->CreateSamplerState(&desc, &mSamplerState);
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
		case D3D11_COMPUTE_SHADER:
		{
			deviceContext->CSSetSamplers(0, 1, mSamplerState.GetAddressOf());
			deviceContext->CSSetShaderResources(mShaderSlot, 1, mView.GetAddressOf());

			break;
		}
		}
	}

	////////////////////////////////////////////////////////////////////////////////////////  
	//     TEXTURECUBE   ///////////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	TextureCube::TextureCube(uint32_t width, uint32_t height, uint32_t slot, D3D11_SHADER_TYPE shaderType, uint32_t levels)
		: mWidth(width), mHeight(height), mShaderSlot(slot), mShaderType(shaderType)
	{
		TOAST_PROFILE_FUNCTION();

		HRESULT result;
		D3D11_TEXTURE2D_DESC textureDesc;

		mLevels = (levels > 0) ? levels : CalculateMipMapCount(width);

		size_t dataSize = mWidth * mHeight * sizeof(uint32_t);
		uint32_t initData = NULL;

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();
		textureDesc.Width = mWidth;
		textureDesc.Height = mHeight;
		textureDesc.MipLevels = levels;
		textureDesc.ArraySize = 6;
		textureDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Usage = D3D11_USAGE_DEFAULT;
		textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		textureDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
		if (levels == 0) {
			textureDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;
			textureDesc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
		}

		result = device->CreateTexture2D(&textureDesc, NULL, &mTexture);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create texture!");

		CreateSRV();

		mSRV->GetResource(&mResource);
	}

	TextureCube::~TextureCube()
	{

	}

	uint32_t TextureCube::CalculateMipMapCount(uint32_t cubemapSize)
	{
		uint32_t levels = 1;
		auto mipSize = cubemapSize >> 1;
		while (mipSize >= 1)
		{
			mipSize >>= 1;
			++levels;
		}

		return levels;
	}

	void TextureCube::SetData(void* data, uint32_t size)
	{

	}

	void TextureCube::BindForSampling() const
	{
		TOAST_PROFILE_FUNCTION();

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		switch (mShaderType)
		{
		case D3D11_VERTEX_SHADER:
		{
			deviceContext->VSSetSamplers(0, 1, mSamplerState.GetAddressOf());
			deviceContext->VSSetShaderResources(mShaderSlot, 1, mSRV.GetAddressOf());

			break;
		}
		case D3D11_PIXEL_SHADER:
		{
			deviceContext->PSSetSamplers(0, 1, mSamplerState.GetAddressOf());
			deviceContext->PSSetShaderResources(mShaderSlot, 1, mSRV.GetAddressOf());

			break;
		}
		case D3D11_COMPUTE_SHADER:
		{
			deviceContext->CSSetSamplers(0, 1, mSamplerState.GetAddressOf());
			deviceContext->CSSetShaderResources(mShaderSlot, 1, mSRV.GetAddressOf());

			break;
		}
		}
	}

	void TextureCube::BindForReadWrite() const
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		switch (mShaderType)
		{
		case D3D11_COMPUTE_SHADER:
		{
			deviceContext->CSSetUnorderedAccessViews(0, 1, mUAV.GetAddressOf(), nullptr);

			break;
		}
		}
	}

	void TextureCube::UnbindUAV() const
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		switch (mShaderType)
		{
		case D3D11_COMPUTE_SHADER:
			deviceContext->CSSetUnorderedAccessViews(0, 1, mNullUAV.GetAddressOf(), nullptr);
		}
	}

	void TextureCube::CreateSampler(D3D11_FILTER filter, D3D11_TEXTURE_ADDRESS_MODE addressMode)
	{
		HRESULT result;
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		D3D11_SAMPLER_DESC desc;
		desc.Filter = filter;
		desc.AddressU = addressMode;
		desc.AddressV = addressMode;
		desc.AddressW = addressMode;
		desc.MaxAnisotropy = (filter == D3D11_FILTER_ANISOTROPIC) ? D3D11_REQ_MAXANISOTROPY : 1;
		desc.MipLODBias = 0.0f;
		desc.MinLOD = 0;
		desc.MaxLOD = D3D11_FLOAT32_MAX;

		result = device->CreateSamplerState(&desc, &mSamplerState);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create the sampler!");
	}

	void TextureCube::CreateSRV()
	{
		HRESULT result;

		D3D11_TEXTURE2D_DESC desc;
		mTexture->GetDesc(&desc);

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
		shaderResourceViewDesc.Format = desc.Format;
		shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
		shaderResourceViewDesc.Texture2DArray.MipLevels = -1;
		shaderResourceViewDesc.Texture2DArray.MostDetailedMip = 0;

		result = device->CreateShaderResourceView(mTexture.Get(), &shaderResourceViewDesc, &mSRV);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create the SRV!");
	}

	void TextureCube::CreateUAV(uint32_t mipSlice)
	{
		HRESULT result;

		D3D11_TEXTURE2D_DESC desc;
		mTexture->GetDesc(&desc);

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
		uavDesc.Format = desc.Format;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
		uavDesc.Texture2DArray.MipSlice = mipSlice;
		uavDesc.Texture2DArray.FirstArraySlice = 0;
		uavDesc.Texture2DArray.ArraySize = 6;

		result = device->CreateUnorderedAccessView(mTexture.Get(), &uavDesc, &mUAV);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create the UAV!");
	}

	void TextureCube::GenerateMips()
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		deviceContext->GenerateMips(mSRV.Get());
	}

}