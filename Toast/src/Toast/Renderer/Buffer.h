#pragma once

#include <d3d11.h>
#include <wrl.h>

namespace Toast {

	static uint32_t ShaderDataTypeSize(DXGI_FORMAT type)
	{
		switch (type)
		{
		case DXGI_FORMAT_R32_FLOAT:					return 4;
		case DXGI_FORMAT_R32G32_FLOAT:				return 4 * 2;
		case DXGI_FORMAT_R32G32B32_FLOAT:			return 4 * 3;
		case DXGI_FORMAT_R32G32B32A32_FLOAT:		return 4 * 4;
		case DXGI_FORMAT_R32_UINT:					return 4;
		case DXGI_FORMAT_R32G32_UINT:				return 4 * 2;
		case DXGI_FORMAT_R32G32B32_UINT:			return 4 * 3;
		case DXGI_FORMAT_R32G32B32A32_UINT:			return 4 * 4;
		}

		return 0;
	}

	class BufferLayout
	{
	public:
		struct BufferElement
		{
			std::string mName;
			DXGI_FORMAT mType;
			uint32_t mSize;
			size_t mOffset;
			uint32_t mSemanticIndex;

			BufferElement() = default;

			BufferElement(DXGI_FORMAT type, const std::string& name, const uint32_t semanticIndex = 0)
				: mName(name), mType(type), mSize(ShaderDataTypeSize(type)), mOffset(0), mSemanticIndex(semanticIndex)
			{
			}

			uint32_t GetComponentCount() const
			{
				switch (mType)
				{
				case DXGI_FORMAT_R32_FLOAT:					return 1;
				case DXGI_FORMAT_R32G32_FLOAT:				return 2;
				case DXGI_FORMAT_R32G32B32_FLOAT:			return 3;
				case DXGI_FORMAT_R32G32B32A32_FLOAT:		return 4;
				case DXGI_FORMAT_R32_UINT:					return 1;
				case DXGI_FORMAT_R32G32_UINT:				return 2;
				case DXGI_FORMAT_R32G32B32_UINT:			return 3;
				case DXGI_FORMAT_R32G32B32A32_UINT:			return 4;
				}

				return 0;
			}
		};

		BufferLayout(const std::vector<BufferElement>& elements, void* VSRaw);
		virtual ~BufferLayout();

		virtual void Bind() const;
		virtual void Unbind() const;

		inline uint32_t GetStride() const { return mStride; }
		inline const std::vector<BufferElement>& GetElements() const { return mElements; }

		std::vector<BufferElement>::iterator begin() { return mElements.begin(); }
		std::vector<BufferElement>::iterator end() { return mElements.end(); }
		std::vector<BufferElement>::const_iterator begin() const { return mElements.begin(); }
		std::vector<BufferElement>::const_iterator end() const { return mElements.end(); }

	private:
		virtual void CalculateOffsetAndStride();
		void CalculateSemanticIndex();

	private:
		uint32_t mStride;
		std::vector<BufferElement> mElements;
		Microsoft::WRL::ComPtr<ID3D11InputLayout> mInputLayout;
	};

	class VertexBuffer
	{
	public:
		VertexBuffer(uint32_t size, uint32_t count);
		VertexBuffer(void* vertices, uint32_t size, uint32_t count);
		virtual ~VertexBuffer();

		virtual void Bind() const;
		virtual void Unbind() const;

		virtual void SetData(const void* data, uint32_t size);

	private:
		Microsoft::WRL::ComPtr<ID3D11Buffer> mVertexBuffer = nullptr;
		uint32_t mSize = 0, mCount;
	};

	class IndexBuffer
	{
	public:
		IndexBuffer(uint32_t* indices, uint32_t count);
		virtual ~IndexBuffer();

		virtual void Bind() const;
		virtual void Unbind() const;

		virtual uint32_t GetCount() const { return mCount; }
	private:
		Microsoft::WRL::ComPtr<ID3D11Buffer> mIndexBuffer = nullptr;
		uint32_t mCount;
	};
}