#include "tpch.h"
#include "DirectXShader.h"

#include "DirectXRendererAPI.h"
#include "Toast/Renderer/Renderer.h"

#include "Toast/Application.h"

#include <d3dcompiler.h>
#include <d3d11shader.h>

namespace Toast {

	DirectXShader::DirectXShader(const std::string& vertexSrc, const std::string& pixelSrc)
	{
		HRESULT result;
		ID3D10Blob* errorRaw = nullptr;
		std::wstring stemp;

		DirectXRendererAPI API = static_cast<DirectXRendererAPI&>(*RenderCommand::sRendererAPI);
		mDevice = API.GetDevice();
		mDeviceContext = API.GetDeviceContext();

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
			&mPSRaw,
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

		result = mDevice->CreatePixelShader(mPSRaw->GetBufferPointer(), mPSRaw->GetBufferSize(), NULL, &mPixelShader);
		TOAST_CORE_ASSERT(SUCCEEDED(result), "Failed to create pixel shader: {0}", pixelSrc);
	}

	DirectXShader::~DirectXShader()
	{
		CLEAN(mVertexShader);
		CLEAN(mVSRaw);
		CLEAN(mPixelShader);
		CLEAN(mPSRaw);
		CLEAN(mColorCB);
		CLEAN(mObjectCB);
		CLEAN(mSceneCB);
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

	void DirectXShader::UploadColorDataPSCBuffer(const DirectX::XMFLOAT4& values)
	{
		if (!mColorCB)
		{
			D3D11_BUFFER_DESC cbDesc;
			cbDesc.ByteWidth = sizeof(DirectX::XMFLOAT4);
			cbDesc.Usage = D3D11_USAGE_DYNAMIC;
			cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			cbDesc.MiscFlags = 0;
			cbDesc.StructureByteStride = 0;

			D3D11_SUBRESOURCE_DATA InitData;
			InitData.pSysMem = &values;
			InitData.SysMemPitch = 0;
			InitData.SysMemSlicePitch = 0;

			mDevice->CreateBuffer(&cbDesc, &InitData, &mColorCB);
		}

		D3D11_MAPPED_SUBRESOURCE ms;
		mDeviceContext->Map(mColorCB, NULL, D3D11_MAP_WRITE_DISCARD, NULL, &ms);
		memcpy(ms.pData, &values, sizeof(DirectX::XMFLOAT4));
		mDeviceContext->Unmap(mColorCB, NULL);

		mDeviceContext->PSSetConstantBuffers(0, 1, &mColorCB);
	}

	void DirectXShader::UploadObjectDataVSCBuffer(const DirectX::XMMATRIX& matrix)
	{
		if (!mObjectCB)
		{
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

			mDevice->CreateBuffer(&cbDesc, &InitData, &mObjectCB);
		}

		D3D11_MAPPED_SUBRESOURCE ms;
		mDeviceContext->Map(mObjectCB, NULL, D3D11_MAP_WRITE_DISCARD, NULL, &ms);
		memcpy(ms.pData, &matrix, sizeof(DirectX::XMMATRIX));
		mDeviceContext->Unmap(mObjectCB, NULL);

		mDeviceContext->VSSetConstantBuffers(1, 1, &mObjectCB);
	}

	void DirectXShader::UploadSceneDataVSCBuffer(const DirectX::XMMATRIX& matrix)
	{
		if (!mSceneCB) 
		{
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

			mDevice->CreateBuffer(&cbDesc, &InitData, &mSceneCB);
		}

		D3D11_MAPPED_SUBRESOURCE ms;
		mDeviceContext->Map(mSceneCB, NULL, D3D11_MAP_WRITE_DISCARD, NULL, &ms);
		memcpy(ms.pData, &matrix, sizeof(DirectX::XMMATRIX));
		mDeviceContext->Unmap(mSceneCB, NULL);

		mDeviceContext->VSSetConstantBuffers(0, 1, &mSceneCB);
	}
}