#include "tpch.h"
#include "DirectXBuffer.h"

#include "Toast/Application.h"

namespace Toast {

	////////////////////////////////////////////////////////////////////////////////////////  
	//     VERTEXBUFFER     ////////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	DirectXVertexBuffer::DirectXVertexBuffer(float* vertices, uint32_t size)
	{
		Application& app = Application::Get();
		ID3D11Device* device;
		D3D11_BUFFER_DESC vbd;
		D3D11_SUBRESOURCE_DATA vd;
		HRESULT result;
		
		device = app.GetWindow().GetGraphicsContext()->GetD3D11Device();

		ZeroMemory(&vbd, sizeof(D3D11_BUFFER_DESC));

		vbd.Usage = D3D11_USAGE_DEFAULT;
		vbd.ByteWidth = sizeof(float) * size;
		vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vbd.CPUAccessFlags = 0;
		vbd.MiscFlags = 0;
		vbd.StructureByteStride = 0;

		vd.pSysMem = vertices;
		vd.SysMemPitch = 0;
		vd.SysMemSlicePitch = 0;

		result = device->CreateBuffer(&vbd, &vd, &mVertexBuffer);

		if (FAILED(result))
			TOAST_CORE_ERROR("Error creating Vertexbuffer!");

		Bind();
	}

	DirectXVertexBuffer::~DirectXVertexBuffer()
	{
		if (mVertexBuffer) 
		{
			mVertexBuffer->Release();
			mVertexBuffer = nullptr;
		}
	}

	void DirectXVertexBuffer::Bind() const
	{
		Application& app = Application::Get();
		ID3D11DeviceContext* deviceContext;

		deviceContext = app.GetWindow().GetGraphicsContext()->GetD3D11DeviceContext();

		uint32_t stride[] = { sizeof(float) * 7 };
		uint32_t offset[] = { 0 };

		deviceContext->IASetVertexBuffers(0, 1, &mVertexBuffer, stride, offset);
	}

	void DirectXVertexBuffer::Unbind() const
	{
		Application& app = Application::Get();
		ID3D11DeviceContext* deviceContext;

		deviceContext = app.GetWindow().GetGraphicsContext()->GetD3D11DeviceContext();

		deviceContext->IASetVertexBuffers(0, 1, NULL, 0, 0);
	}

	////////////////////////////////////////////////////////////////////////////////////////  
	//     INDEXBUFFER     /////////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	DirectXIndexBuffer::DirectXIndexBuffer(uint32_t* indices, uint32_t count)
		: mCount(count)
	{
		Application& app = Application::Get();
		ID3D11Device* device;
		D3D11_BUFFER_DESC ibd;
		D3D11_SUBRESOURCE_DATA id;
		HRESULT result;

		device = app.GetWindow().GetGraphicsContext()->GetD3D11Device();

		ZeroMemory(&ibd, sizeof(D3D11_BUFFER_DESC));

		ibd.Usage = D3D11_USAGE_DEFAULT;
		ibd.ByteWidth = sizeof(uint32_t) * count;
		ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		ibd.CPUAccessFlags = 0;
		ibd.MiscFlags = 0;
		ibd.StructureByteStride = 0;

		id.pSysMem = indices;
		id.SysMemPitch = 0;
		id.SysMemSlicePitch = 0;

		result = device->CreateBuffer(&ibd, &id, &mIndexBuffer);

		if (FAILED(result))
			TOAST_CORE_ERROR("Error creating Indexbuffer!");

		Bind();
	}

	DirectXIndexBuffer::~DirectXIndexBuffer()
	{
		if (mIndexBuffer)
		{
			mIndexBuffer->Release();
			mIndexBuffer = nullptr;
		}
	}

	void DirectXIndexBuffer::Bind() const
	{
		Application& app = Application::Get();
		ID3D11DeviceContext* deviceContext;

		deviceContext = app.GetWindow().GetGraphicsContext()->GetD3D11DeviceContext();

		deviceContext->IASetIndexBuffer(mIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	}

	void DirectXIndexBuffer::Unbind() const
	{
		Application& app = Application::Get();
		ID3D11DeviceContext* deviceContext;

		deviceContext = app.GetWindow().GetGraphicsContext()->GetD3D11DeviceContext();

		deviceContext->IASetIndexBuffer(NULL, DXGI_FORMAT_R32_UINT, 0);
	}
}