#pragma once

#include "Toast/Renderer/Buffer.h"

#include <d3d11.h>
#include <d3d11shader.h>
#include <d3d11shadertracing.h>
#include <wrl.h>
#include <DirectXMath.h>
#include <string>

namespace Toast {

	class Shader
	{
	public:
		enum class BindingType
		{
			Buffer,
			Texture,
			Sampler
		};

		struct ResourceBindingDesc
		{
			std::string Name			{""};
			D3D11_SHADER_TYPE Shader	{ D3D11_VERTEX_SHADER };
			uint32_t BindSlot			{ 0 };
			BindingType Type			{ BindingType::Buffer };
			uint32_t Size				{ 0 };
			uint32_t Count				{ 0 };
		};
	public:
		Shader(const std::string& filepath);
		~Shader();

		void Bind() const;
		void Unbind() const;

		const std::string GetName() const { return mName; }

		const ID3D10Blob* GetVSRaw() const { return mRawBlobs.at(D3D11_VERTEX_SHADER); }

		const std::vector<ResourceBindingDesc> GetResourceBindings() const { return mResourceBindings; }
		std::string GetResourceName(BindingType type, uint32_t bindSlot, D3D11_SHADER_TYPE shaderType) const;
	private:
		std::string ReadFile(const std::string& filepath);
		std::unordered_map<D3D11_SHADER_TYPE, std::string> PreProcess(const std::string& source);
		void Compile(const std::unordered_map<D3D11_SHADER_TYPE, std::string> shaderSources);

		void ProcessInputLayout(const std::string& source);
		void ProcessResources();
	private:
		Ref<BufferLayout> mLayout;

		std::string mName;

		Microsoft::WRL::ComPtr<ID3D11VertexShader> mVertexShader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> mPixelShader;
		Microsoft::WRL::ComPtr<ID3D11ComputeShader> mComputeShader;
		std::unordered_map<D3D11_SHADER_TYPE, ID3D10Blob*> mRawBlobs;

		std::vector<ResourceBindingDesc> mResourceBindings;
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