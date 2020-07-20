#include "tpch.h"
#include "DirectXBuffer.h"

#include "Toast/Application.h"

namespace Toast {

	////////////////////////////////////////////////////////////////////////////////////////  
	//     BUFFERLAYOUT  ///////////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	DirectXBufferLayout::DirectXBufferLayout(const std::initializer_list<BufferElement>& elements, std::shared_ptr<Shader> shader)
		: mElements(elements)
	{
		Application& app = Application::Get();
		mDevice = app.GetWindow().GetGraphicsContext()->GetD3D11Device();
		mDeviceContext = app.GetWindow().GetGraphicsContext()->GetD3D11DeviceContext();

		uint32_t index = 0;

		CalculateOffsetAndStride();
		CalculateSemanticIndex();

		D3D11_INPUT_ELEMENT_DESC* inputLayoutDesc = new D3D11_INPUT_ELEMENT_DESC[mElements.size()];

		for (const auto& element : mElements)
		{
			inputLayoutDesc[index].SemanticName = element.mName.c_str();
			inputLayoutDesc[index].SemanticIndex = element.mSemanticIndex;
			inputLayoutDesc[index].Format = ShaderDataTypeToDirectXBaseType(element.mType);
			inputLayoutDesc[index].InputSlot = 0;
			inputLayoutDesc[index].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
			inputLayoutDesc[index].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			inputLayoutDesc[index].InstanceDataStepRate = 0;

			index++;
		}

		ID3D10Blob* VSRaw = shader->GetVSRaw();

		mDevice->CreateInputLayout(inputLayoutDesc, 
								  2,
			                      VSRaw->GetBufferPointer(),
			                      VSRaw->GetBufferSize(),
			                      &mInputLayout);

		delete[] inputLayoutDesc;

		Bind();
	}

	DirectXBufferLayout::~DirectXBufferLayout()
	{
		mElements.clear();
		mElements.shrink_to_fit();

		if (mInputLayout)
		{
			mInputLayout->Release();
			mInputLayout = nullptr;
		}
	}

	void DirectXBufferLayout::Bind() const
	{
		mDeviceContext->IASetInputLayout(mInputLayout);
	}

	void DirectXBufferLayout::Unbind() const
	{
		mDeviceContext->IASetInputLayout(nullptr);
	}

	void DirectXBufferLayout::CalculateOffsetAndStride()
	{
		uint32_t offset = 0;
		mStride = 0;

		for (auto& element : mElements)
		{
			element.mOffset = offset;
			offset += element.mSize;
			mStride += element.mSize;
		}
	}

	void DirectXBufferLayout::CalculateSemanticIndex() 
	{
	}

	////////////////////////////////////////////////////////////////////////////////////////  
	//     VERTEXBUFFER     ////////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	DirectXVertexBuffer::DirectXVertexBuffer(float* vertices, uint32_t size, uint32_t count)
		: mSize(size), mCount(count)
	{
		D3D11_BUFFER_DESC vbd;
		D3D11_SUBRESOURCE_DATA vd;
		HRESULT result;

		Application& app = Application::Get();
		mDevice = app.GetWindow().GetGraphicsContext()->GetD3D11Device();
		mDeviceContext = app.GetWindow().GetGraphicsContext()->GetD3D11DeviceContext();

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

		result = mDevice->CreateBuffer(&vbd, &vd, &mVertexBuffer);

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
		uint32_t stride[] = { sizeof(float) * ((mSize / sizeof(float)) / mCount) };
		uint32_t offset[] = { 0 };

		mDeviceContext->IASetVertexBuffers(0, 1, &mVertexBuffer, stride, offset);
	}

	void DirectXVertexBuffer::Unbind() const
	{
		mDeviceContext->IASetVertexBuffers(0, 1, NULL, 0, 0);
	}

	////////////////////////////////////////////////////////////////////////////////////////  
	//     INDEXBUFFER     /////////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	DirectXIndexBuffer::DirectXIndexBuffer(uint32_t* indices, uint32_t count)
		: mCount(count)
	{
		D3D11_BUFFER_DESC ibd;
		D3D11_SUBRESOURCE_DATA id;
		HRESULT result;

		Application& app = Application::Get();
		mDevice = app.GetWindow().GetGraphicsContext()->GetD3D11Device();
		mDeviceContext = app.GetWindow().GetGraphicsContext()->GetD3D11DeviceContext();

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

		result = mDevice->CreateBuffer(&ibd, &id, &mIndexBuffer);

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
		mDeviceContext->IASetIndexBuffer(mIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	}

	void DirectXIndexBuffer::Unbind() const
	{
		mDeviceContext->IASetIndexBuffer(NULL, DXGI_FORMAT_R32_UINT, 0);
	}
}