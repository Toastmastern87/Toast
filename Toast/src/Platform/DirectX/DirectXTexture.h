#pragma once

#include <d3d11.h>

#include "Toast/Renderer/Texture.h"

namespace Toast {

	class DirectXTexture2D : public Texture2D 
	{
	public:
		DirectXTexture2D(uint32_t width, uint32_t height, uint32_t id);
		DirectXTexture2D(const std::string& path, uint32_t id);
		virtual ~DirectXTexture2D();

		virtual uint32_t GetWidth() const override { return mWidth; }
		virtual uint32_t GetHeight() const override { return mHeight; }

		virtual void SetData(void* data, uint32_t size) override;

		virtual void Bind() const override;

		virtual bool operator==(const Texture& other) const override
		{
			return mID == ((DirectXTexture2D&)other).mID;
		};
	private:
		void CreateSampler();

	private:
		std::string mPath;
		uint32_t mWidth, mHeight, mID;

		ID3D11Device* mDevice = nullptr;
		ID3D11DeviceContext* mDeviceContext = nullptr;
		ID3D11Resource* mResource = nullptr;
		ID3D11ShaderResourceView* mView = nullptr;
		ID3D11SamplerState* mSamplerState = nullptr;
	};
}
