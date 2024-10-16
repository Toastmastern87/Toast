#include "tpch.h"
#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/Texture.h"

#include "Toast/Core/Application.h"

#include <WICTextureLoader.h>

#include <system_error>

namespace Toast {

	////////////////////////////////////////////////////////////////////////////////////////  
	//     TEXTURE       ///////////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	uint32_t Texture::CalculateMipMapCount(uint32_t width, uint32_t height)
	{
		uint32_t levels = 1;
		while ((width | height) >> levels)
			levels++;

		return levels;
	}

	////////////////////////////////////////////////////////////////////////////////////////  
	//     TEXTURE2D     ///////////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	Texture2D::Texture2D(DXGI_FORMAT format, uint32_t width, uint32_t height, D3D11_USAGE usage, D3D11_BIND_FLAG bindFlag)
		: mWidth(width), mHeight(height)
	{
		TOAST_PROFILE_FUNCTION();

		HRESULT result;
		D3D11_TEXTURE2D_DESC textureDesc = {};

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		textureDesc.ArraySize = 1;
		textureDesc.BindFlags = bindFlag;
		textureDesc.Usage = usage;
		textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		textureDesc.Format = format;
		textureDesc.Height = mHeight;
		textureDesc.Width = mWidth;
		textureDesc.MipLevels = 1;
		textureDesc.MiscFlags = 0;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;

		result = device->CreateTexture2D(&textureDesc, nullptr, &mTexture);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create texture!");

		CreateSRV();

		mSRV->GetResource(&mResource);
	}

	Texture2D::Texture2D(const std::string& filePath, bool forceSRGB)
		: mFilePath(filePath)
	{
		TOAST_PROFILE_FUNCTION();

		HRESULT result;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> textureInterface;
		D3D11_TEXTURE2D_DESC desc = {};

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		std::wstring stemp = std::wstring(mFilePath.begin(), mFilePath.end());

		if (forceSRGB)
			result = DirectX::CreateWICTextureFromFileEx(device, deviceContext, stemp.c_str(), 0, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0,
				DirectX::WIC_LOADER_FLAGS::WIC_LOADER_FORCE_SRGB, &mResource, &mSRV);

		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to load texture!");

		mResource->QueryInterface<ID3D11Texture2D>(&textureInterface);
		textureInterface->GetDesc(&desc);

		//TOAST_CORE_INFO("Creating texture: %s, format: %d", mFilePath.c_str(), desc.Format);

		mWidth = desc.Width;
		mHeight = desc.Height;
	}

	void Texture2D::SetData(void* data, uint32_t size)
	{
		TOAST_PROFILE_FUNCTION();

		D3D11_MAPPED_SUBRESOURCE ms;

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		//TOAST_CORE_ASSERT(size == (mWidth * mHeight * size), "Data must be entire texture!");
		deviceContext->Map(mResource.Get(), NULL, D3D11_MAP_WRITE_DISCARD, NULL, &ms);
		memcpy(ms.pData, data, size);
		deviceContext->Unmap(mResource.Get(), NULL);
	}

