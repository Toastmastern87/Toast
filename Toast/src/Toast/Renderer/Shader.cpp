#include "tpch.h"
#include "Toast/Renderer/Shader.h"
#include "Toast/Renderer/RendererAPI.h"
#include "Toast/Renderer/Renderer.h"

#include "Toast/Core/Application.h"

#include <fstream>

#include <d3dcompiler.h>
#include <d3d11shader.h>

namespace Toast {

	static D3D11_SHADER_TYPE ShaderTypeFromString(const std::string& type)
	{
		if (type == "vertex")
			return D3D11_VERTEX_SHADER;
		if (type == "pixel" || type == "fragment")
			return D3D11_PIXEL_SHADER;
		if (type == "compute")
			return D3D11_COMPUTE_SHADER;

		TOAST_CORE_ASSERT(false, "Unknown shader type!", type);

		return static_cast<D3D11_SHADER_TYPE>(0);
	}

	static std::string& ShaderVersionFromType(const D3D11_SHADER_TYPE type)
	{
		static std::string returnStr = "";
		static std::string vertexVersion = "vs_5_0";
		static std::string pixelVersion = "ps_5_0";
		static std::string computeVersion = "cs_5_0";

		if (type == D3D11_VERTEX_SHADER)
			return vertexVersion;
		if (type == D3D11_PIXEL_SHADER)
			return pixelVersion;
		if (type == D3D11_COMPUTE_SHADER)
			return computeVersion;

		TOAST_CORE_ASSERT(false, "Unknown shader type!", type);

		return returnStr;
	}

	Shader::Shader(const std::string& filepath)
	{
		TOAST_PROFILE_FUNCTION();

		HRESULT result;

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		Microsoft::WRL::ComPtr<ID3D11Device> device = API->GetDevice();

		// Finds the shader name
		auto lastSlash = filepath.find_last_of("/\\");
		lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;
		auto lastDot = filepath.rfind('.');
		auto count = lastDot == std::string::npos ? filepath.size() - lastSlash : lastDot - lastSlash;
		mName = filepath.substr(lastSlash, count);

		std::string source = ReadFile(filepath);
		auto shaderSources = PreProcess(source);
		Compile(shaderSources);
		if (mRawBlobs.find(D3D11_VERTEX_SHADER) != mRawBlobs.end())
			ProcessInputLayout(source);

		ProcessResources();

		for (auto& kv : mRawBlobs)
		{
			switch (kv.first) {
			case D3D11_VERTEX_SHADER:
				result = device->CreateVertexShader(kv.second->GetBufferPointer(), kv.second->GetBufferSize(), NULL, &mVertexShader);
				TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create vertex shader: {0}", filepath);
				break;
			case D3D11_PIXEL_SHADER:
				result = device->CreatePixelShader(kv.second->GetBufferPointer(), kv.second->GetBufferSize(), NULL, &mPixelShader);
				TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create pixel shader: {0}", filepath);
				break;
			case D3D11_COMPUTE_SHADER:
				result = device->CreateComputeShader(kv.second->GetBufferPointer(), kv.second->GetBufferSize(), NULL, &mComputeShader);
				TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create compute shader: {0}", filepath);
				break;
			}
		}
	}

	Shader::~Shader()
	{
		TOAST_PROFILE_FUNCTION();

		for (auto& kv : mRawBlobs)
		{
			CLEAN(kv.second);
		}

		mRawBlobs.clear();
	}

	std::string Shader::ReadFile(const std::string& filepath)
	{
		TOAST_PROFILE_FUNCTION();

		std::string result;

		std::ifstream in(filepath, std::ios::in | std::ios::binary);
		if (in)
		{
			in.seekg(0, std::ios::end);
			size_t size = in.tellg();
			if (size != -1)
			{
				result.resize(size);
				in.seekg(0, std::ios::beg);
				in.read(&result[0], size);
				in.close();
			}
			else
			{
				TOAST_CORE_ERROR("Could not read from file '{0}'", filepath);
			}
		}
		else
		{
			TOAST_CORE_ERROR("Could not open file '{0}'", filepath);
		}

		return result;
	}

	std::unordered_map<D3D11_SHADER_TYPE, std::string> Shader::PreProcess(const std::string& source)
	{
		TOAST_PROFILE_FUNCTION();

		std::unordered_map<D3D11_SHADER_TYPE, std::string> shaderSources;

		const char* typeToken = "#type";
		size_t typeTokenLength = strlen(typeToken);
		size_t pos = source.find(typeToken, 0); //Start of shader type declaration line

		while (pos != std::string::npos)
		{
			size_t eol = source.find_first_of("\r\n", pos); //End of shader type declaration line
			TOAST_CORE_ASSERT(eol != std::string::npos, "Syntax error");
			size_t being = pos + typeTokenLength + 1; //Start of shader type name(after "#type " keyword)
			std::string type = source.substr(being, eol - being);
			TOAST_CORE_ASSERT(ShaderTypeFromString(type), "Invalid shader type specified");

			size_t nextLinePos = source.find_first_not_of("\r\n", eol); //Start of shader code after shader type declaration line
			TOAST_CORE_ASSERT(nextLinePos != std::string::npos, "Syntax error");
			pos = source.find(typeToken, nextLinePos); //Start of next shader type declaration line

			shaderSources[ShaderTypeFromString(type)] = (pos == std::string::npos) ? source.substr(nextLinePos) : source.substr(nextLinePos, pos - nextLinePos);
		}

		return shaderSources;
	}

