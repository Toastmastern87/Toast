#pragma once

#include <string>
#include <optional>

namespace Toast {

	class FileDialogs 
	{
	public:
		// These return empty string if canceled
		static std::optional<std::string> OpenFile(const char* filter);
		static std::optional<std::string> SaveFile(const char* filter);
		static std::vector<std::string> GetAllFiles(std::string path);
	};

}