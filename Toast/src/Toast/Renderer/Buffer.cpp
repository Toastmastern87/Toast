#include "tpch.h"
#include "Toast/Renderer/Buffer.h"
#include "Toast/Renderer/RendererAPI.h"
#include "Toast/Renderer/Renderer.h"

#include "Toast/Core/Application.h"

namespace Toast {

	////////////////////////////////////////////////////////////////////////////////////////  
	//     BUFFERLAYOUT  ///////////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	BufferLayout::BufferLayout(const std::vector<BufferElement>& elements, void* VSRaw)
		: mElements(elements)
	{
		TOAST_PROFILE_FUNCTION();

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		uint32_t index = 0;

		CalculateOffsetAndStride();
		CalculateSemanticIndex();

		D3D11_INPUT_ELEMENT_DESC* inputLayoutDesc = new D3D11_INPUT_ELEMENT_DESC[mElements.size()];

		for (const auto& element : mElements)
		{
			inputLayoutDesc[index].SemanticName = element.mName.c_str();
			inputLayoutDesc[index].SemanticIndex = element.mSemanticIndex;
			inputLayoutDesc[index].Format = element.mType;
			inputLayoutDesc[index].InputSlot = 0;
			inputLayoutDesc[index].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
			inputLayoutDesc[index].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			inputLayoutDesc[index].InstanceDataStepRate = 0;

			index++;
		}

		device->CreateInputLayout(inputLayoutDesc,
			(UINT)mElements.size(),
			static_cast<ID3D10Blob*>(VSRaw)->GetBufferPointer(),
			static_cast<ID3D10Blob*>(VSRaw)->GetBufferSize(),
			&mInputLayout);

		delete[] inputLayoutDesc;

		Bind();
	}

	BufferLayout::~BufferLayout()
	{
		TOAST_PROFILE_FUNCTION();

		mElements.clear();
		mElements.shrink_to_fit();
	}

	void BufferLayout::Bind() const
	{
		TOAST_PROFILE_FUNCTION();

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		if (mElements.size() == 0)
			deviceContext->IASetInputLayout(nullptr);
		else
			deviceContext->IASetInputLayout(mInputLayout.Get());
	}

	void BufferLayout::Unbind() const
	{
		TOAST_PROFILE_FUNCTION();

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		deviceContext->IASetInputLayout(nullptr);
	}

	void BufferLayout::CalculateOffsetAndStride()
	{
		size_t offset = 0;
		mStride = 0;

		for (auto& element : mElements)
		{
			element.mOffset = offset;
			offset += element.mSize;
			mStride += element.mSize;
		}
	}

	void BufferLayout::CalculateSemanticIndex()
	{
	}

	////////////////////////////////////////////////////////////////////////////////////////  
	//     VERTEXBUFFER     ////////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	VertexBuffer::VertexBuffer(uint32_t size, uint32_t count)
		: mSize(size), mCount(count)
	{
		TOAST_PROFILE_FUNCTION();

		D3D11_BUFFER_DESC vbd;
		HRESULT result;

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		ZeroMemory(&vbd, sizeof(D3D11_BUFFER_DESC));

		vbd.Usage = D3D11_USAGE_DYNAMIC;
		vbd.ByteWidth = sizeof(float) * size;
		vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		vbd.MiscFlags = 0;
		vbd.StructureByteStride = 0;

		result = device->CreateBuffer(&vbd, nullptr, &mVertexBuffer);

		if (FAILED(result))
			TOAST_CORE_ERROR("Error creating Vertexbuffer!");

		Bind();
	}

	VertexBuffer::VertexBuffer(void* vertices, uint32_t size, uint32_t count)
		: mSize(size), mCount(count)
	{
		TOAST_PROFILE_FUNCTION();

		D3D11_BUFFER_DESC vbd;
		D3D11_SUBRESOURCE_DATA vd;
		HRESULT result;

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		ZeroMemory(&vbd, sizeof(D3D11_BUFFER_DESC));

		vbd.Usage = D3D11_USAGE_DEFAULT;
		vbd.ByteWidth = size;
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

	VertexBuffer::~VertexBuffer()
	{
		TOAST_PROFILE_FUNCTION();
	}

	void VertexBuffer::Bind() const
	{
		TOAST_PROFILE_FUNCTION();

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		uint32_t stride[] = { mSize / mCount };
		uint32_t offset[] = { 0 };

		deviceContext->IASetVertexBuffers(0, 1, mVertexBuffer.GetAddressOf(), stride, offset);
	}

	void VertexBuffer::Unbind() const
	{
		TOAST_PROFILE_FUNCTION();

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		deviceContext->IASetVertexBuffers(0, 1, NULL, 0, 0);
	}

	void VertexBuffer::SetData(const void* data, uint32_t size)
	{
		D3D11_MAPPED_SUBRESOURCE ms;

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		deviceContext->Map(mVertexBuffer.Get(), NULL, D3D11_MAP_WRITE_DISCARD, NULL, &ms);
		memcpy(ms.pData, data, size);
		deviceContext->Unmap(mVertexBuffer.Get(), NULL);
	}

	////////////////////////////////////////////////////////////////////////////////////////  
	//     INDEXBUFFER     /////////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	IndexBuffer::IndexBuffer(uint32_t* indices, uint32_t count)
		: mCount(count)
	{
		TOAST_PROFILE_FUNCTION();

		D3D11_BUFFER_DESC ibd;
		D3D11_SUBRESOURCE_DATA id;
		HRESULT result;

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

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

	IndexBuffer::~IndexBuffer()
	{
		TOAST_PROFILE_FUNCTION();
	}

	void IndexBuffer::Bind() const
	{
		TOAST_PROFILE_FUNCTION();

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		deviceContext->IASetIndexBuffer(mIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	}

	void IndexBuffer::Unbind() const
	{
		TOAST_PROFILE_FUNCTION();

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		deviceContext->IASetIndexBuffer(NULL, DXGI_FORMAT_R32_UINT, 0);
	}
}