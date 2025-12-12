#include "tpch.h"
#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/Texture.h"

#include "Toast/Core/Application.h"

#include <WICTextureLoader.h>

#include <wincodec.h>
#include <wrl/client.h>

#include <system_error>

namespace Toast {

HRESULT MyWICGetPixelFormatBitsPerPixel(const WICPixelFormatGUID* pGuid, UINT* pBPP)
{
    if (!pGuid || !pBPP)
        return E_INVALIDARG;
        
    if (memcmp(pGuid, &GUID_WICPixelFormat32bppRGBA, sizeof(WICPixelFormatGUID)) == 0)
    {
        *pBPP = 32;
        return S_OK;
    }
    else if (memcmp(pGuid, &GUID_WICPixelFormat64bppRGBA, sizeof(WICPixelFormatGUID)) == 0)
    {
        *pBPP = 64;
        return S_OK;
    }
    else if (memcmp(pGuid, &GUID_WICPixelFormat24bppBGR, sizeof(WICPixelFormatGUID)) == 0)
    {
        *pBPP = 24;
        return S_OK;
    }
    else if (memcmp(pGuid, &GUID_WICPixelFormat24bppRGB, sizeof(WICPixelFormatGUID)) == 0)
    {
        *pBPP = 24;
        return S_OK;
    }
    else
    {
        // Fallback: assume 32 bits per pixel if unknown.
        *pBPP = 32;
        return S_OK;
    }
}

	HRESULT LoadImageDataFromFile(const std::wstring& filename,	std::vector<uint8_t>& imageData, UINT& width, UINT& height,	DXGI_FORMAT& format, UINT& rowPitch)
	{
		using namespace Microsoft::WRL;

		// Create the WIC factory.
		ComPtr<IWICImagingFactory> factory;
		HRESULT hr = CoCreateInstance(
			CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&factory));
		if (FAILED(hr))
			return hr;

		// Create a decoder for the image.
		ComPtr<IWICBitmapDecoder> decoder;
		hr = factory->CreateDecoderFromFilename(filename.c_str(), nullptr,
			GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
		if (FAILED(hr))
			return hr;

		// Retrieve the first frame of the image.
		ComPtr<IWICBitmapFrameDecode> frame;
		hr = decoder->GetFrame(0, &frame);
		if (FAILED(hr))
			return hr;

		// Get the image dimensions.
		hr = frame->GetSize(&width, &height);
		if (FAILED(hr))
			return hr;

		// Retrieve the pixel format of the image.
		WICPixelFormatGUID pixelFormat;
		hr = frame->GetPixelFormat(&pixelFormat);
		if (FAILED(hr))
			return hr;

		// Query the bit depth.
		UINT bitsPerPixel = 0;
		hr = MyWICGetPixelFormatBitsPerPixel(&pixelFormat, &bitsPerPixel);
		if (FAILED(hr))
			return hr;

		ComPtr<IWICComponentInfo> ci;
		factory->CreateComponentInfo(pixelFormat, &ci);

		ComPtr<IWICPixelFormatInfo> pfi;
		ci.As(&pfi);

		UINT bpp = 0, channelCount = 0;
		pfi->GetBitsPerPixel(&bpp);  
		pfi->GetChannelCount(&channelCount);

		// Decide on the desired format based on bit depth.
		GUID desiredGUID;
		if (bitsPerPixel == 32)
		{
			if (pixelFormat == GUID_WICPixelFormat32bppGrayFloat)
			{
				desiredGUID = GUID_WICPixelFormat32bppGrayFloat;
				format = DXGI_FORMAT_R32_FLOAT; 
				rowPitch = width * 4;
			}
			else if (channelCount == 1)
			{
				desiredGUID = GUID_WICPixelFormat16bppGray;
				format = DXGI_FORMAT_R16_UNORM;   
				rowPitch = width * 2;        
			}
			else 
			{
				desiredGUID = GUID_WICPixelFormat32bppRGBA;
				format = DXGI_FORMAT_R8G8B8A8_UNORM;
				rowPitch = width * 4; // 4 bytes per pixel.
			}
		}
		else if (bitsPerPixel == 64)
		{
			desiredGUID = GUID_WICPixelFormat64bppRGBA;
			format = DXGI_FORMAT_R16G16B16A16_UNORM;
			rowPitch = width * 8; // 8 bytes per pixel (16 bits per channel).
		}
		else
		{
			// Default to 32bpp if unexpected bit depth.
			desiredGUID = GUID_WICPixelFormat32bppRGBA;
			format = DXGI_FORMAT_R8G8B8A8_UNORM;
			rowPitch = width * 4;
		}

		// Convert the image to the desired format if necessary.
		if (memcmp(&pixelFormat, &desiredGUID, sizeof(WICPixelFormatGUID)) != 0)
		{
			ComPtr<IWICFormatConverter> converter;
			hr = factory->CreateFormatConverter(&converter);
			if (FAILED(hr))
				return hr;

			hr = converter->Initialize(frame.Get(), desiredGUID, WICBitmapDitherTypeNone, nullptr, 0.f, WICBitmapPaletteTypeCustom);
			if (FAILED(hr))
				return hr;

			imageData.resize(rowPitch * height);
			hr = converter->CopyPixels(nullptr, rowPitch, static_cast<UINT>(imageData.size()), imageData.data());
		}
		else
		{
			// If already in the desired format, just copy the pixels.
			imageData.resize(rowPitch * height);
			hr = frame->CopyPixels(nullptr, rowPitch, static_cast<UINT>(imageData.size()), imageData.data());
		}

		return hr;
	}

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
	//     TEXTURE1D     ///////////////////////////////////////////////////////////////////  
	////////////////////////////////////////////////////////////////////////////////////////

