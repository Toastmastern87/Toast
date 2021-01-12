#include "tpch.h"
#include "SceneEnvironment.h"

#include "Renderer.h"

namespace Toast {
	
	Environment Environment::Load(const std::string& filepath) 
	{
		auto [radiance, irradiance] = Renderer::CreateEnvironmentMap(filepath);

		return { filepath, radiance, irradiance };
	}
}