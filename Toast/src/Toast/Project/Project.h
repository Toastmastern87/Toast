#pragma once

#include "Toast/Scene/Scene.h"

#include <filesystem>
#include <string>

namespace Toast {

	struct ProjectSceneEntry
	{
		UUID Id;
		std::filesystem::path Path;   
	};

	class Project 
	{
	public:
		Project() = default;
		Project(std::string& name, std::filesystem::path& path);
		~Project();

		const std::string& GetName() const { return mName; }
		const std::filesystem::path& GetPath() const { return mPath; }

		const std::unordered_map<UUID, ProjectSceneEntry>& GetScenes() const { return mScenes; }

		UUID CreateNewScene(const std::string& baseName = "NewScene", bool setActive = false);
		bool RenameScene(UUID id, const std::string& newName);

		UUID GetActiveSceneID() const { return mActiveSceneID; }
		void SetActiveScene(UUID id) { mActiveSceneID = id; }

		std::filesystem::path GetScenePath(UUID id) const
		{
			auto it = mScenes.find(id);
			return (it != mScenes.end()) ? it->second.Path : std::filesystem::path{};
		}

		std::filesystem::path GetActiveScenePath() const
		{
			return GetScenePath(mActiveSceneID);
		}

		std::string GetSceneDisplayName(UUID id) const
		{
			auto it = mScenes.find(id);
			if (it == mScenes.end())
				return "Untitled Scene";

			// If your scene name == filename, this is perfect and zero-cost.
			return it->second.Path.stem().string();
		}
	private:
		void CreateDefaultScene();
	private:
		std::string mName;
		std::filesystem::path mPath;

		std::unordered_map<UUID, ProjectSceneEntry> mScenes;
		UUID mActiveSceneID{};

		friend class ProjectSerializer;
	};

}