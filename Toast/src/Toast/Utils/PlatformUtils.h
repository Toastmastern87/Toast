#pragma once

#include <string>

namespace Toast {

	class FileDialogs 
	{
	public:
		// These return empty string if cancelled
		static std::string OpenFile(const char* filter);
		static std::string SaveFile(const char* filter);
	};
}