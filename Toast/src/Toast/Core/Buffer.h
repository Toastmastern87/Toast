#pragma once

#include "Toast/Core/Base.h"
#include "Toast/Core/Log.h"

namespace Toast 
{
	struct Buffer 
	{
		uint8_t* Data = nullptr;
		uint64_t Size = 0;

		Buffer() : Data(nullptr), Size(0) 
		{
		}

		Buffer(uint8_t* data, uint32_t size)
			: Data(data), Size(size)
		{
		}

		Buffer(uint64_t size)
			: Size(size)
		{
			Allocate(Size);
		}

		void Allocate(uint64_t size)
		{
			Release();
			
			if (size == 0)
				return;

			Data = new uint8_t[size];
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
		T& Read(uint64_t offset = 0)
		{
			return *(T*)((byte*)Data + offset);
		}

		template<typename T>
		T* As()
		{
			return (T*)Data;
		}

		void Write(uint8_t* data, uint64_t size, uint64_t offset = 0)
		{
			TOAST_CORE_ASSERT((offset + size) <= Size, "Buffer overflow!");
			std::memcpy((byte*)Data + offset, data, size);
		}

		operator bool() const 
		{
			return (bool)Data;
		}
	};
}