	void Shader::Compile(const std::unordered_map<D3D11_SHADER_TYPE, std::string> shaderSources)
	{
		TOAST_PROFILE_FUNCTION();

		HRESULT result;
		Microsoft::WRL::ComPtr<ID3D10Blob> errorRaw = nullptr;

		for (auto& kv : shaderSources)
		{
			D3D11_SHADER_TYPE type = kv.first;
			const std::string& source = kv.second;

			result = D3DCompile(source.c_str(),
				source.size(),
				NULL,
				NULL,
				NULL,
				"main",
				ShaderVersionFromType(type).c_str(),
				D3D10_SHADER_ENABLE_STRICTNESS,
				0,
				&mRawBlobs[type],
				&errorRaw);

			if (FAILED(result))
			{
				char* errorText = (char*)errorRaw->GetBufferPointer();

				errorText[strlen(errorText) - 1] = '\0';

				TOAST_CORE_ERROR("{0}", errorText);
				TOAST_CORE_ASSERT(false, "Shader compilation failure!")

					return;
			}
		}
	}

	void Shader::ProcessInputLayout(const std::string& source)
	{
		std::vector<BufferLayout::BufferElement> inputLayoutDesc;
		Microsoft::WRL::ComPtr<ID3D11ShaderReflection> reflector;
		D3D11_SHADER_DESC shaderDesc;

		D3DReflect(mRawBlobs.at(D3D11_VERTEX_SHADER)->GetBufferPointer(), mRawBlobs.at(D3D11_VERTEX_SHADER)->GetBufferSize(), IID_ID3D11ShaderReflection, (void**)&reflector);

		reflector->GetDesc(&shaderDesc);

		const char* typeToken = "#inputlayout";
		size_t pos = source.find(typeToken, 0); //Start of input layout meta data
		size_t typeTokenLength = strlen(typeToken);

		size_t eol = source.find_first_of("\r\n", pos); //End of type input layout declaration line
		std::string inputType = source.substr(pos, eol - pos);

		for (uint32_t i = 0; i < shaderDesc.InputParameters; i++)
		{
			D3D11_SIGNATURE_PARAMETER_DESC paramDesc;
			reflector->GetInputParameterDesc(i, &paramDesc);

			BufferLayout::BufferElement elementDesc;
			elementDesc.mName = paramDesc.SemanticName;
			elementDesc.mSemanticIndex = paramDesc.SemanticIndex;

			pos = eol + 2;
			eol = source.find_first_of("\r\n", pos);
			std::string inputType = source.substr(pos, eol - pos);

			if (inputType == "vertex")
				elementDesc.mInputClassification = D3D11_INPUT_PER_VERTEX_DATA;
			else if (inputType == "instance")
				elementDesc.mInputClassification = D3D11_INPUT_PER_INSTANCE_DATA; 

			if (paramDesc.Mask == 1)
			{
				if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32) elementDesc.mType = DXGI_FORMAT_R32_UINT;
				else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32) elementDesc.mType = DXGI_FORMAT_R32_SINT;
				else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) elementDesc.mType = DXGI_FORMAT_R32_FLOAT;
			}
			else if (paramDesc.Mask <= 3)
			{
				if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32) elementDesc.mType = DXGI_FORMAT_R32G32_UINT;
				else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32) elementDesc.mType = DXGI_FORMAT_R32G32_SINT;
				else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) elementDesc.mType = DXGI_FORMAT_R32G32_FLOAT;
			}
			else if (paramDesc.Mask <= 7)
			{
				if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32) elementDesc.mType = DXGI_FORMAT_R32G32B32_UINT;
				else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32) elementDesc.mType = DXGI_FORMAT_R32G32B32_SINT;
				else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) elementDesc.mType = DXGI_FORMAT_R32G32B32_FLOAT;
			}
			else if (paramDesc.Mask <= 15)
			{
				if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32) elementDesc.mType = DXGI_FORMAT_R32G32B32A32_UINT;
				else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32) elementDesc.mType = DXGI_FORMAT_R32G32B32A32_SINT;
				else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) elementDesc.mType = DXGI_FORMAT_R32G32B32A32_FLOAT;
			}

			inputLayoutDesc.push_back(elementDesc);

			pos = source.find_first_of("\r\n", pos) + 1;
		}

		mLayout = CreateRef<BufferLayout>(inputLayoutDesc, mRawBlobs.at(D3D11_VERTEX_SHADER));
	}

	void Shader::ProcessResources()
	{
		for (auto& kv : mRawBlobs)
		{
			Microsoft::WRL::ComPtr<ID3D11ShaderReflection> reflector;
			D3D11_SHADER_DESC shaderDesc;

			D3DReflect(kv.second->GetBufferPointer(), kv.second->GetBufferSize(), IID_ID3D11ShaderReflection, (void**)&reflector);

			reflector->GetDesc(&shaderDesc);
			 
			for (uint32_t i = 0; i < shaderDesc.BoundResources; i++)
			{
				D3D11_SHADER_INPUT_BIND_DESC resourceDesc;

				reflector->GetResourceBindingDesc(i, &resourceDesc);

				switch (resourceDesc.Type)
				{
					case D3D_SIT_TEXTURE:
						mResourceBindings.push_back(ResourceBindingDesc{ resourceDesc.Name, kv.first, resourceDesc.BindPoint, BindingType::Texture, 0, 0 });
						break;
					case D3D_SIT_SAMPLER:
						mResourceBindings.push_back(ResourceBindingDesc{ resourceDesc.Name, kv.first, resourceDesc.BindPoint, BindingType::Sampler, 0, 0 });
						break;
					case D3D_SIT_CBUFFER:
						ID3D11ShaderReflectionConstantBuffer* cbReflection;
						D3D11_SHADER_BUFFER_DESC cbDesc;

						cbReflection = reflector->GetConstantBufferByName(resourceDesc.Name);
						cbReflection->GetDesc(&cbDesc);

						mResourceBindings.push_back(ResourceBindingDesc{ resourceDesc.Name, kv.first, resourceDesc.BindPoint, BindingType::Buffer, cbDesc.Size, 0 });

						break;
				}
			}
		}
	}

	void Shader::Bind() const
	{
		TOAST_PROFILE_FUNCTION();

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		if (mRawBlobs.find(D3D11_VERTEX_SHADER) != mRawBlobs.end())
			mLayout->Bind();

		for (auto& kv : mRawBlobs)
		{
			switch (kv.first)
			{
			case D3D11_VERTEX_SHADER:
				deviceContext->VSSetShader(mVertexShader.Get(), nullptr, 0);
				break;
			case D3D11_PIXEL_SHADER:
				deviceContext->PSSetShader(mPixelShader.Get(), nullptr, 0);
				break;
			case D3D11_COMPUTE_SHADER:
				deviceContext->CSSetShader(mComputeShader.Get(), nullptr, 0);
				break;
			}
		}
	}

	void Shader::Unbind() const
	{
		TOAST_PROFILE_FUNCTION();

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		for (auto& kv : mRawBlobs)
		{
			switch (kv.first)
			{
			case D3D11_VERTEX_SHADER:
				deviceContext->VSSetShader(nullptr, 0, 0);
				break;
			case D3D11_PIXEL_SHADER:
				deviceContext->PSSetShader(nullptr, 0, 0);
				break;
			case D3D11_COMPUTE_SHADER:
				deviceContext->CSSetShader(nullptr, 0, 0);
				break;
			}
		}
	}

	std::string Shader::GetResourceName(BindingType type, uint32_t bindSlot, D3D11_SHADER_TYPE shaderType) const
	{
		for (auto& resource : mResourceBindings)
		{
			if (resource.Type == type && resource.BindSlot == bindSlot && resource.Shader == shaderType) 
				return resource.Name;
		}	
	}

	std::unordered_map<std::string, Ref<Shader>> ShaderLibrary::mShaders;

	void ShaderLibrary::Add(const std::string name, const Ref<Shader>& shader)
	{
		if (Exists(name))
			return;

		mShaders[name] = shader;
	}

	void ShaderLibrary::Add(const Ref<Shader>& shader)
	{
		auto& name = shader->GetName();
		Add(name, shader);
	}

	Ref<Shader> ShaderLibrary::Load(const std::string& filepath) 
	{
		auto shader = CreateRef<Shader>(filepath);
		Add(shader);
		return shader;
	}

	Ref<Shader> ShaderLibrary::Load(const std::string& name, const std::string& filepath)
	{
		auto shader = CreateRef<Shader>(filepath);
		Add(name, shader);
		return shader;
	}

	Ref<Shader> ShaderLibrary::Get(const std::string& name)
	{
		TOAST_CORE_ASSERT(Exists(name), "Shader not found!");
		return mShaders[name];
	}

	std::vector<std::string> ShaderLibrary::GetShaderList()
	{
		std::vector<std::string> shaderList;

		for (std::pair<std::string, Ref<Shader>> shader : mShaders)
		{
			shaderList.push_back(shader.first);
		}

		return shaderList;
	}

	bool ShaderLibrary::Exists(const std::string& name)
	{
		return mShaders.find(name) != mShaders.end();
	}
}