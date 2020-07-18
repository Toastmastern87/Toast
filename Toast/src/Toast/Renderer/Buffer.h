#pragma once

namespace Toast {
	
	enum class ShaderDataType 
	{
		None = 0, Float, Float2, Float3, Float4, Int, Int2, Int3, Int4
	};

	static uint32_t ShaderDataTypeSize(ShaderDataType type)
	{
		switch (type) 
		{
			case ShaderDataType::Float:			return 4;
			case ShaderDataType::Float2:		return 4 * 2;
			case ShaderDataType::Float3:		return 4 * 3;
			case ShaderDataType::Float4:		return 4 * 4;
			case ShaderDataType::Int:			return 4;
			case ShaderDataType::Int2:			return 4 * 2;
			case ShaderDataType::Int3:			return 4 * 3;
			case ShaderDataType::Int4:			return 4 * 4;
		}

		TOAST_CORE_ASSERT(false, "Unkown ShaderDataType!");
		return 0;
	}

	struct BufferElement 
	{
		std::string mName;
		ShaderDataType mType;
		uint32_t mSize;
		uint32_t mOffset;
		uint32_t mSemanticIndex;

		BufferElement() {}

		BufferElement(ShaderDataType type, const std::string& name)
			: mName(name), mType(type), mSize(ShaderDataTypeSize(type)), mOffset(0), mSemanticIndex(0)
		{
		}

		uint32_t GetComponentCount() const
		{
			switch (mType) 
			{
				case ShaderDataType::Float:			return 1;
				case ShaderDataType::Float2:		return 2;
				case ShaderDataType::Float3:		return 3;
				case ShaderDataType::Float4:		return 4;
				case ShaderDataType::Int:			return 1;
				case ShaderDataType::Int2:			return 2;
				case ShaderDataType::Int3:			return 3;
				case ShaderDataType::Int4:			return 4;
			}

			TOAST_CORE_ASSERT(false, "Unkown ShaderDataType!");
			return 0;
		}
	};

	class BufferLayout
	{
	public:
		BufferLayout() {}

		BufferLayout(const std::initializer_list<BufferElement> & elements)
			: mElements(elements)
		{
			CalculateOffsetAndStride();
			CalculateSemanticIndex();
		}

		inline uint32_t GetStride() const { return mStride; }
		inline const std::vector<BufferElement>& GetElements() const { return mElements; }

		std::vector<BufferElement>::iterator begin() { return mElements.begin(); }
		std::vector<BufferElement>::iterator end() { return mElements.end(); }
		std::vector<BufferElement>::const_iterator begin() const { return mElements.begin(); }
		std::vector<BufferElement>::const_iterator end() const { return mElements.end(); }

	private:
		void CalculateOffsetAndStride()
		{
			uint32_t offset = 0;
			mStride = 0;

			for(auto& element : mElements)
			{
				element.mOffset = offset;
				offset += element.mSize;
				mStride += element.mSize;
			}
		}

		void CalculateSemanticIndex()
		{

		}

	private:
		uint32_t mStride;
		std::vector<BufferElement> mElements;
	};

	class VertexBuffer 
	{
	public:
		virtual ~VertexBuffer() = default;

		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		virtual const BufferLayout& GetLayout() const = 0;
		virtual void SetLayout(const BufferLayout& layout) = 0;

		static VertexBuffer* Create(float* vertices, uint32_t size);
	};

	class IndexBuffer 
	{
	public:
		virtual ~IndexBuffer() = default;

		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		virtual uint32_t GetCount() const = 0;

		static IndexBuffer* Create(uint32_t* indices, uint32_t count);
	};
}