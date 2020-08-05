#pragma once

#include <d3d11.h>
#include <d3d11shadertracing.h>
#include <DirectXMath.h>

#include "Toast/Renderer/Shader.h"

namespace Toast {

	class DirectXShader : public Shader
	{
	public:
		DirectXShader(const std::string& filepath);
		virtual ~DirectXShader() override;

		virtual void Bind() const override;
		virtual void Unbind() const override;
		virtual void UploadColorDataPSCBuffer(const DirectX::XMFLOAT4& values);
		virtual void UploadObjectDataVSCBuffer(const DirectX::XMMATRIX& matrix);
		virtual void UploadSceneDataVSCBuffer(const DirectX::XMMATRIX& matrix);

		ID3D10Blob* GetVSRaw() const { return mRawBlobs.at(D3D11_VERTEX_SHADER); }
	private:
		std::string ReadFile(const std::string& filepath);
		std::unordered_map<D3D11_SHADER_TYPE, std::string> PreProcess(const std::string& source);
		void Compile(const std::unordered_map<D3D11_SHADER_TYPE, std::string> shaderSources);

	private:
		ID3D11VertexShader* mVertexShader = nullptr;
		ID3D11PixelShader* mPixelShader = nullptr;
		std::unordered_map<D3D11_SHADER_TYPE, ID3D10Blob*> mRawBlobs;
		ID3D11Buffer* mSceneCB = nullptr;
		ID3D11Buffer* mObjectCB = nullptr;
		ID3D11Buffer* mColorCB = nullptr;

		ID3D11Device* mDevice = nullptr;
		ID3D11DeviceContext* mDeviceContext = nullptr;
	};
}