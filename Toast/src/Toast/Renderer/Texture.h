#pragma once

#include <d3d11.h>
#include <d3d11shadertracing.h>
#include <wrl.h>
#include <string>

#include "Toast/Core/Base.h"

namespace Toast {

	class Texture 
	{
	public:
		virtual ~Texture() = default;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;
		virtual void* GetID() const = 0;

		virtual void SetData(void* data, uint32_t size) = 0;

		virtual void Bind() const = 0;
	};

	class Texture2D
	{
	public:
		Texture2D(uint32_t width, uint32_t height, uint32_t slot, D3D11_SHADER_TYPE shaderType, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);
		Texture2D(const std::string& path, uint32_t slot, D3D11_SHADER_TYPE shaderType, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);
		~Texture2D();

		uint32_t GetWidth() const { return mWidth; }
		uint32_t GetHeight() const { return mHeight; }
		uint32_t GetBindPoint() const { return mShaderSlot; }
		D3D11_SHADER_TYPE GetShaderType() const { return mShaderType; }
		std::string GetPath() const { return mPath; }
		void* GetID() const { return (void*)mView.Get(); }

		void SetData(void* data, uint32_t size);

		virtual void Bind() const;

		bool operator==(const Texture2D& other) const
		{
			return mView == ((Texture2D&)other).mView;
		};

		void CreateSampler(D3D11_FILTER filter, D3D11_TEXTURE_ADDRESS_MODE addressMode);
	private:
		std::string mPath;
		uint32_t mWidth, mHeight, mShaderSlot;
		D3D11_SHADER_TYPE mShaderType;

		Microsoft::WRL::ComPtr<ID3D11Resource> mResource;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mView;
		Microsoft::WRL::ComPtr<ID3D11SamplerState> mSamplerState;
	};

	class TextureCube
	{
	public:
		TextureCube(uint32_t width, uint32_t height, uint32_t slot, D3D11_SHADER_TYPE shaderType, uint32_t levels = 0);
		~TextureCube();

		uint32_t GetWidth() const { return mWidth; }
		uint32_t GetHeight() const { return mHeight; }
		uint32_t GetBindPoint() const { return mShaderSlot; }
		uint32_t GetMipLevelCount() const { return mLevels; }
		ID3D11Resource* GetResource() const { return mResource.Get(); }
		D3D11_SHADER_TYPE GetShaderType() const { return mShaderType; }
		void SetShaderType(D3D11_SHADER_TYPE type) { mShaderType = type; }
		std::string GetPath() const { return mPath; }
		ID3D11ShaderResourceView* GetID() const { return mSRV.Get(); }

		uint32_t CalculateMipMapCount(uint32_t cubemapSize);

		void SetData(void* data, uint32_t size);

		void BindForSampling() const;
		void BindForReadWrite() const;
		void UnbindUAV() const;
		void CreateSampler(D3D11_FILTER filter, D3D11_TEXTURE_ADDRESS_MODE addressMode);
		void CreateSRV();
		void CreateUAV(uint32_t mipSlice);

		void GenerateMips();
	private:
		std::string mPath;
		uint32_t mWidth, mHeight, mShaderSlot, mLevels;
		D3D11_SHADER_TYPE mShaderType;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> mTexture;
		Microsoft::WRL::ComPtr<ID3D11Resource> mResource;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mSRV;
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> mUAV;
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> mNullUAV = { nullptr };
		Microsoft::WRL::ComPtr<ID3D11SamplerState> mSamplerState;
	};
}