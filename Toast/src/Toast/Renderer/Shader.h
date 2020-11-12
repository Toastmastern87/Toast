#pragma once

#include "Toast/Renderer/Buffer.h"

#include <d3d11.h>
#include <d3d11shader.h>
#include <wrl.h>
#include <d3d11shadertracing.h>
#include <DirectXMath.h>

namespace Toast {

	class Shader
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
		Shader(const std::string& filepath);
		~Shader();

		void Bind() const;
		void Unbind() const;

		const std::string GetName() const { return mName; }

		void SetData(const std::string& cbName, void* data);

		const ID3D10Blob* GetVSRaw() const { return mRawBlobs.at(D3D11_VERTEX_SHADER); }
		const std::unordered_map<std::string, uint32_t> GetTextureResources() const { return mTextureResources; }
	private:
		std::string ReadFile(const std::string& filepath);
		std::unordered_map<D3D11_SHADER_TYPE, std::string> PreProcess(const std::string& source);
		void Compile(const std::unordered_map<D3D11_SHADER_TYPE, std::string> shaderSources);

		void ProcessConstantBuffers();
		void ProcessInputLayout();
		void ProcessTextureResources();
	private:
		Ref<BufferLayout> mLayout;
		std::unordered_map<std::string, ConstantBuffer> mConstantBuffers;

		std::string mName;

		Microsoft::WRL::ComPtr<ID3D11VertexShader> mVertexShader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> mPixelShader;
		std::unordered_map<D3D11_SHADER_TYPE, ID3D10Blob*> mRawBlobs;

		std::unordered_map<std::string, uint32_t> mTextureResources;
	};

	class ShaderLibrary 
	{
	public:
		static void Add(const std::string name, const Ref<Shader>& shader);
		static void Add(const Ref<Shader>& shader);
		static Ref<Shader> Load(const std::string& filepath);
		static Ref<Shader> Load(const std::string& name, const std::string& filepath);

		static Ref<Shader> Get(const std::string& name);
		static std::vector<std::string> GetShaderList();

		static bool Exists(const std::string& name);
	private:
		static std::unordered_map<std::string, Ref<Shader>> mShaders;
	};
}