	Texture1D::Texture1D(DXGI_FORMAT format, DXGI_FORMAT srvFormat, uint32_t width,	uint32_t mipLevels,	uint32_t arraySize,	D3D11_USAGE usage, UINT bindFlags, UINT cpuAccessFlags)
		: mFormat(format), mSRVFormat(srvFormat), mWidth(width), mMipLevels(std::max(1u, mipLevels)), mArraySize(std::max(1u, arraySize)), mUsage(usage), mBindFlags(bindFlags), mCPUAccessFlags(cpuAccessFlags)
	{
		CreateTexture1D(nullptr, 0);
		CreateSRV();

		if (mBindFlags & D3D11_BIND_UNORDERED_ACCESS)
			CreateUAV(0, 0, mArraySize);

		mResource = mTexture1D;
	}

	Texture1D::Texture1D(DXGI_FORMAT format, DXGI_FORMAT srvFormat, uint32_t width, const void* initialData, size_t initialDataSizeBytes, uint32_t mipLevels, uint32_t arraySize, D3D11_USAGE usage, UINT bindFlags, UINT cpuAccessFlags)
		: mFormat(format), mSRVFormat(srvFormat), mWidth(width), mMipLevels(std::max(1u, mipLevels)), mArraySize(std::max(1u, arraySize)), mUsage(usage), mBindFlags(bindFlags), mCPUAccessFlags(cpuAccessFlags)
	{
		CreateTexture1D(initialData, initialDataSizeBytes);
		CreateSRV();

		if (mBindFlags & D3D11_BIND_UNORDERED_ACCESS)
			CreateUAV(0, 0, mArraySize);

		mResource = mTexture1D;
	}

	void Texture1D::CreateTexture1D(const void* initData, size_t initSizeBytes)
	{
		auto* device = RenderCommand::sRendererAPI->GetDevice();

		D3D11_TEXTURE1D_DESC td = {};
		td.Width = mWidth;
		td.MipLevels = mMipLevels;
		td.ArraySize = mArraySize;
		td.Format = mFormat;
		td.Usage = mUsage;
		td.BindFlags = mBindFlags;
		td.CPUAccessFlags = mCPUAccessFlags;
		td.MiscFlags = 0;
		if (mMipLevels == 0)
			td.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;

		const bool hasInit = (initData != nullptr && initSizeBytes > 0);

		D3D11_SUBRESOURCE_DATA sd = {};
		const D3D11_SUBRESOURCE_DATA* pSD = nullptr;

		if (hasInit && mMipLevels == 1 && mArraySize == 1)
		{
			sd.pSysMem = initData;
			sd.SysMemPitch = 0;
			sd.SysMemSlicePitch = 0;
			pSD = &sd;
		}

		HRESULT hr = device->CreateTexture1D(&td, pSD, &mTexture1D);
		TOAST_CORE_ASSERT(SUCCEEDED(hr), "Unable to create Texture1D!");
	}

	void Texture1D::CreateSRV()
	{
		auto* device = RenderCommand::sRendererAPI->GetDevice();

		D3D11_SHADER_RESOURCE_VIEW_DESC sd = {};
		sd.Format = (mSRVFormat == DXGI_FORMAT_UNKNOWN) ? mFormat : mSRVFormat;

		if (mArraySize > 1)
		{
			sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1DARRAY;
			sd.Texture1DArray.MostDetailedMip = 0;
			sd.Texture1DArray.MipLevels = (mMipLevels == 0 ? -1 : mMipLevels);
			sd.Texture1DArray.FirstArraySlice = 0;
			sd.Texture1DArray.ArraySize = mArraySize;
		}
		else
		{
			sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
			sd.Texture1D.MostDetailedMip = 0;
			sd.Texture1D.MipLevels = (mMipLevels == 0 ? -1 : mMipLevels);
		}

		HRESULT hr = device->CreateShaderResourceView(mTexture1D.Get(), &sd, &mSRV);
		TOAST_CORE_ASSERT(SUCCEEDED(hr), "Unable to create SRV for Texture1D!");
	}

