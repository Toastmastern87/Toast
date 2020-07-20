#pragma once

#include <d3d11.h>

namespace Toast {

	class Shader {
	public:
		Shader(const std::string& vertexSrc, const std::string& pixelSrc);
		~Shader();

		void Bind() const;
		void Unbind() const;

		ID3D10Blob* GetVSRaw() const { return mVSRaw; }

	private:
		ID3D11VertexShader* mVertexShader = nullptr;
		ID3D11PixelShader* mPixelShader = nullptr;
		ID3D10Blob* mVSRaw = nullptr;

		ID3D11Device* mDevice;
		ID3D11DeviceContext* mDeviceContext;
	};
}