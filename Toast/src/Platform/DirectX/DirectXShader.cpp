#include "tpch.h"
#include "DirectXShader.h"
#include "DirectXContext.h"

#include "Toast/Application.h"

#include <d3dcompiler.h>
#include <d3d11shader.h>

namespace Toast {

	DirectXShader::DirectXShader(const std::string& vertexSrc, const std::string& pixelSrc)
	{
		HRESULT result;
		ID3D10Blob* PSRaw = nullptr;
		ID3D10Blob* errorRaw = nullptr;
		std::wstring stemp;

		Application& app = Application::Get();
		mDevice = app.GetWindow().GetContext()->GetDevice();
		mDeviceContext = app.GetWindow().GetContext()->GetDeviceContext();

		stemp = std::wstring(vertexSrc.begin(), vertexSrc.end());

		result = D3DCompileFromFile(stemp.c_str(),
			NULL,
			NULL,
			"main",
			"vs_5_0",
			D3D10_SHADER_ENABLE_STRICTNESS,
			0,
			&mVSRaw,
			&errorRaw);

		if (FAILED(result)) {
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

		result = mDevice->CreateVertexShader(mVSRaw->GetBufferPointer(), mVSRaw->GetBufferSize(), NULL, &mVertexShader);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create vertex shader: {0}", vertexSrc);

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

		result = mDevice->CreatePixelShader(PSRaw->GetBufferPointer(), PSRaw->GetBufferSize(), NULL, &mPixelShader);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create pixel shader: {0}", pixelSrc);

		PSRaw->Release();
	}

	DirectXShader::~DirectXShader()
	{
		if (mVertexShader)
			mVertexShader = nullptr;

		if (mPixelShader)
			mPixelShader = nullptr;
	}

	void DirectXShader::Bind() const
	{
		mDeviceContext->VSSetShader(mVertexShader, 0, 0);
		mDeviceContext->PSSetShader(mPixelShader, 0, 0);
	}

	void DirectXShader::Unbind() const
	{
		ID3D11VertexShader* nullVertexShader = nullptr;
		ID3D11PixelShader* nullPixelShader = nullptr;

		mDeviceContext->VSSetShader(nullVertexShader, 0, 0);
		mDeviceContext->PSSetShader(nullPixelShader, 0, 0);
	}

	void DirectXShader::UploadConstantBuffer(const std::string& name, const DirectX::XMMATRIX& matrix) const
	{
		ID3D11ShaderReflection* reflector = nullptr;
		D3D11_SHADER_INPUT_BIND_DESC bindDesc;
		ID3D11Buffer* constantBuffer = nullptr;

		D3DReflect(mVSRaw->GetBufferPointer(), mVSRaw->GetBufferSize(), IID_ID3D11ShaderReflection, (void**)&reflector);

		reflector->GetResourceBindingDescByName(name.c_str(), &bindDesc);

		D3D11_BUFFER_DESC cbDesc;
		cbDesc.ByteWidth = sizeof(DirectX::XMMATRIX);
		cbDesc.Usage = D3D11_USAGE_DYNAMIC;
		cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		cbDesc.MiscFlags = 0;
		cbDesc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA InitData;
		InitData.pSysMem = &matrix;
		InitData.SysMemPitch = 0;
		InitData.SysMemSlicePitch = 0;

		mDevice->CreateBuffer(&cbDesc, &InitData, &constantBuffer);

		mDeviceContext->VSSetConstantBuffers(bindDesc.BindPoint, 1, &constantBuffer);
	}
}