#include "tpch.h"
#include "Platform/DirectX/DirectXShader.h"

#include "Platform/DirectX/DirectXRendererAPI.h"
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
		if(type == "pixel" || type == "fragment")
			return D3D11_PIXEL_SHADER;

		TOAST_CORE_ASSERT(false, "Unknown shader type!", type);

		return static_cast<D3D11_SHADER_TYPE>(0);
	}

	static std::string& ShaderVersionFromType(const D3D11_SHADER_TYPE type)
	{
		static std::string returnStr = "";
		static std::string vertexVersion = "vs_5_0";
		static std::string pixelVersion = "ps_5_0";

		if (type == D3D11_VERTEX_SHADER)
			return vertexVersion;
		if (type == D3D11_PIXEL_SHADER)
			return pixelVersion;

		TOAST_CORE_ASSERT(false, "Unknown shader type!", type);

		return returnStr;
	}

	DirectXShader::DirectXShader(const std::string& filepath)
	{
		TOAST_PROFILE_FUNCTION();

		HRESULT result;

		DirectXRendererAPI* API = static_cast<DirectXRendererAPI*>(RenderCommand::sRendererAPI.get());
		Microsoft::WRL::ComPtr<ID3D11Device> device = API->GetDevice();

		std::string source = ReadFile(filepath);
		auto shaderSources = PreProcess(source);
		Compile(shaderSources);
		ProcessConstantBuffers();

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
			}
		}

		// Finds the shader name
		auto lastSlash = filepath.find_last_of("/\\");
		lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;
		auto lastDot = filepath.rfind('.');
		auto count = lastDot == std::string::npos ? filepath.size() - lastSlash : lastDot - lastSlash;
		mName = filepath.substr(lastSlash, count);
	}

	DirectXShader::~DirectXShader()
	{
		TOAST_PROFILE_FUNCTION();

		for (auto& kv : mRawBlobs)
		{
			CLEAN(kv.second);
		}

		mRawBlobs.clear();
	}

	std::string DirectXShader::ReadFile(const std::string& filepath)
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

	std::unordered_map<D3D11_SHADER_TYPE, std::string> DirectXShader::PreProcess(const std::string& source) 
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

	void DirectXShader::Compile(const std::unordered_map<D3D11_SHADER_TYPE, std::string> shaderSources) 
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

	void DirectXShader::ProcessConstantBuffers()
	{
		DirectXRendererAPI* API = static_cast<DirectXRendererAPI*>(RenderCommand::sRendererAPI.get());
		ID3D11Device* device = API->GetDevice();

		for (auto& kv : mRawBlobs)
		{
			Microsoft::WRL::ComPtr<ID3D11ShaderReflection> reflector;
			D3D11_SHADER_DESC shaderDesc;

			D3DReflect(kv.second->GetBufferPointer(), kv.second->GetBufferSize(), IID_ID3D11ShaderReflection, (void**)&reflector);

			reflector->GetDesc(&shaderDesc);

			for (uint32_t i = 0; i < shaderDesc.ConstantBuffers; i++) 
			{
				ID3D11ShaderReflectionConstantBuffer* cbReflection;
				D3D11_SHADER_BUFFER_DESC cbDesc;
				D3D11_SHADER_INPUT_BIND_DESC inputDesc;
				ConstantBuffer constantBuffer;

				cbReflection = reflector->GetConstantBufferByIndex(i);

				cbReflection->GetDesc(&cbDesc);
				reflector->GetResourceBindingDescByName(cbDesc.Name, &inputDesc);

				if (mConstantBuffers.find(cbDesc.Name) == mConstantBuffers.end())
				{
					D3D11_BUFFER_DESC bufferDesc;
					bufferDesc.ByteWidth = cbDesc.Size;
					bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
					bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
					bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
					bufferDesc.MiscFlags = 0;
					bufferDesc.StructureByteStride = 0;

					device->CreateBuffer(&bufferDesc, nullptr, &constantBuffer.Buffer);

					constantBuffer.ShaderType = kv.first;
					constantBuffer.Size = (size_t)cbDesc.Size;
					constantBuffer.BindPoint = inputDesc.BindPoint;

					mConstantBuffers[cbDesc.Name] = constantBuffer;
				}	
			}
		}
	}

	void DirectXShader::Bind() const
	{
		TOAST_PROFILE_FUNCTION();

		DirectXRendererAPI* API = static_cast<DirectXRendererAPI*>(RenderCommand::sRendererAPI.get());
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		for (auto& kv : mRawBlobs)
		{
			switch (kv.first) {
				case D3D11_VERTEX_SHADER:
					deviceContext->VSSetShader(mVertexShader.Get(), nullptr, 0);
					break;
				case D3D11_PIXEL_SHADER:
					deviceContext->PSSetShader(mPixelShader.Get(), nullptr, 0);
					break;
			}
		}
	}

	void DirectXShader::Unbind() const
	{
		TOAST_PROFILE_FUNCTION();

		DirectXRendererAPI* API = static_cast<DirectXRendererAPI*>(RenderCommand::sRendererAPI.get());
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		for (auto& kv : mRawBlobs)
		{
			switch (kv.first) {
			case D3D11_VERTEX_SHADER:
				deviceContext->VSSetShader(nullptr, 0, 0);
				break;
			case D3D11_PIXEL_SHADER:
				deviceContext->PSSetShader(nullptr, 0, 0);
				break;
			}
		}
	}

	void DirectXShader::SetData(const std::string& cbName, void* data)
	{
		DirectXRendererAPI* API = static_cast<DirectXRendererAPI*>(RenderCommand::sRendererAPI.get());
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		// Check to see if the constant buffer exists
		TOAST_CORE_ASSERT(!(mConstantBuffers.find(cbName) == mConstantBuffers.end()), "Constant Buffer doesn't exist");

		D3D11_MAPPED_SUBRESOURCE ms;
		deviceContext->Map(mConstantBuffers[cbName].Buffer.Get(), NULL, D3D11_MAP_WRITE_DISCARD, NULL, &ms);
		memcpy(ms.pData, data, mConstantBuffers[cbName].Size);
		deviceContext->Unmap(mConstantBuffers[cbName].Buffer.Get(), NULL);

		switch (mConstantBuffers[cbName].ShaderType) {
		case D3D11_VERTEX_SHADER:
			deviceContext->VSSetConstantBuffers(mConstantBuffers[cbName].BindPoint, 1, mConstantBuffers[cbName].Buffer.GetAddressOf());
			break;
		case D3D11_PIXEL_SHADER:
			deviceContext->PSSetConstantBuffers(mConstantBuffers[cbName].BindPoint, 1, mConstantBuffers[cbName].Buffer.GetAddressOf());
			break;
		}
	}
}