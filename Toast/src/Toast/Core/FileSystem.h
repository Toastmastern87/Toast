#pragma once

#include "Toast/Core/Buffer.h"

namespace Toast {

	class FileSystem
	{
	public:
		static Buffer ReadFileBinary(const std::filesystem::path& filepath);
	};

}