	void Texture1D::CreateUAV(uint32_t mipSlice, uint32_t firstArraySlice, uint32_t arraySize)
	{
		auto* device = RenderCommand::sRendererAPI->GetDevice();

		D3D11_UNORDERED_ACCESS_VIEW_DESC ud = {};
		ud.Format = mFormat;

		if (mArraySize > 1)
		{
			ud.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1DARRAY;
			ud.Texture1DArray.MipSlice = mipSlice;
			ud.Texture1DArray.FirstArraySlice = firstArraySlice;
			ud.Texture1DArray.ArraySize = arraySize;
		}
		else
		{
			ud.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1D;
			ud.Texture1D.MipSlice = mipSlice;
		}

		HRESULT hr = device->CreateUnorderedAccessView(mTexture1D.Get(), &ud, &mUAV);
		TOAST_CORE_ASSERT(SUCCEEDED(hr), "Unable to create UAV for Texture1D!");
	}

	void Texture1D::Bind(uint32_t bindslot, D3D11_SHADER_TYPE shaderType) const
	{
		auto* ctx = RenderCommand::sRendererAPI->GetDeviceContext();

		switch (shaderType)
		{
		case D3D11_VERTEX_SHADER:
			ctx->VSSetShaderResources(bindslot, 1, mSRV.GetAddressOf());
			// no break; (matches your existing pattern)
		case D3D11_PIXEL_SHADER:
			ctx->PSSetShaderResources(bindslot, 1, mSRV.GetAddressOf());
			// no break;
		case D3D11_COMPUTE_SHADER:
			ctx->CSSetShaderResources(bindslot, 1, mSRV.GetAddressOf());
			break;
		default: break;
		}
	}

	void Texture1D::BindForReadWrite(uint32_t bindslot, D3D11_SHADER_TYPE shaderType) const
	{
		auto* ctx = RenderCommand::sRendererAPI->GetDeviceContext();
		switch (shaderType)
		{
		case D3D11_COMPUTE_SHADER:
			ctx->CSSetUnorderedAccessViews(bindslot, 1, mUAV.GetAddressOf(), nullptr);
			break;
		default: break;
		}
	}

	void Texture1D::UnbindUAV(uint32_t bindslot, D3D11_SHADER_TYPE shaderType) const
	{
		auto* ctx = RenderCommand::sRendererAPI->GetDeviceContext();
		ID3D11UnorderedAccessView* nullUAV = nullptr;

		switch (shaderType)
		{
		case D3D11_COMPUTE_SHADER:
			ctx->CSSetUnorderedAccessViews(bindslot, 1, &nullUAV, nullptr);
			break;
		default: break;
		}
	}

	void Texture1D::SetData(const void* data, size_t sizeBytes, uint32_t mipLevel, uint32_t arraySlice)
	{
		// For DEFAULT usage, UpdateSubresource is simplest:
		auto* ctx = RenderCommand::sRendererAPI->GetDeviceContext();

		const UINT subresource = D3D11CalcSubresource(
			mipLevel,
			arraySlice,
			mMipLevels
		);

		D3D11_BOX box = {};
		// For 1D textures, only X dimension matters; Y,Z are ignored.
		box.left = 0;
		box.right = mWidth >> mipLevel ? (mWidth >> mipLevel) : 1;
		box.top = 0;
		box.bottom = 1;
		box.front = 0;
		box.back = 1;

		ctx->UpdateSubresource(mTexture1D.Get(), subresource, &box, data, 0, 0);
	}

