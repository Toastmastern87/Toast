#pragma once

#include <d3d11.h>
#include <d3d11shadertracing.h>
#include <wrl.h>
#include <string>

#include "Toast/Core/Base.h"

#include <../vendor/directxtex/include/DirectXTex.h>

namespace Toast {

	class Texture 
	{
	public:
		virtual ~Texture() = default;

		virtual void CreateSRV() = 0;

		virtual void Bind(uint32_t bindslot = 0, D3D11_SHADER_TYPE shaderType = D3D11_VERTEX_SHADER) const = 0;

		virtual const uint32_t GetWidth() const = 0;
		virtual const uint32_t GetHeight() const = 0;
		virtual const std::string GetFilePath() const = 0;
		virtual void* GetID() const = 0;
		virtual Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetSRV() const = 0;
		virtual Microsoft::WRL::ComPtr<ID3D11Texture2D> GetTexture() const = 0;
		virtual const uint32_t GetMipLevelCount() const = 0;

		static uint32_t CalculateMipMapCount(uint32_t width, uint32_t height);
		virtual void GenerateMips() const = 0;

		virtual ID3D11Resource* GetResource() const = 0;

		virtual bool operator==(const Texture& other) const = 0;
	};

	class Texture2D : public Texture
	{
	public:
		Texture2D(DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT srvFormat = DXGI_FORMAT_UNKNOWN, uint32_t width = 1, uint32_t height = 1, D3D11_USAGE usage = D3D11_USAGE_DEFAULT, D3D11_BIND_FLAG bindFlag = (D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS), uint32_t samples = 1, UINT cpuAccessFlags = 0);
		Texture2D(const std::string& filePath, bool forceSRGB = true);
		~Texture2D() = default;

		virtual void CreateSRV() override;

		virtual void Bind(uint32_t bindslot = 0, D3D11_SHADER_TYPE shaderType = D3D11_VERTEX_SHADER) const override;

		virtual const uint32_t GetWidth() const override { return mWidth; }
		virtual const uint32_t GetHeight() const override { return mHeight; }
		virtual const std::string GetFilePath() const override { return mFilePath; }
		virtual void* GetID() const override { return (void*)mSRV.Get(); }
		virtual Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetSRV() const override { return mSRV; }
		virtual const uint32_t GetMipLevelCount() const override;

		void SetData(void* data, uint32_t size);

		void BindForReadWrite(uint32_t bindslot = 0, D3D11_SHADER_TYPE shaderType = D3D11_COMPUTE_SHADER) const;
		void UnbindUAV(uint32_t bindslot = 0, D3D11_SHADER_TYPE shaderType = D3D11_COMPUTE_SHADER) const;
		void CreateUAV(uint32_t mipSlice);

		virtual void GenerateMips() const override;

		virtual ID3D11Resource* GetResource() const override { return mResource.Get(); }

		virtual bool operator==(const Texture& other) const override
		{
			return mSRV == ((Texture2D&)other).mSRV;
		};

		virtual Microsoft::WRL::ComPtr<ID3D11Texture2D> GetTexture() const override { return mTexture; }
	private:
		std::string mFilePath = "";
		uint32_t mWidth, mHeight;

		DXGI_FORMAT mFormat, mSRVFormat;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> mTexture;
		Microsoft::WRL::ComPtr<ID3D11Resource> mResource;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mSRV;
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> mUAV;

		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> mNullUAV = { nullptr };
	};

	class TextureCube : public Texture
	{
	public:
		TextureCube(DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT srvFormat = DXGI_FORMAT_UNKNOWN, uint32_t width = 1, uint32_t height = 1, D3D11_USAGE usage = D3D11_USAGE_DEFAULT, D3D11_BIND_FLAG bindFlag = (D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS), uint32_t samples = 1, UINT cpuAccessFlags = 0, uint32_t mipLevels = 1);
		TextureCube(const std::string& filePath, uint32_t width, uint32_t height, uint32_t mipLevels = 0);
		TextureCube();
		~TextureCube() = default;

		virtual void CreateSRV() override;

		virtual void Bind(uint32_t bindslot = 0, D3D11_SHADER_TYPE shaderType = D3D11_VERTEX_SHADER) const override;

		virtual const uint32_t GetWidth() const override { return mWidth; }
		virtual const uint32_t GetHeight() const override { return mHeight; }
		virtual const std::string GetFilePath() const override { return mFilePath; }
		virtual void* GetID() const override { return (void*)mSRV.Get(); }
		virtual Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetSRV() const override { return mSRV; }
		virtual const uint32_t GetMipLevelCount() const override;

		virtual ID3D11Resource* GetResource() const override { return mResource.Get(); }
		void SetData(void* data, uint32_t size);

		void BindForReadWrite(uint32_t bindslot = 0, D3D11_SHADER_TYPE shaderType = D3D11_COMPUTE_SHADER) const;
		void BindForReadWriteUpdated(uint32_t bindSlot, D3D11_SHADER_TYPE shaderType, uint32_t mipLevel, uint32_t faceIndex) const;
		void UnbindUAV(uint32_t bindslot = 0, D3D11_SHADER_TYPE shaderType = D3D11_COMPUTE_SHADER) const;
		void UnbindUAVUpdated(uint32_t bindSlot, D3D11_SHADER_TYPE shaderType) const;
		void CreateUAV(uint32_t mipSlice);
		void CreateUAVUpdated(uint32_t mipLevel, uint32_t faceIndex);

		virtual void GenerateMips() const override;

		virtual bool operator==(const Texture& other) const override
		{
			return mSRV == ((TextureCube&)other).mSRV;
		};

		virtual Microsoft::WRL::ComPtr<ID3D11Texture2D> GetTexture() const override { return mTexture; }
	private:
		std::string mFilePath = "";
		uint32_t mWidth, mHeight, mMipLevels;
		DXGI_FORMAT mFormat;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> mTexture;
		Microsoft::WRL::ComPtr<ID3D11Resource> mResource;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mSRV;

		std::vector<std::vector<Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>>> mUAVs; // [face][mip]

		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> mUAV;
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> mNullUAV = { nullptr };
	};

	class TextureSampler
	{
	public:
		TextureSampler(D3D11_FILTER filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_MODE addressMode = D3D11_TEXTURE_ADDRESS_WRAP);
		~TextureSampler() = default;

		void Bind(uint32_t bindslot = 0, D3D11_SHADER_TYPE shaderType = D3D11_VERTEX_SHADER) const;
	private:
		Microsoft::WRL::ComPtr<ID3D11SamplerState> mSamplerState;
	};

	class TextureLibrary
	{
	public:
		static Texture2D* LoadTexture2D(const std::string& filePath);
		static TextureCube* LoadTextureCube(const std::string& filePath, uint32_t width, uint32_t height, uint32_t levels = 0);
		static TextureSampler* LoadTextureSampler(const std::string& name, D3D11_FILTER filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_MODE addressMode = D3D11_TEXTURE_ADDRESS_WRAP);

		static Texture* Get(const std::string& name);
		static TextureSampler* GetSampler(const std::string& name);
		static std::unordered_map<std::string, Scope<Texture>>& GetTextures() { return mTextures; }

		static bool Exists(const std::string& name);
		static bool ExistsSampler(const std::string& name);
	private:
		static std::unordered_map<std::string, Scope<Texture>> mTextures;
		static std::unordered_map<std::string, Scope<TextureSampler>> mTextureSamplers;
	};

}