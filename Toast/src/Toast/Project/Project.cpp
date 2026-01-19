#include "tpch.h"
#include "Project.h"

#include "Toast/Scene/SceneManager.h"

namespace Toast {

	Project::Project(std::string& name, std::filesystem::path& path)
		: mName(name), mPath(path)
	{
		std::filesystem::create_directories(path);

		std::string projectNameStr(name);		

		std::filesystem::path assetsPath = path / "Assets";

		// Create sub folders inside the "Assets" folder.
		std::filesystem::create_directories(assetsPath / "Scenes");
		std::filesystem::create_directories(assetsPath / "Textures");
		std::filesystem::create_directories(assetsPath / "Fonts");
		std::filesystem::create_directories(assetsPath / "Meshes");
		std::filesystem::create_directories(assetsPath / "Scripts");
		std::filesystem::create_directories(assetsPath / "Materials");
		std::filesystem::create_directories(assetsPath / "Prefabs");

		CreateDefaultScene();
	}

	Project::~Project()
	{
	}

	void Project::CreateDefaultScene()
	{
		if (!mScenes.empty())
			return;

		Ref<Scene> scene = CreateRef<Scene>(); 
		UUID id = scene->GetUUID();

		mScenes.emplace(id, scene);
		mActiveScene = scene;
	}

}