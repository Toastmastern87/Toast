
#include "tpch.h"
#include "Toast/Renderer/RendererBuffer.h"
#include "Toast/Renderer/RendererAPI.h"
#include "Toast/Renderer/Renderer.h"

#include "Toast/Core/Log.h"

#include "Toast/Core/Application.h"

#include <system_error>  // For std::system_category
#include <locale>        // For std::wstring_convert
#include <codecvt>       // For std::codecvt_utf8

namespace Toast {

	std::string HResultToString(HRESULT hr)
	{
		LPVOID messageBuffer = nullptr;
		size_t size = FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			hr,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPSTR)&messageBuffer,
			0,
			NULL);

		std::string message((LPSTR)messageBuffer, size);

		// Free the buffer allocated by FormatMessage
		LocalFree(messageBuffer);

		return message;
	}

	////////////////////////////////////////////////////////////////////////////////////////  
	//     VERTEXBUFFER     ////////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	VertexBuffer::VertexBuffer(uint32_t size, uint32_t count, uint32_t bindslot)
		: mSize(size), mCount(count), mBindSlot(bindslot)
	{
		TOAST_PROFILE_FUNCTION();

		D3D11_BUFFER_DESC vbd;
		HRESULT result;

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		ZeroMemory(&vbd, sizeof(D3D11_BUFFER_DESC));

		vbd.Usage = D3D11_USAGE_DYNAMIC;
		vbd.ByteWidth = size;
		vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		vbd.MiscFlags = 0;
		vbd.StructureByteStride = 0;

		result = device->CreateBuffer(&vbd, nullptr, &mVertexBuffer);

		if (FAILED(result))
			TOAST_CORE_ERROR("Error creating Vertexbuffer!");

		// TODO remove the bind here, creating a vertex buffer doesn't mean you want to bind it.
		Bind();
	}

	VertexBuffer::VertexBuffer(void* vertices, uint32_t size, uint32_t count, uint32_t bindslot, D3D11_USAGE usage)
		: mSize(size), mCount(count), mBindSlot(bindslot), mUsage(usage)
	{
		TOAST_PROFILE_FUNCTION();

		D3D11_BUFFER_DESC vbd;
		D3D11_SUBRESOURCE_DATA vd;
		HRESULT result;

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		ZeroMemory(&vbd, sizeof(D3D11_BUFFER_DESC));

		vbd.Usage = mUsage;
		vbd.ByteWidth = mSize;
		vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vbd.CPUAccessFlags = (mUsage == D3D11_USAGE_DEFAULT) ? 0 : D3D11_CPU_ACCESS_WRITE;
		vbd.MiscFlags = 0;
		vbd.StructureByteStride = 0;

		vd.pSysMem = vertices;
		vd.SysMemPitch = 0;
		vd.SysMemSlicePitch = 0;

		result = device->CreateBuffer(&vbd, &vd, &mVertexBuffer);
		
		if (FAILED(result))
		{
			std::string errorMessage = HResultToString(result);
			TOAST_CORE_ERROR("Error creating Vertexbuffer: %s", errorMessage.c_str());
		}

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

		deviceContext->IASetVertexBuffers(mBindSlot, 1, mVertexBuffer.GetAddressOf(), stride, offset);
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
		TOAST_PROFILE_FUNCTION();

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
		ibd.ByteWidth = sizeof(uint32_t) * mCount;
		ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		ibd.CPUAccessFlags = 0;
		ibd.MiscFlags = 0;
		ibd.StructureByteStride = 0;

		id.pSysMem = indices;
		id.SysMemPitch = 0;
		id.SysMemSlicePitch = 0;

		result = device->CreateBuffer(&ibd, &id, &mIndexBuffer);

		if (FAILED(result))
		{
			std::string errorMessage = HResultToString(result);
			TOAST_CORE_ERROR("Error creating Indexbuffer: %s", errorMessage.c_str());
		}

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

	////////////////////////////////////////////////////////////////////////////////////////  
	//     CONSTANTBUFFER     //////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	ConstantBuffer::ConstantBuffer(const std::string name, const uint32_t size, std::vector<CBufferBindInfo> bindInfo, D3D11_USAGE usage)
		: mName(name), mSize(size), mBindInfo(bindInfo)
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();
		HRESULT result;

		D3D11_BUFFER_DESC bufferDesc;
		bufferDesc.ByteWidth = size;
		bufferDesc.Usage = usage;
		bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bufferDesc.CPUAccessFlags = (usage == D3D11_USAGE_DYNAMIC) ? D3D11_CPU_ACCESS_WRITE : NULL;
		bufferDesc.MiscFlags = 0;
		bufferDesc.StructureByteStride = 0;

		result = device->CreateBuffer(&bufferDesc, nullptr, &mBuffer);

		if (FAILED(result))
		{
			std::string errorMessage = HResultToString(result);
			TOAST_CORE_ERROR("Error creating Constantbuffer[%s]: %s", name.c_str(), errorMessage.c_str());
		}
	}

	void ConstantBuffer::Bind() const
	{
		TOAST_PROFILE_FUNCTION();

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();
		//TOAST_CORE_CRITICAL("Binding buffer named: %s", mName.c_str());
		for(auto& bindInfo : mBindInfo)
		{
			//TOAST_CORE_CRITICAL("Binding buffer with bind point: %d", bindInfo.BindPoint);
			switch (bindInfo.ShaderType)
			{
			case D3D11_VERTEX_SHADER:
				deviceContext->VSSetConstantBuffers((UINT)bindInfo.BindPoint, 1, mBuffer.GetAddressOf());
				break;
			case D3D11_PIXEL_SHADER:
				deviceContext->PSSetConstantBuffers((UINT)bindInfo.BindPoint, 1, mBuffer.GetAddressOf());
				break;
			case D3D11_COMPUTE_SHADER:
				deviceContext->CSSetConstantBuffers((UINT)bindInfo.BindPoint, 1, mBuffer.GetAddressOf());
				break;
			}
		}
	}

	void ConstantBuffer::Map(Buffer& data)
	{
		TOAST_PROFILE_FUNCTION();

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();
		D3D11_MAPPED_SUBRESOURCE ms;
		deviceContext->Map(mBuffer.Get(), NULL, D3D11_MAP_WRITE_DISCARD, NULL, &ms);
		std::memcpy(ms.pData, data.Data, data.Size);
		deviceContext->Unmap(mBuffer.Get(), NULL);
	}

	////////////////////////////////////////////////////////////////////////////////////////  
	// CONSTANTBUFFERLIBRARY  //////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	std::unordered_map<std::string, Ref<ConstantBuffer>> ConstantBufferLibrary::mConstantBuffers;

	void ConstantBufferLibrary::Add(const std::string name, const Ref<ConstantBuffer>& buffer)
	{
		if (Exists(name))
			return;

		mConstantBuffers[name] = buffer;
	}

	void ConstantBufferLibrary::Add(const Ref<ConstantBuffer>& buffer)

	{
		auto& name = buffer->GetName();
		Add(name, buffer);
	}

	Ref<ConstantBuffer> ConstantBufferLibrary::Load(const std::string& name, const uint32_t size, std::vector<CBufferBindInfo> bindInfo)
	{
		if (Exists(name))
			return mConstantBuffers[name];

		auto buffer = CreateRef<ConstantBuffer>(name, size, bindInfo);
		Add(name, buffer);
		return buffer;
	}

	Ref<ConstantBuffer> ConstantBufferLibrary::Get(const std::string& name)
	{
		TOAST_CORE_ASSERT(Exists(name), "Buffer not found!");
		return mConstantBuffers[name];
	}

	std::vector<std::string> ConstantBufferLibrary::GetBufferList()
	{
		std::vector<std::string> bufferList;

		for (std::pair<std::string, Ref<ConstantBuffer>> buffer : mConstantBuffers)
		{
			bufferList.push_back(buffer.first);
		}

		return bufferList;
	}

	bool ConstantBufferLibrary::Exists(const std::string& name)
	{
		return mConstantBuffers.find(name) != mConstantBuffers.end();
	}
}