#pragma once

#include <d3d11.h>
#include <wrl.h>

#include "Toast/Renderer/Texture.h"

namespace Toast {

	class DirectXTexture2D : public Texture2D 
	{
	public:
		DirectXTexture2D(uint32_t width, uint32_t height, uint32_t slot);
		DirectXTexture2D(const std::string& path, uint32_t slot);
		virtual ~DirectXTexture2D();

		virtual uint32_t GetWidth() const override { return mWidth; }
		virtual uint32_t GetHeight() const override { return mHeight; }
		virtual void* GetID() const override { return (void*)mView.Get(); }

		virtual void SetData(void* data, uint32_t size) override;

		virtual void Bind() const override;

		virtual bool operator==(const Texture& other) const override
		{
			return mView == ((DirectXTexture2D&)other).mView;
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
