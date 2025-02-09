#include "tpch.h"
#include "Project.h"

namespace Toast {

	Project::Project(std::string& name, std::filesystem::path& basePath)
		: mName(name), mBasePath(basePath)
	{
		TOAST_CORE_INFO("Project %s Created!", mName.c_str());
	}

	Project::~Project()
	{
	}

}