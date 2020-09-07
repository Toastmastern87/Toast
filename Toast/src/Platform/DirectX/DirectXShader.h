#pragma once

#include <d3d11.h>
#include <wrl.h>
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

		virtual const std::string GetName() const override { return mName; }

		virtual void SetSceneData(const DirectX::XMMATRIX& matrix) override;
		virtual void SetObjectData(const DirectX::XMMATRIX& matrix) override;
		virtual void SetColorData(const DirectX::XMFLOAT4& values, float tilingFactor) override;

		virtual void UploadColorDataPSCBuffer(const DirectX::XMFLOAT4& values, float tilingFactor);
		virtual void UploadObjectDataVSCBuffer(const DirectX::XMMATRIX& matrix);
		virtual void UploadSceneDataVSCBuffer(const DirectX::XMMATRIX& matrix);

		ID3D10Blob* GetVSRaw() const { return mRawBlobs.at(D3D11_VERTEX_SHADER); }
	private:
		std::string ReadFile(const std::string& filepath);
		std::unordered_map<D3D11_SHADER_TYPE, std::string> PreProcess(const std::string& source);
		void Compile(const std::unordered_map<D3D11_SHADER_TYPE, std::string> shaderSources);

	private:
		std::string mName;

		Microsoft::WRL::ComPtr<ID3D11VertexShader> mVertexShader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> mPixelShader;
		std::unordered_map<D3D11_SHADER_TYPE, ID3D10Blob*> mRawBlobs;
		Microsoft::WRL::ComPtr<ID3D11Buffer> mSceneCB;
		Microsoft::WRL::ComPtr<ID3D11Buffer> mObjectCB;
		Microsoft::WRL::ComPtr<ID3D11Buffer> mColorCB;

		struct cbColorData
		{
			DirectX::XMFLOAT4	color;
			float				tilingFactor;
		};
	};
}