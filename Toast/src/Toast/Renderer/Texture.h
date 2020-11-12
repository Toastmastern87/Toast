#pragma once

#include <d3d11.h>
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
		Texture2D(uint32_t width, uint32_t height, uint32_t slot);
		Texture2D(const std::string& path, uint32_t slot);
		~Texture2D();

		uint32_t GetWidth() const { return mWidth; }
		uint32_t GetHeight() const { return mHeight; }
		uint32_t GetBindPoint() const { return mShaderSlot; }
		std::string GetPath() const { return mPath; }
		void* GetID() const { return (void*)mView.Get(); }

		void SetData(void* data, uint32_t size);

		void Bind() const;

		bool operator==(const Texture2D& other) const
		{
			return mView == ((Texture2D&)other).mView;
		};
	private:
		void CreateSampler();

	private:
		std::string mPath;
		uint32_t mWidth, mHeight, mShaderSlot;

		Microsoft::WRL::ComPtr<ID3D11Resource> mResource;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mView;
		Microsoft::WRL::ComPtr<ID3D11SamplerState> mSamplerState;
	};
}