	void Texture2D::BindForReadWrite(uint32_t bindslot, D3D11_SHADER_TYPE shaderType) const
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		switch (shaderType)
		{
		case D3D11_COMPUTE_SHADER:
			deviceContext->CSSetUnorderedAccessViews(bindslot, 1, mUAV.GetAddressOf(), nullptr);
		}
	}

	void Texture2D::UnbindUAV(uint32_t bindslot, D3D11_SHADER_TYPE shaderType) const
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		switch (shaderType)
		{
		case D3D11_COMPUTE_SHADER:
			deviceContext->CSSetUnorderedAccessViews(bindslot, 1, mNullUAV.GetAddressOf(), nullptr);
		}
	}

	void Texture2D::CreateUAV(uint32_t mipSlice)
	{
		D3D11_TEXTURE2D_DESC desc = {};
		mTexture->GetDesc(&desc);

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = desc.Format;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = mipSlice;

		HRESULT result = device->CreateUnorderedAccessView(mTexture.Get(), &uavDesc, &mUAV);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create the UAV!");
	}

	void Texture2D::GenerateMips() const
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		deviceContext->GenerateMips(mSRV.Get());
	}

	void Texture2D::CreateSRV()
	{
		D3D11_TEXTURE2D_DESC desc = {};
		mTexture->GetDesc(&desc);

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc = {};
		shaderResourceViewDesc.Format = desc.Format;
		shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		shaderResourceViewDesc.Texture2D.MipLevels = -1;
		shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;

		HRESULT result = device->CreateShaderResourceView(mTexture.Get(), &shaderResourceViewDesc, &mSRV);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create the SRV!");
	}

	void Texture2D::Bind(uint32_t bindslot, D3D11_SHADER_TYPE shaderType) const
	{
		TOAST_PROFILE_FUNCTION();

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		switch (shaderType)
		{
		case D3D11_VERTEX_SHADER:
			deviceContext->VSSetShaderResources(bindslot, 1, mSRV.GetAddressOf());
		case D3D11_PIXEL_SHADER:
			deviceContext->PSSetShaderResources(bindslot, 1, mSRV.GetAddressOf());
		case D3D11_COMPUTE_SHADER:
			deviceContext->CSSetShaderResources(bindslot, 1, mSRV.GetAddressOf());
		}
	}

	const uint32_t Texture2D::GetMipLevelCount() const
	{
		return Texture::CalculateMipMapCount(mWidth, mHeight);
	}

	////////////////////////////////////////////////////////////////////////////////////////  
	//     TEXTURECUBE   ///////////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	TextureCube::TextureCube(const std::string& filePath, uint32_t width, uint32_t height, uint32_t levels)
		: mFilePath(filePath), mWidth(width), mHeight(height)
	{
		TOAST_PROFILE_FUNCTION();
		D3D11_TEXTURE2D_DESC textureDesc = {};

		mLevels = (levels > 0) ? levels : CalculateMipMapCount(width, height);

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

		HRESULT result = device->CreateTexture2D(&textureDesc, nullptr, &mTexture);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create texture!");

		CreateSRV();

		mSRV->GetResource(&mResource);
	}

	const uint32_t TextureCube::GetMipLevelCount() const
	{
		return Texture::CalculateMipMapCount(mWidth, mHeight);
	}

	void TextureCube::SetData(void* data, uint32_t size)
	{

	}

	void TextureCube::Bind(uint32_t bindslot, D3D11_SHADER_TYPE shaderType) const
	{
		TOAST_PROFILE_FUNCTION();

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		switch (shaderType)
		{
		case D3D11_VERTEX_SHADER:
			deviceContext->VSSetShaderResources(bindslot, 1, mSRV.GetAddressOf());
		case D3D11_PIXEL_SHADER:
			deviceContext->PSSetShaderResources(bindslot, 1, mSRV.GetAddressOf());
		case D3D11_COMPUTE_SHADER:
			deviceContext->CSSetShaderResources(bindslot, 1, mSRV.GetAddressOf());
		}
	}

	void TextureCube::BindForReadWrite(uint32_t bindslot, D3D11_SHADER_TYPE shaderType) const
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		switch (shaderType)
		{
		case D3D11_COMPUTE_SHADER:
			deviceContext->CSSetUnorderedAccessViews(bindslot, 1, mUAV.GetAddressOf(), nullptr);
		}
	}

	void TextureCube::UnbindUAV(uint32_t bindslot, D3D11_SHADER_TYPE shaderType) const
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		switch (shaderType)
		{
		case D3D11_COMPUTE_SHADER:
			deviceContext->CSSetUnorderedAccessViews(bindslot, 1, mNullUAV.GetAddressOf(), nullptr);
		}
	}

	void TextureCube::CreateSRV()
	{
		D3D11_TEXTURE2D_DESC desc = {};
		mTexture->GetDesc(&desc);

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc = {};
		shaderResourceViewDesc.Format = desc.Format;
		shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
		shaderResourceViewDesc.Texture2DArray.MipLevels = -1;
		shaderResourceViewDesc.Texture2DArray.MostDetailedMip = 0;

		HRESULT result = device->CreateShaderResourceView(mTexture.Get(), &shaderResourceViewDesc, &mSRV);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create the SRV!");
	}

	void TextureCube::CreateUAV(uint32_t mipSlice)
	{
		D3D11_TEXTURE2D_DESC desc = {};
		mTexture->GetDesc(&desc);

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = desc.Format;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
		uavDesc.Texture2DArray.MipSlice = mipSlice;
		uavDesc.Texture2DArray.FirstArraySlice = 0;
		uavDesc.Texture2DArray.ArraySize = 6;

		HRESULT result = device->CreateUnorderedAccessView(mTexture.Get(), &uavDesc, &mUAV);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create the UAV!");
	}

	void TextureCube::GenerateMips() const
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		deviceContext->GenerateMips(mSRV.Get());
	}

	////////////////////////////////////////////////////////////////////////////////////////  
	//     TEXTURE SAMPLER   ///////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	TextureSampler::TextureSampler(D3D11_FILTER filter, D3D11_TEXTURE_ADDRESS_MODE addressMode)
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		D3D11_SAMPLER_DESC desc = {};
		desc.Filter = filter;
		desc.AddressU = addressMode;
		desc.AddressV = addressMode;
		desc.AddressW = addressMode;
		desc.MaxAnisotropy = (filter == D3D11_FILTER_ANISOTROPIC) ? D3D11_REQ_MAXANISOTROPY : 1;
		desc.MipLODBias = 0.0f;
		desc.MinLOD = 0;
		desc.MaxLOD = D3D11_FLOAT32_MAX;

		HRESULT result = device->CreateSamplerState(&desc, &mSamplerState);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create the sampler!");
	}

	void TextureSampler::Bind(uint32_t bindslot, D3D11_SHADER_TYPE shaderType) const
	{
		TOAST_PROFILE_FUNCTION();

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		switch (shaderType)
		{
		case D3D11_VERTEX_SHADER:
			deviceContext->VSSetSamplers(bindslot, 1, mSamplerState.GetAddressOf());
		case D3D11_PIXEL_SHADER:
			deviceContext->PSSetSamplers(bindslot, 1, mSamplerState.GetAddressOf());
		case D3D11_COMPUTE_SHADER:
			deviceContext->CSSetSamplers(bindslot, 1, mSamplerState.GetAddressOf());
		}
	}

	////////////////////////////////////////////////////////////////////////////////////////  
	//     TEXTURE LIBRARY    //////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	std::unordered_map<std::string, Scope<Texture>> TextureLibrary::mTextures;
	std::unordered_map<std::string, Scope<TextureSampler>> TextureLibrary::mTextureSamplers;

	Texture2D* TextureLibrary::LoadTexture2D(const std::string& filePath)
	{
		if (Exists(filePath)) 
			return (Texture2D*)mTextures[filePath].get();

		mTextures[filePath] = CreateScope<Texture2D>(filePath);

		return (Texture2D*)mTextures[filePath].get();
	}

	TextureCube* TextureLibrary::LoadTextureCube(const std::string& filePath, uint32_t width, uint32_t height, uint32_t levels)
	{
		if (Exists(filePath))
			return (TextureCube*)mTextures[filePath].get();

		mTextures[filePath] = CreateScope<TextureCube>(filePath, width, height, levels);
		return (TextureCube*)mTextures[filePath].get();
	}

	TextureSampler* TextureLibrary::LoadTextureSampler(const std::string& name, D3D11_FILTER filter, D3D11_TEXTURE_ADDRESS_MODE addressMode)
	{
		mTextureSamplers[name] = CreateScope<TextureSampler>(filter, addressMode);
		return mTextureSamplers[name].get();
	}

	Texture* TextureLibrary::Get(const std::string& filePath)
	{
		TOAST_CORE_ASSERT(Exists(filePath), "Texture not found!");
		return mTextures[filePath].get();
	}

	TextureSampler* TextureLibrary::GetSampler(const std::string& name)
	{
		TOAST_CORE_ASSERT(ExistsSampler(name), "Texture sampler not found!");
		return mTextureSamplers[name].get();
	}

	bool TextureLibrary::Exists(const std::string& filePath)
	{
		return mTextures.find(filePath) != mTextures.end();
	}

	bool TextureLibrary::ExistsSampler(const std::string& name)
	{
		return mTextureSamplers.find(name) != mTextureSamplers.end();
	}
}