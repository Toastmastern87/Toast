#pragma once

#include "Project.h"

#include <yaml-cpp/yaml.h>

namespace Toast {

	class ProjectSerializer
	{
	public:
		ProjectSerializer(Project* project)
			: mProject(project) {}

		void Serialize(const std::string& filepath);
		bool Deserialize(const std::string& filepath);

	private:
		Project* mProject = nullptr;
	};

}
