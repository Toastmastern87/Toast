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

		virtual void CreateSRV() = 0;

		virtual void Bind(uint32_t bindslot = 0, D3D11_SHADER_TYPE shaderType = D3D11_VERTEX_SHADER) const = 0;

		virtual const uint32_t GetWidth() const = 0;
		virtual const uint32_t GetHeight() const = 0;
		virtual const std::string GetFilePath() const = 0;
		virtual void* GetID() const = 0;
		virtual const uint32_t GetMipLevelCount() const = 0;

		static uint32_t CalculateMipMapCount(uint32_t width, uint32_t height);

		virtual bool operator==(const Texture& other) const = 0;
	};

	class Texture2D : public Texture
	{
	public:
		Texture2D(DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM, uint32_t width = 1, uint32_t height = 1);
		Texture2D(const std::string& filePath);
		~Texture2D() = default;

		virtual void CreateSRV() override;

		virtual void Bind(uint32_t bindslot = 0, D3D11_SHADER_TYPE shaderType = D3D11_VERTEX_SHADER) const override;

		virtual const uint32_t GetWidth() const override { return mWidth; }
		virtual const uint32_t GetHeight() const override { return mHeight; }
		virtual const std::string GetFilePath() const override { return mFilePath; }
		virtual void* GetID() const override { return (void*)mSRV.Get(); }
		virtual const uint32_t GetMipLevelCount() const override;

		void SetData(void* data, uint32_t size);

		void BindForReadWrite(uint32_t bindslot = 0, D3D11_SHADER_TYPE shaderType = D3D11_COMPUTE_SHADER) const;
		void UnbindUAV(uint32_t bindslot = 0, D3D11_SHADER_TYPE shaderType = D3D11_COMPUTE_SHADER) const;
		void CreateUAV(uint32_t mipSlice);

		virtual bool operator==(const Texture& other) const override
		{
			return mSRV == ((Texture2D&)other).mSRV;
		};
	private:
		std::string mFilePath = "";
		uint32_t mWidth, mHeight;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> mTexture;
		Microsoft::WRL::ComPtr<ID3D11Resource> mResource;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mSRV;
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> mUAV;
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> mNullUAV = { nullptr };
	};

	class TextureCube : public Texture
	{
	public:
		TextureCube(const std::string& filePath, uint32_t width, uint32_t height, uint32_t levels = 0);
		~TextureCube() = default;

		virtual void CreateSRV() override;

		virtual void Bind(uint32_t bindslot = 0, D3D11_SHADER_TYPE shaderType = D3D11_VERTEX_SHADER) const override;

		virtual const uint32_t GetWidth() const override { return mWidth; }
		virtual const uint32_t GetHeight() const override { return mHeight; }
		virtual const std::string GetFilePath() const override { return mFilePath; }
		virtual void* GetID() const override { return (void*)mSRV.Get(); }
		virtual const uint32_t GetMipLevelCount() const override;

		ID3D11Resource* GetResource() const { return mResource.Get(); }
		void SetData(void* data, uint32_t size);

		void BindForReadWrite(uint32_t bindslot = 0, D3D11_SHADER_TYPE shaderType = D3D11_COMPUTE_SHADER) const;
		void UnbindUAV(uint32_t bindslot = 0, D3D11_SHADER_TYPE shaderType = D3D11_COMPUTE_SHADER) const;
		void CreateUAV(uint32_t mipSlice);

		void GenerateMips();

		virtual bool operator==(const Texture& other) const override
		{
			return mSRV == ((TextureCube&)other).mSRV;
		};
	private:
		std::string mFilePath = "";
		uint32_t mWidth, mHeight, mLevels;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> mTexture;
		Microsoft::WRL::ComPtr<ID3D11Resource> mResource;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mSRV;
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
		static void Add(const Ref<Texture>& texture);
		static void AddSampler(const std::string& name, const Ref<TextureSampler>& sampler);
		static Ref<Texture2D> LoadTexture2D(const std::string& filePath);
		static Ref<TextureCube> LoadTextureCube(const std::string& filePath, uint32_t width, uint32_t height, uint32_t levels = 0);
		static Ref<TextureSampler> LoadTextureSampler(const std::string& name, D3D11_FILTER filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_MODE addressMode = D3D11_TEXTURE_ADDRESS_WRAP);

		static Ref<Texture> Get(const std::string& name);
		static Ref<TextureSampler> GetSampler(const std::string& name);
		static std::unordered_map<std::string, Ref<Texture>> GetTextures() { return mTextures; }

		static bool Exists(const std::string& name);
		static bool ExistsSampler(const std::string& name);
	private:
		static std::unordered_map<std::string, Ref<Texture>> mTextures;
		static std::unordered_map<std::string, Ref<TextureSampler>> mTextureSamplers;
	};

}