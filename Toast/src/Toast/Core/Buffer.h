#pragma once

#include "Toast/Core/Base.h"
#include "Toast/Core/Log.h"

namespace Toast 
{
	struct Buffer 
	{
		void* Data;
		uint32_t Size;

		Buffer() : Data(nullptr), Size(0) 
		{
		}

		Buffer(void* data, uint32_t size) 
			: Data(data), Size(size)
		{
		}

		void Allocate(uint32_t size) 
		{
			delete[] Data;
			Data = nullptr;
			
			if (size == 0)
				return;

			Data = new byte[size];
			Size = size;
		}

		void ZeroInitialize()
		{
			if (Data)
				memset(Data, 0, Size);
		}

		void Release() 
		{
			delete[] Data;
			Data = nullptr;
			Size = 0;
		}

		template<typename T>
		T& Read(uint32_t offset = 0)
		{
			return *(T*)((byte*)Data + offset);
		}

		void Write(void* data, uint32_t size, uint32_t offset = 0) 
		{
			TOAST_CORE_ASSERT((offset + size) <= Size, "Buffer overflow!");
			std::memcpy((byte*)Data + offset, data, size);
		}

		inline uint32_t GetSize() const { return Size; }
	};
}