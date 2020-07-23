#pragma once

#include <d3d11.h>
#include <DirectXMath.h>

#include "Toast/Renderer/Shader.h"

namespace Toast {

	class DirectXShader : public Shader
	{
	public:
		DirectXShader(const std::string& vertexSrc, const std::string& pixelSrc);
		virtual ~DirectXShader();

		virtual void Bind() const override;
		virtual void Unbind() const override;
		virtual void UploadConstantBuffer(const std::string& name, const DirectX::XMMATRIX& matrix) const override;

		ID3D10Blob* GetVSRaw() const { return mVSRaw; }

	private:
		ID3D11VertexShader* mVertexShader = nullptr;
		ID3D11PixelShader* mPixelShader = nullptr;
		ID3D10Blob* mVSRaw = nullptr;

		ID3D11Device* mDevice;
		ID3D11DeviceContext* mDeviceContext;
	};
}