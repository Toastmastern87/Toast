#pragma once

#include "Shader.h"

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

	class BufferLayout
	{
	public:
		struct BufferElement
		{
			std::string mName;
			ShaderDataType mType;
			uint32_t mSize;
			uint32_t mOffset;
			uint32_t mSemanticIndex;

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

	public:
		~BufferLayout() = default;

		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		virtual uint32_t GetStride() const = 0;
		virtual const std::vector<BufferElement>& GetElements() const = 0;

		static BufferLayout* Create(const std::initializer_list<BufferElement>& elements, std::shared_ptr<Shader> shader);

	private:
		virtual void CalculateOffsetAndStride() = 0;
	};

	class VertexBuffer 
	{
	public:
		virtual ~VertexBuffer() = default;

		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		static VertexBuffer* Create(float* vertices, uint32_t size, uint32_t count);
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