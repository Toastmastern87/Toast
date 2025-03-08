#pragma once

#include "Toast/Core/Base.h"

#include "Toast/Renderer/Formats.h"
#include "Toast/Renderer/Texture.h"

#include <d3d11.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <wrl.h>

namespace Toast {

	enum class RenderTargetType
	{
		Color = 0,
		Depth = 1,
		ColorCube = 2
	};

	class RenderTarget 
	{
	public:
		RenderTarget(RenderTargetType type, uint32_t width, uint32_t height, uint32_t samples, TextureFormat format, bool swapChainTarget = false, bool blending = false);
		~RenderTarget() = default;

		void Init(RenderTargetType type, uint32_t width, uint32_t height, uint32_t samples, TextureFormat format, bool swapChainTarget = false, bool blending = false);
		void Clear(const DirectX::XMFLOAT4 clearColor);
		void Clean();
		void Resize(uint32_t width, uint32_t height);

		std::tuple<uint32_t, uint32_t> GetSize() { return { mWitdh, mHeight }; }

		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> GetRTV();
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> GetRTVFace(uint32_t faceIndex);
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetSRV();

		Microsoft::WRL::ComPtr<ID3D11Texture2D> GetTexture() { return mTexture->GetTexture(); }
		Texture* GetTextureOriginal() { return mTexture.get(); }

		TextureFormat GetFormat() { return mFormat; }

		const D3D11_RENDER_TARGET_BLEND_DESC& GetBlendDesc() const { return mBlendDesc; }
		void SetBlendDesc(const D3D11_RENDER_TARGET_BLEND_DESC& blendDesc) { mBlendDesc = blendDesc; }

		void Unbind();

		template<typename T>
		T ReadPixel(uint32_t x, uint32_t y) 
		{
			HRESULT result;
			D3D11_TEXTURE2D_DESC textureDesc = {};
			D3D11_TEXTURE2D_DESC sourceDesc;
			Microsoft::WRL::ComPtr<ID3D11Texture2D> stagedTexture;

			// Define the source region (one pixel)
			D3D11_BOX srcBox = { x, y, 0, x + 1, y + 1, 1 };

			RendererAPI* API = RenderCommand::sRendererAPI.get();
			ID3D11Device* device = API->GetDevice();
			ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

			mTexture->GetTexture()->GetDesc(&sourceDesc);

			// Out-of-bound check
			if (x >= sourceDesc.Width || y >= sourceDesc.Height)
				return T(); 

			// Create a staging texture with the same format as the source texture.
			textureDesc.Width = 1;
			textureDesc.Height = 1;
			textureDesc.MipLevels = 1;
			textureDesc.ArraySize = 1;
			textureDesc.Format = sourceDesc.Format; // Use the same format as the source
			textureDesc.SampleDesc.Count = 1;
			textureDesc.SampleDesc.Quality = 0;
			textureDesc.Usage = D3D11_USAGE_STAGING;
			textureDesc.BindFlags = 0;
			textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			textureDesc.MiscFlags = 0;

			result = device->CreateTexture2D(&textureDesc, nullptr, &stagedTexture);
			TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to create staging 2D texture!");

			deviceContext->CopySubresourceRegion(
				stagedTexture.Get(), 0, 0, 0, 0,
				mTexture->GetTexture().Get(), 0, &srcBox
			);

			D3D11_MAPPED_SUBRESOURCE msr;
			HRESULT hr = deviceContext->Map(stagedTexture.Get(), 0, D3D11_MAP_READ, 0, &msr);
			if (FAILED(hr))
				return T();

			T value;
			if constexpr (std::is_same_v<T, int>)
			{
				value = *reinterpret_cast<int*>(msr.pData);
			}
			else if constexpr (std::is_same_v<T, DirectX::XMFLOAT4>)
			{
				// Check the source texture format.
				if (sourceDesc.Format == DXGI_FORMAT_R32G32B32A32_FLOAT)
				{
					float* pixelData = reinterpret_cast<float*>(msr.pData);
					value.x = pixelData[0];
					value.y = pixelData[1];
					value.z = pixelData[2];
					value.w = pixelData[3];
				}
				else if (sourceDesc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT)
				{
					// For half-precision floats, use XMConvertHalfToFloat.
					uint16_t* pixelData = reinterpret_cast<uint16_t*>(msr.pData);
					value.x = DirectX::PackedVector::XMConvertHalfToFloat(pixelData[0]);
					value.y = DirectX::PackedVector::XMConvertHalfToFloat(pixelData[1]);
					value.z = DirectX::PackedVector::XMConvertHalfToFloat(pixelData[2]);
					value.w = DirectX::PackedVector::XMConvertHalfToFloat(pixelData[3]);
				}
				else
				{
					// Fallback: assume float data
					float* pixelData = reinterpret_cast<float*>(msr.pData);
					value.x = pixelData[0];
					value.y = pixelData[1];
					value.z = pixelData[2];
					value.w = pixelData[3];
				}
			}
			else
			{
				// Generic fallback: copy raw bytes into value.
				std::memcpy(&value, msr.pData, sizeof(T));
			}

			deviceContext->Unmap(stagedTexture.Get(), 0);
			return value;
		}
	private:
		bool IsIntegerFormat(TextureFormat format);
	private:
		RenderTargetType mType;
		uint32_t mWitdh, mHeight, mSamples;
		TextureFormat mFormat, mDepthFormat;

		bool mSwapChainTarget = false;

		Scope<Texture> mTexture;

		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> mRTV;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> mRTVArray[6];

		D3D11_RENDER_TARGET_BLEND_DESC mBlendDesc;
	};
}