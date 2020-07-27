#pragma once

#include <d3d11.h>
#include <DirectXMath.h>

#include "Toast/Renderer/Shader.h"

namespace Toast {

	class DirectXShader : public Shader
	{
	public:
		DirectXShader(const std::string& vertexSrc, const std::string& pixelSrc);
		virtual ~DirectXShader() override;

		virtual void Bind() const override;
		virtual void Unbind() const override;
		virtual void UploadColorDataPSCBuffer(const DirectX::XMFLOAT4& values);
		virtual void UploadObjectDataVSCBuffer(const DirectX::XMMATRIX& matrix);
		virtual void UploadSceneDataVSCBuffer(const DirectX::XMMATRIX& matrix);

		ID3D10Blob* GetVSRaw() const { return mVSRaw; }

	private:
		ID3D11VertexShader* mVertexShader = nullptr;
		ID3D11PixelShader* mPixelShader = nullptr;
		ID3D10Blob* mVSRaw = nullptr;
		ID3D10Blob* mPSRaw = nullptr;
		ID3D11Buffer* mSceneCB = nullptr;
		ID3D11Buffer* mObjectCB = nullptr;
		ID3D11Buffer* mColorCB = nullptr;

		ID3D11Device* mDevice = nullptr;
		ID3D11DeviceContext* mDeviceContext = nullptr;
	};
}