#pragma once

#include <d3d11.h>
#include <d3d11shader.h>
#include <wrl.h>
#include <d3d11shadertracing.h>
#include <DirectXMath.h>

#include "Toast/Renderer/Shader.h"

namespace Toast {

	class DirectXShader : public Shader
	{
	private:
		struct ConstantBuffer
		{
			Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer;
			size_t Size;
			uint32_t BindPoint;
			D3D11_SHADER_TYPE ShaderType;
		};
	public:
		DirectXShader(const std::string& filepath);
		virtual ~DirectXShader() override;

		virtual void Bind() const override;
		virtual void Unbind() const override;

		virtual const std::string GetName() const override { return mName; }

		virtual void SetData(const std::string& cbName, void* data) override;

		ID3D10Blob* GetVSRaw() const { return mRawBlobs.at(D3D11_VERTEX_SHADER); }
	private:
		std::string ReadFile(const std::string& filepath);
		std::unordered_map<D3D11_SHADER_TYPE, std::string> PreProcess(const std::string& source);
		void Compile(const std::unordered_map<D3D11_SHADER_TYPE, std::string> shaderSources);

		void ProcessConstantBuffers();
	private:
		std::unordered_map<std::string, ConstantBuffer> mConstantBuffers;
	private:
		std::string mName;

		Microsoft::WRL::ComPtr<ID3D11VertexShader> mVertexShader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> mPixelShader;
		std::unordered_map<D3D11_SHADER_TYPE, ID3D10Blob*> mRawBlobs;
	};
}