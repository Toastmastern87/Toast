#pragma once

#include "Toast/Scene/Scene.h"

#include <filesystem>
#include <string>

namespace Toast {

	class Project 
	{
	public:
		Project() = default;
		Project(std::string& name, std::filesystem::path& path);
		~Project();

		Scene* GetActiveScene() { return mActiveScene.get(); }
	private:
		void CreateDefaultScene();

	private:
		std::string mName;

		std::filesystem::path mPath;

		std::unordered_map<UUID, Ref<Scene>> mScenes;
		Ref<Scene> mActiveScene;
	};

}