#include "tpch.h"
#include "Shader.h"

#include <d3dcompiler.h>

#include "Toast/Application.h"

namespace Toast {

	Shader::Shader(const std::string& vertexSrc, const std::string& pixelSrc)
	{
		HRESULT result;
		ID3D10Blob* VSRaw = nullptr;
		ID3D10Blob* PSRaw = nullptr;
		ID3D10Blob* errorRaw = nullptr;
		std::wstring stemp;

		Application& app = Application::Get();
		ID3D11Device* device = app.GetWindow().GetGraphicsContext()->GetD3D11Device();

		stemp = std::wstring(vertexSrc.begin(), vertexSrc.end());

		result = D3DCompileFromFile(stemp.c_str(),
									NULL,
									NULL,
									"main",
									"vs_5_0", 
									D3D10_SHADER_ENABLE_STRICTNESS,
									0,
									&VSRaw,
									&errorRaw);

		if (FAILED(result))	{
			if (errorRaw) {
				char* errorText = (char*)errorRaw->GetBufferPointer();

				errorText[strlen(errorText) - 1] = '\0';

				TOAST_CORE_ERROR("{0}", errorText);
				TOAST_CORE_ASSERT(false, "Vertex shader compilation failure!");
			}
			else
				TOAST_CORE_ERROR("Shader file not found: {0}", (char*)stemp.c_str());

			return;
		}

		result = device->CreateVertexShader(VSRaw->GetBufferPointer(), VSRaw->GetBufferSize(), NULL, &mVertexShader);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create vertex shader: {0}", vertexSrc);

		VSRaw->Release();

		stemp = std::wstring(pixelSrc.begin(), pixelSrc.end());

		result = D3DCompileFromFile(stemp.c_str(),
									NULL,
									NULL,
									"main",
									"ps_5_0",
									D3D10_SHADER_ENABLE_STRICTNESS,
									0,
									&PSRaw,
									&errorRaw);

		if (FAILED(result))
		{
			if (errorRaw) {
				char* errorText = (char*)errorRaw->GetBufferPointer();

				errorText[strlen(errorText) - 1] = '\0';

				TOAST_CORE_ERROR("{0}", errorText);
				TOAST_CORE_ASSERT(false, "Pixel shader compilation failure!")
			}
			else
				TOAST_CORE_ERROR("Shader file not found: {0}", (char*)stemp.c_str());

			return;
		}

		result = device->CreatePixelShader(PSRaw->GetBufferPointer(), PSRaw->GetBufferSize(), NULL, &mPixelShader);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create pixel shader: {0}", pixelSrc);

		PSRaw->Release();
	}

	Shader::~Shader()
	{
		if (mVertexShader)
			mVertexShader = nullptr;

		if (mPixelShader)
			mPixelShader = nullptr;
	}

	void Shader::Bind() const
	{
		Application& app = Application::Get();
		ID3D11DeviceContext* deviceContext = app.GetWindow().GetGraphicsContext()->GetD3D11DeviceContext();

		deviceContext->VSSetShader(mVertexShader, 0, 0);
		deviceContext->PSSetShader(mPixelShader, 0, 0);
	}

	void Shader::Unbind() const
	{
		Application& app = Application::Get();
		ID3D11DeviceContext* deviceContext = app.GetWindow().GetGraphicsContext()->GetD3D11DeviceContext();

		ID3D11VertexShader* nullVertexShader = nullptr;
		ID3D11PixelShader* nullPixelShader = nullptr;

		deviceContext->VSSetShader(nullVertexShader, 0, 0);
		deviceContext->PSSetShader(nullPixelShader, 0, 0);
	}
}