	void Texture1D::GenerateMips() const
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		deviceContext->GenerateMips(mSRV.Get());
	}

	////////////////////////////////////////////////////////////////////////////////////////  
	//     TEXTURE2D     ///////////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	Texture2D::Texture2D(DXGI_FORMAT format, DXGI_FORMAT srvFormat, uint32_t width, uint32_t height, D3D11_USAGE usage, D3D11_BIND_FLAG bindFlag, uint32_t samples, UINT cpuAccessFlags, UINT mipLevels)
		: mWidth(width), mHeight(height), mFormat(format), mSRVFormat(srvFormat)
	{
		TOAST_PROFILE_FUNCTION();

		HRESULT result;
		D3D11_TEXTURE2D_DESC textureDesc = {};

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		textureDesc.ArraySize = 1;
		textureDesc.BindFlags = bindFlag;
		textureDesc.Usage = usage;
		textureDesc.CPUAccessFlags = cpuAccessFlags;
		textureDesc.Format = format;
		textureDesc.Height = mHeight;
		textureDesc.Width = mWidth;
		textureDesc.MipLevels = mipLevels;
		textureDesc.MiscFlags = 0;
		textureDesc.SampleDesc.Count = samples;
		textureDesc.SampleDesc.Quality = 0;
		if (mipLevels == 0) 
			textureDesc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;

		result = device->CreateTexture2D(&textureDesc, nullptr, &mTexture);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create texture!");

		if (bindFlag & D3D11_BIND_SHADER_RESOURCE)
			CreateSRV();

		if (bindFlag & D3D11_BIND_UNORDERED_ACCESS)
			CreateUAV(0);

		if (mSRV)
			mSRV->GetResource(&mResource);
		else      
			mResource = mTexture;

		if (mipLevels == 0)
			GenerateMips();
	}

	Texture2D::Texture2D(DXGI_FORMAT format, DXGI_FORMAT srvFormat, uint32_t width, uint32_t height, D3D11_USAGE usage, D3D11_BIND_FLAG bindFlag, uint32_t samples, UINT cpuAccessFlags, void* initialData, UINT rowPitch)
		: mWidth(width), mHeight(height), mFormat(format), mSRVFormat(srvFormat)
	{
		TOAST_PROFILE_FUNCTION();

		D3D11_TEXTURE2D_DESC textureDesc = {};
		textureDesc.ArraySize = 1;
		textureDesc.BindFlags = bindFlag;
		textureDesc.Usage = usage;
		textureDesc.CPUAccessFlags = cpuAccessFlags;
		textureDesc.Format = format;
		textureDesc.Height = mHeight;
		textureDesc.Width = mWidth;
		textureDesc.MipLevels = 1;
		textureDesc.MiscFlags = 0;
		textureDesc.SampleDesc.Count = samples;
		textureDesc.SampleDesc.Quality = 0;

		D3D11_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pSysMem = initialData;
		subresourceData.SysMemPitch = rowPitch;

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		HRESULT result = device->CreateTexture2D(&textureDesc, initialData ? &subresourceData : nullptr, &mTexture);
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

		std::wstring wFilePath = std::wstring(mFilePath.begin(), mFilePath.end());

		result = LoadImageDataFromFile(wFilePath, mImageData, mWidth, mHeight, mFormat, mRowPitch);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to load texture!");

		mSRVFormat = forceSRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : mFormat;

		// Create the texture using the loaded data.
		D3D11_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pSysMem = mImageData.data();
		subresourceData.SysMemPitch = mRowPitch;

		D3D11_TEXTURE2D_DESC textureDesc = {};
		textureDesc.ArraySize = 1;
		textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		textureDesc.Usage = D3D11_USAGE_DEFAULT;
		textureDesc.CPUAccessFlags = 0;
		textureDesc.Format = forceSRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : mFormat;
		textureDesc.Height = mHeight;
		textureDesc.Width = mWidth;
		textureDesc.MipLevels = 0;
		textureDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;

		result = device->CreateTexture2D(&textureDesc, nullptr, &mTexture);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create texture!");

		deviceContext->UpdateSubresource(mTexture.Get(), 0, nullptr, mImageData.data(), mRowPitch, 0);

		CreateSRV();
		mSRV->GetResource(&mResource);

		mResource->QueryInterface<ID3D11Texture2D>(&textureInterface);
		textureInterface->GetDesc(&desc);

		GenerateMips();

		//TOAST_CORE_INFO("Creating texture: %s, format: %d", mFilePath.c_str(), desc.Format);

		mWidth = desc.Width;
		mHeight = desc.Height;
		mFormat = desc.Format;
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

		mImageData.resize(size);
		memcpy(mImageData.data(), data, size);

		mRowPitch = mWidth * 4 * sizeof(float);
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
		shaderResourceViewDesc.Format = (mSRVFormat != DXGI_FORMAT_UNKNOWN) ? mSRVFormat : desc.Format;
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
	//     TEXTURE3D     ///////////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	Texture3D::Texture3D(DXGI_FORMAT format, DXGI_FORMAT srvFormat, uint32_t width, uint32_t height, uint32_t depth, D3D11_USAGE usage, D3D11_BIND_FLAG bindFlags, UINT cpuAccessFlags)
		: mWidth(width), mHeight(height), mDepth(depth), mFormat(format), mSRVFormat(srvFormat)
	{
		auto* device = RenderCommand::sRendererAPI->GetDevice();

		D3D11_TEXTURE3D_DESC td = {};
		td.Width = mWidth;
		td.Height = mHeight;
		td.Depth = mDepth;
		td.MipLevels = 1;
		td.Format = mFormat;
		td.Usage = usage;
		td.BindFlags = bindFlags;
		td.CPUAccessFlags = cpuAccessFlags;

		HRESULT hr = device->CreateTexture3D(&td, nullptr, &mTexture3D);
		TOAST_CORE_ASSERT(SUCCEEDED(hr), "Unable to create Texture3D!");

		mResource = mTexture3D; // for GetResource()

		if (bindFlags & D3D11_BIND_UNORDERED_ACCESS)
			CreateUAV(0);

		CreateSRV();
	}

	void Texture3D::CreateSRV()
	{
		auto* device = RenderCommand::sRendererAPI->GetDevice();

		D3D11_SHADER_RESOURCE_VIEW_DESC sd = {};
		sd.Format = (mSRVFormat == DXGI_FORMAT_UNKNOWN) ? mFormat : mSRVFormat;
		sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
		sd.Texture3D.MostDetailedMip = 0;
		sd.Texture3D.MipLevels = 1;

		HRESULT hr = device->CreateShaderResourceView(mTexture3D.Get(), &sd, &mSRV);
		TOAST_CORE_ASSERT(SUCCEEDED(hr), "Unable to create SRV for Texture3D!");
	}

	void Texture3D::CreateUAV(uint32_t firstWSlice, uint32_t wSize)
	{
		auto* device = RenderCommand::sRendererAPI->GetDevice();

		D3D11_UNORDERED_ACCESS_VIEW_DESC ud = {};
		ud.Format = mFormat;
		ud.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
		ud.Texture3D.FirstWSlice = firstWSlice;
		ud.Texture3D.WSize = (wSize == 0) ? mDepth : wSize;

		HRESULT hr = device->CreateUnorderedAccessView(mTexture3D.Get(), &ud, &mUAV);
		TOAST_CORE_ASSERT(SUCCEEDED(hr), "Unable to create UAV for Texture3D!");
	}

	void Texture3D::SetData(const void* data, uint32_t rowPitch, uint32_t depthPitch)
	{
		TOAST_PROFILE_FUNCTION();

		ID3D11DeviceContext* deviceContext = RenderCommand::sRendererAPI->GetDeviceContext();

		// Describe the source data
		D3D11_BOX box = {};
		box.left = 0;
		box.top = 0;
		box.front = 0;
		box.right = mWidth;
		box.bottom = mHeight;
		box.back = mDepth;

		// Update entire resource
		deviceContext->UpdateSubresource(mTexture3D.Get(), 0,	&box, data,	rowPitch, depthPitch);
	}

	void Texture3D::Bind(uint32_t bindslot, D3D11_SHADER_TYPE shaderType) const
	{
		TOAST_PROFILE_FUNCTION();

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		switch (shaderType)
		{
		case D3D11_VERTEX_SHADER:
			deviceContext->VSSetShaderResources(bindslot, 1, mSRV.GetAddressOf());
			break;
		case D3D11_PIXEL_SHADER:
			deviceContext->PSSetShaderResources(bindslot, 1, mSRV.GetAddressOf());
			break;
		case D3D11_COMPUTE_SHADER:
			deviceContext->CSSetShaderResources(bindslot, 1, mSRV.GetAddressOf());
			break;
		}
	}

	void Texture3D::BindForReadWrite(uint32_t bindslot, D3D11_SHADER_TYPE shaderType) const
	{
		auto* ctx = RenderCommand::sRendererAPI->GetDeviceContext();
		ID3D11UnorderedAccessView* uav = mUAV.Get();
		ctx->CSSetUnorderedAccessViews(bindslot, 1, &uav, nullptr);
	}

	void Texture3D::UnbindUAV(uint32_t bindslot, D3D11_SHADER_TYPE shaderType) const
	{
		auto* ctx = RenderCommand::sRendererAPI->GetDeviceContext();
		ID3D11UnorderedAccessView* nullUAV = nullptr;
		ctx->CSSetUnorderedAccessViews(bindslot, 1, &nullUAV, nullptr);
	}

	////////////////////////////////////////////////////////////////////////////////////////  
	//     TEXTURECUBE   ///////////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	TextureCube::TextureCube(const std::string& filePath, uint32_t width, uint32_t height, uint32_t levels)
		: mFilePath(filePath), mWidth(width), mHeight(height), mMipLevels(levels)
	{
		TOAST_PROFILE_FUNCTION();
		D3D11_TEXTURE2D_DESC textureDesc = {};

		mFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();
		textureDesc.Width = mWidth;
		textureDesc.Height = mHeight;
		textureDesc.MipLevels = levels;
		textureDesc.ArraySize = 6;
		textureDesc.Format = mFormat;
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

		// Initialize UAVs: Resize to [6][mMipLevels] and initialize with nullptr
		mUAVs.resize(6, std::vector<Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>>(mMipLevels, nullptr));

		// Create UAVs for each face and mip level
		for (uint32_t face = 0; face < 6; ++face)
		{
			for (uint32_t mip = 0; mip < mMipLevels; ++mip)
			{
				D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
				uavDesc.Format = mFormat;
				uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
				uavDesc.Texture2DArray.MipSlice = mip;
				uavDesc.Texture2DArray.FirstArraySlice = face;
				uavDesc.Texture2DArray.ArraySize = 1;

				Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
				result = device->CreateUnorderedAccessView(mTexture.Get(), &uavDesc, &uav);
				assert(SUCCEEDED(result) && "Unable to create UAV for TextureCube!");

				mUAVs[face][mip] = uav;
			}
		}
	}

	TextureCube::TextureCube()
	{
		TOAST_PROFILE_FUNCTION();
		D3D11_TEXTURE2D_DESC textureDesc = {};

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();
		textureDesc.Width = 1;
		textureDesc.Height = 1;
		textureDesc.MipLevels = 1;
		textureDesc.ArraySize = 6;
		textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureDesc.CPUAccessFlags = 0;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Usage = D3D11_USAGE_DEFAULT;
		textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		textureDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

		// Create white pixel data
		UINT whitePixel[1] = { 0xFFFFFFFF }; // White color in RGBA8

		D3D11_SUBRESOURCE_DATA initData[6] = {};
		for (int i = 0; i < 6; ++i)
		{
			initData[i].pSysMem = whitePixel;
			initData[i].SysMemPitch = sizeof(UINT);
			initData[i].SysMemSlicePitch = 0;
		}

		HRESULT result = device->CreateTexture2D(&textureDesc, initData, &mTexture);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create texture!");

		CreateSRV();

		mSRV->GetResource(&mResource);
	}

	TextureCube::TextureCube(DXGI_FORMAT format, DXGI_FORMAT srvFormat, uint32_t width, uint32_t height, D3D11_USAGE usage, D3D11_BIND_FLAG bindFlag, uint32_t samples, UINT cpuAccessFlags, uint32_t mipLevels)
		: mFormat(format), mWidth(width), mHeight(height), mMipLevels(mipLevels)
	{
		D3D11_TEXTURE2D_DESC textureDesc = {};

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();
		textureDesc.Width = width;
		textureDesc.Height = height;
		textureDesc.MipLevels = 1;
		textureDesc.ArraySize = 6;
		textureDesc.Format = (DXGI_FORMAT)format;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Usage = usage;
		textureDesc.BindFlags = bindFlag;
		textureDesc.CPUAccessFlags = cpuAccessFlags;
		textureDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
		if (mipLevels == 0) {
			textureDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;
			textureDesc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
		}

		HRESULT result = device->CreateTexture2D(&textureDesc, nullptr, &mTexture);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create texture!");

		CreateSRV();

		if (bindFlag & D3D11_BIND_UNORDERED_ACCESS)
			CreateUAV(0);

		mSRV->GetResource(&mResource);
	}

	TextureCube::TextureCube(const std::string& filePath, DXGI_FORMAT format, uint32_t width, uint32_t height, uint32_t mipLevels)
		: mFilePath(filePath), mFormat(format), mWidth(width), mHeight(height), mMipLevels(mipLevels)
	{
		TOAST_PROFILE_FUNCTION();
		D3D11_TEXTURE2D_DESC textureDesc = {};

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();
		textureDesc.Width = mWidth;
		textureDesc.Height = mHeight;
		textureDesc.MipLevels = mipLevels;
		textureDesc.ArraySize = 6;
		textureDesc.Format = mFormat;
		textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Usage = D3D11_USAGE_DEFAULT;
		textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		textureDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
		if (mipLevels == 0) {
			textureDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;
			textureDesc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
		}

		HRESULT result = device->CreateTexture2D(&textureDesc, nullptr, &mTexture);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create texture!");

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		srvDesc.Texture2DArray.MostDetailedMip = 0;
		srvDesc.Texture2DArray.MipLevels = -1;  // all mips
		srvDesc.Texture2DArray.FirstArraySlice = 0;
		srvDesc.Texture2DArray.ArraySize = 6;

		device->CreateShaderResourceView(mTexture.Get(), &srvDesc, &mSRV);

		mSRV->GetResource(&mResource);

		// Initialize UAVs: Resize to [6][mMipLevels] and initialize with nullptr
		mUAVs.resize(6, std::vector<Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>>(mMipLevels, nullptr));

		// Create UAVs for each face and mip level
		for (uint32_t face = 0; face < 6; ++face)
		{
			for (uint32_t mip = 0; mip < mMipLevels; ++mip)
			{
				D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
				uavDesc.Format = mFormat;
				uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
				uavDesc.Texture2DArray.MipSlice = mip;
				uavDesc.Texture2DArray.FirstArraySlice = face;
				uavDesc.Texture2DArray.ArraySize = 1;

				Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
				result = device->CreateUnorderedAccessView(mTexture.Get(), &uavDesc, &uav);
				assert(SUCCEEDED(result) && "Unable to create UAV for TextureCube!");

				mUAVs[face][mip] = uav;
			}
		}
	}

	const uint32_t TextureCube::GetMipLevelCount() const
	{ 
		return mMipLevels;
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

	void TextureCube::BindForReadWriteUpdated(uint32_t bindSlot, D3D11_SHADER_TYPE shaderType, uint32_t mipLevel, uint32_t faceIndex) const
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		if (faceIndex >= mUAVs.size() || mipLevel >= mUAVs[0].size())
			return;

		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav = mUAVs[faceIndex][mipLevel];
		if (!uav)
			return;

		switch (shaderType)
		{
		case D3D11_COMPUTE_SHADER:
			deviceContext->CSSetUnorderedAccessViews(bindSlot, 1, uav.GetAddressOf(), nullptr);
			break;
			// Handle other shader types if necessary
		default:
			break;
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

	void TextureCube::UnbindUAVUpdated(uint32_t bindSlot, D3D11_SHADER_TYPE shaderType) const
	{
		ID3D11UnorderedAccessView* nullUAV = nullptr;
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		switch (shaderType)
		{
		case D3D11_COMPUTE_SHADER:
			deviceContext->CSSetUnorderedAccessViews(bindSlot, 1, &nullUAV, nullptr);
			break;
			// Handle other shader types if necessary
		default:
			break;
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
		shaderResourceViewDesc.TextureCube.MipLevels = desc.MipLevels;
		shaderResourceViewDesc.TextureCube.MostDetailedMip = 0;

		HRESULT result = device->CreateShaderResourceView(mTexture.Get(), &shaderResourceViewDesc, &mSRV);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create the SRV!");
	}

	void TextureCube::CreateUAV(uint32_t mipLevel)
	{
		D3D11_TEXTURE2D_DESC desc = {};
		mTexture->GetDesc(&desc);

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = desc.Format;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
		uavDesc.Texture2DArray.MipSlice = mipLevel;
		uavDesc.Texture2DArray.FirstArraySlice = 0;
		uavDesc.Texture2DArray.ArraySize = 6;

		HRESULT result = device->CreateUnorderedAccessView(mTexture.Get(), &uavDesc, &mUAV);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create the UAV!");
	}

	void TextureCube::CreateUAVUpdated(uint32_t mipLevel, uint32_t faceIndex)
	{
		// Ensure mipLevel and faceIndex are within valid ranges
		if (faceIndex >= 6)
			return;

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		// Describe the UAV
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = mFormat;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
		uavDesc.Texture2DArray.MipSlice = mipLevel;
		uavDesc.Texture2DArray.FirstArraySlice = faceIndex;
		uavDesc.Texture2DArray.ArraySize = 1;

		// Create the UAV
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
		HRESULT hr = device->CreateUnorderedAccessView(mTexture.Get(), &uavDesc, &uav);
		if (SUCCEEDED(hr))
		{
			// Store UAVs in a 2D vector [face][mip]
			if (mUAVs.size() < 6)
				mUAVs.resize(6, std::vector<Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>>(mMipLevels, nullptr));

			mUAVs[faceIndex][mipLevel] = uav;
		}
	}

	void TextureCube::GenerateMips() const
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		deviceContext->GenerateMips(mSRV.Get());
	}

	////////////////////////////////////////////////////////////////////////////////////////  
	//     TEXTURE2DARRAY    ///////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	Texture2DArray::Texture2DArray(DXGI_FORMAT format, uint32_t width, uint32_t height, uint32_t arraySize,
		D3D11_USAGE usage, D3D11_BIND_FLAG bindFlag, uint32_t samples, UINT cpuAccessFlags)
		: mWidth(width), mHeight(height), mArraySize(arraySize), mFormat(format)
	{
		D3D11_TEXTURE2D_DESC textureDesc = {};
		textureDesc.ArraySize = mArraySize;
		textureDesc.BindFlags = bindFlag;
		textureDesc.Usage = usage;
		textureDesc.CPUAccessFlags = cpuAccessFlags;
		textureDesc.Format = format;
		textureDesc.Height = mHeight;
		textureDesc.Width = mWidth;
		textureDesc.MipLevels = 1;
		textureDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;;
		textureDesc.SampleDesc.Count = samples;
		textureDesc.SampleDesc.Quality = 0;

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		HRESULT result = device->CreateTexture2D(&textureDesc, nullptr, &mTexture);
		TOAST_CORE_ASSERT(SUCCEEDED(result),"Unable to create texture array!");

		CreateSRV();
	}

	Texture2DArray::Texture2DArray(DXGI_FORMAT format, uint32_t width, uint32_t height, uint32_t arraySize,
		D3D11_USAGE usage, D3D11_BIND_FLAG bindFlag, uint32_t samples, UINT cpuAccessFlags,	const std::vector<const void*>& initialData,
		const std::vector<UINT>& rowPitches)
		: mWidth(width), mHeight(height), mArraySize(arraySize), mFormat(format)
	{
		TOAST_CORE_ASSERT(initialData.size() == arraySize && rowPitches.size() == arraySize, "Wrong initial data");

		D3D11_TEXTURE2D_DESC textureDesc = {};
		textureDesc.ArraySize = mArraySize;
		textureDesc.BindFlags = bindFlag;
		textureDesc.Usage = usage;
		textureDesc.CPUAccessFlags = cpuAccessFlags;
		textureDesc.Format = format;
		textureDesc.Height = mHeight;
		textureDesc.Width = mWidth;
		textureDesc.MipLevels = 1;
		textureDesc.MiscFlags = 0;
		textureDesc.SampleDesc.Count = samples;
		textureDesc.SampleDesc.Quality = 0;

		std::vector<D3D11_SUBRESOURCE_DATA> subresources(mArraySize);
		for (uint32_t i = 0; i < mArraySize; ++i)
		{
			subresources[i].pSysMem = initialData[i];
			subresources[i].SysMemPitch = rowPitches[i];
			subresources[i].SysMemSlicePitch = 0;
		}

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		HRESULT result = device->CreateTexture2D(&textureDesc, subresources.data(), &mTexture);
		TOAST_CORE_ASSERT(SUCCEEDED(result),"Unable to create texture array with initial data!");

		CreateSRV();
	}

	void Texture2DArray::CreateSRV()
	{
		D3D11_TEXTURE2D_DESC desc = {};
		mTexture->GetDesc(&desc);

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = desc.Format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		srvDesc.Texture2DArray.MostDetailedMip = 0;
		srvDesc.Texture2DArray.MipLevels = desc.MipLevels;
		srvDesc.Texture2DArray.FirstArraySlice = 0;
		srvDesc.Texture2DArray.ArraySize = mArraySize;

		HRESULT result = device->CreateShaderResourceView(mTexture.Get(), &srvDesc, &mSRV);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create texture array SRV!");
	}

	void Texture2DArray::Bind(uint32_t bindslot, D3D11_SHADER_TYPE shaderType) const
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		switch (shaderType)
		{
		case D3D11_VERTEX_SHADER:
			deviceContext->VSSetShaderResources(bindslot, 1, mSRV.GetAddressOf());
			break;
		case D3D11_PIXEL_SHADER:
			deviceContext->PSSetShaderResources(bindslot, 1, mSRV.GetAddressOf());
			break;
		case D3D11_COMPUTE_SHADER:
			deviceContext->CSSetShaderResources(bindslot, 1, mSRV.GetAddressOf());
			break;
		default:
			break;
		}
	}

	void Texture2DArray::GenerateMips() const
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		deviceContext->GenerateMips(mSRV.Get());
	}

	////////////////////////////////////////////////////////////////////////////////////////  
	//     TEXTURE SAMPLER   ///////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	TextureSampler::TextureSampler(D3D11_FILTER filter, D3D11_TEXTURE_ADDRESS_MODE addressMode, float mipLODBias)
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		D3D11_SAMPLER_DESC desc = {};
		desc.Filter = filter;
		desc.AddressU = addressMode;
		desc.AddressV = addressMode;
		desc.AddressW = addressMode;
		desc.MaxAnisotropy = (filter == D3D11_FILTER_ANISOTROPIC) ? D3D11_REQ_MAXANISOTROPY : 1;
		desc.MipLODBias = mipLODBias;
		desc.MinLOD = 0;
		desc.MaxLOD = D3D11_FLOAT32_MAX;

		HRESULT result = device->CreateSamplerState(&desc, &mSamplerState);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create the sampler!");
	}

	TextureSampler::TextureSampler(D3D11_FILTER filter, D3D11_TEXTURE_ADDRESS_MODE uAddressMode, D3D11_TEXTURE_ADDRESS_MODE vAddressMode, float mipLODBias)
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		D3D11_SAMPLER_DESC desc = {};
		desc.Filter = filter;
		desc.AddressU = uAddressMode;
		desc.AddressV = vAddressMode;
		desc.AddressW = uAddressMode;
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

	Texture2D* TextureLibrary::LoadTexture2D(const std::string& filePath, const bool sRGB)
	{
		if (Exists(filePath)) 
			return dynamic_cast<Texture2D*>(mTextures[filePath].get());

		mTextures[filePath] = CreateScope<Texture2D>(filePath, sRGB);

		return dynamic_cast<Texture2D*>(mTextures[filePath].get());
	}

	TextureCube* TextureLibrary::LoadTextureCube(const std::string& filePath, uint32_t width, uint32_t height, uint32_t levels)
	{
		if (Exists(filePath))
			return (TextureCube*)mTextures[filePath].get();

		mTextures[filePath] = CreateScope<TextureCube>(filePath, width, height, levels);
		return (TextureCube*)mTextures[filePath].get();
	}

	TextureSampler* TextureLibrary::LoadTextureSampler(const std::string& name, D3D11_FILTER filter, D3D11_TEXTURE_ADDRESS_MODE addressMode, float mipLODBias)
	{
		mTextureSamplers[name] = CreateScope<TextureSampler>(filter, addressMode, mipLODBias);
		return mTextureSamplers[name].get();
	}

	TextureSampler* TextureLibrary::LoadTextureSampler(const std::string& name, D3D11_FILTER filter, D3D11_TEXTURE_ADDRESS_MODE uAddressMode, D3D11_TEXTURE_ADDRESS_MODE vAddressMode, float mipLODBias)
	{
		mTextureSamplers[name] = CreateScope<TextureSampler>(filter, uAddressMode, vAddressMode, mipLODBias);
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