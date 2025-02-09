#pragma once

#include "Project.h"

namespace Toast {

	class ProjectSerializer
	{
	public:
		static void Serialize(const std::string& filepath);

		static void Deserialize(const std::string& filepath);
	};

}
