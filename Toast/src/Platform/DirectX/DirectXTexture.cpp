#include "tpch.h"
#include "DirectXTexture.h"

#include "Toast/Application.h"

#include <WICTextureLoader.h>

namespace Toast {

	DirectXTexture2D::DirectXTexture2D(const std::string& path) 
		: mPath(path)
	{
		HRESULT result;

		Application& app = Application::Get();
		mDevice = app.GetWindow().GetContext()->GetDevice();
		mDeviceContext = app.GetWindow().GetContext()->GetDeviceContext();

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
		CLEAN(mResource);
		CLEAN(mView);
		CLEAN(mSamplerState);
	}

	void DirectXTexture2D::Bind() const 
	{
		mDeviceContext->PSSetSamplers(0, 1, &mSamplerState);
		mDeviceContext->PSSetShaderResources(0, 1, &mView);
	}
}