#pragma once

#include <filesystem>
#include <string>

namespace Toast {

	enum class ProjectRenderOverlay { NONE = 0, POSITIONS = 1, NORMALS = 2, ALBEDOMETALLIC = 3, ROUGHNESS = 4, LPASS = 5, ATMOSPHERICSCATTERING = 6, SSAO = 7 };

	class Project 
	{
	public:
		//Settings
		struct ProjectSettings
		{
			bool IsDirty = false;

			ProjectRenderOverlay RenderOverlaySetting = ProjectRenderOverlay::NONE;

			enum class Wireframe { NO = 0, YES = 1, ONTOP = 2 };
			Wireframe WireframeRendering = Wireframe::NO;

			bool Grid = true;
			bool CameraFrustum = true;
			bool SunLightFrustum = true;
			bool BackfaceCulling = true;
			bool FrustumCulling = true;
			bool RenderColliders = false;
			bool RenderUI = true;
			bool Shadows = true;
			bool SSAO = true;
			bool DynamicIBL = true;

			int PhysicSlowmotion = 1;
			int PhysicsFPS = 60;
			float physicsElapsedTime = 0.0;
			float SunFrustumOrthoSize = 500.0f;
		};


		Project() = default;
		Project(std::string& name, std::filesystem::path& basePath);
		~Project();

	private:
		std::string mName;

		std::filesystem::path mBasePath;
	};

}