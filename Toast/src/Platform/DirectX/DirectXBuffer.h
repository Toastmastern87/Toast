#pragma once

#include <d3d11.h>

#include "Toast/Renderer/Buffer.h"

namespace Toast {

	static DXGI_FORMAT ShaderDataTypeToDirectXBaseType(ShaderDataType type)
	{
		switch (type)
		{
		case Toast::ShaderDataType::Float:		return DXGI_FORMAT_R32_FLOAT;
		case Toast::ShaderDataType::Float2:		return DXGI_FORMAT_R32G32_FLOAT;
		case Toast::ShaderDataType::Float3:		return DXGI_FORMAT_R32G32B32_FLOAT;
		case Toast::ShaderDataType::Float4:		return DXGI_FORMAT_R32G32B32A32_FLOAT;
		case Toast::ShaderDataType::Int:		return DXGI_FORMAT_R32_UINT;
		case Toast::ShaderDataType::Int2:		return DXGI_FORMAT_R32G32_UINT;
		case Toast::ShaderDataType::Int3:		return DXGI_FORMAT_R32G32B32_UINT;
		case Toast::ShaderDataType::Int4:		return DXGI_FORMAT_R32G32B32A32_UINT;
		}

		TOAST_CORE_ASSERT(false, "Unkown ShaderDataType!");
		return DXGI_FORMAT_UNKNOWN;
	}

	class DirectXBufferLayout : public BufferLayout 
	{
	public:
		DirectXBufferLayout(const std::initializer_list<BufferElement>& elements, Ref<Shader> shader);
		virtual ~DirectXBufferLayout();

		virtual void Bind() const override;
		virtual void Unbind() const override;

		inline uint32_t GetStride() const { return mStride; }
		inline const std::vector<BufferElement>& GetElements() const { return mElements; }

		std::vector<BufferElement>::iterator begin() { return mElements.begin(); }
		std::vector<BufferElement>::iterator end() { return mElements.end(); }
		std::vector<BufferElement>::const_iterator begin() const { return mElements.begin(); }
		std::vector<BufferElement>::const_iterator end() const { return mElements.end(); }

	private:
		virtual void CalculateOffsetAndStride() override;
		void CalculateSemanticIndex();

	private:
		uint32_t mStride;
		std::vector<BufferElement> mElements;
		ID3D11InputLayout* mInputLayout;
		
		ID3D11Device* mDevice;
		ID3D11DeviceContext* mDeviceContext;
	};

	class DirectXVertexBuffer : public VertexBuffer 
	{
	public:
		DirectXVertexBuffer(float* vertices, uint32_t size, uint32_t count);
		virtual ~DirectXVertexBuffer();

		virtual void Bind() const override;
		virtual void Unbind() const override;

	private:
		ID3D11Buffer* mVertexBuffer = nullptr;
		uint32_t mSize, mCount;

		ID3D11Device* mDevice;
		ID3D11DeviceContext* mDeviceContext;
	}; 

	class DirectXIndexBuffer : public IndexBuffer
	{
	public:
		DirectXIndexBuffer(uint32_t* indices, uint32_t count);
		virtual ~DirectXIndexBuffer();

		virtual void Bind() const override;
		virtual void Unbind() const override;

		virtual uint32_t GetCount() const { return mCount; }
	private:
		ID3D11Buffer* mIndexBuffer = nullptr;
		uint32_t mCount;

		ID3D11Device* mDevice;
		ID3D11DeviceContext* mDeviceContext;
	};
}