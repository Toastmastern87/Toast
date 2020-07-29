#pragma once

#include <d3d11.h>

#include "Toast/Renderer/Texture.h"

namespace Toast {

	class DirectXTexture2D : public Texture2D 
	{
	public:
		DirectXTexture2D(const std::string& path);
		virtual ~DirectXTexture2D();

		virtual uint32_t GetWidth() const override { return mWidth; }
		virtual uint32_t GetHeight() const override { return mHeight; }

		virtual void Bind() const override;

	private:
		std::string mPath;
		uint32_t mWidth, mHeight;

		ID3D11Device* mDevice = nullptr;
		ID3D11DeviceContext* mDeviceContext = nullptr;
		ID3D11Resource* mResource = nullptr;
		ID3D11ShaderResourceView* mView = nullptr;
		ID3D11SamplerState* mSamplerState = nullptr;
	};
}
