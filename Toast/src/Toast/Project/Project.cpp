#include "tpch.h"
#include "Project.h"

#include "Toast/Scene/SceneSerializer.h"

#include <unordered_set>
#include <cctype>

namespace Toast {

	static std::string SanitizeFileStem(const std::string& s)
	{
		// Keep it simple: letters, digits, space, underscore, dash. Convert other chars to underscore.
		std::string out;
		out.reserve(s.size());

		for (unsigned char c : s)
		{
			if (std::isalnum(c) || c == ' ' || c == '_' || c == '-')
				out.push_back((char)c);
			else
				out.push_back('_');
		}

		// Trim leading/trailing spaces/dots (avoid weird filenames on some FS)
		while (!out.empty() && (out.front() == ' ' || out.front() == '.'))
			out.erase(out.begin());
		while (!out.empty() && (out.back() == ' ' || out.back() == '.'))
			out.pop_back();

		if (out.empty())
			out = "Untitled Scene";

		return out;
	}

	static std::filesystem::path MakeUniquePath(const std::filesystem::path& desiredAbsPath)
	{
		if (!std::filesystem::exists(desiredAbsPath))
			return desiredAbsPath;

		const std::filesystem::path dir = desiredAbsPath.parent_path();
		const std::string stem = desiredAbsPath.stem().string();
		const std::string ext = desiredAbsPath.extension().string();

		for (int i = 1; i < 10000; i++)
		{
			std::filesystem::path candidate = dir / (stem + "_" + std::to_string(i) + ext);
			if (!std::filesystem::exists(candidate))
				return candidate;
		}

		// Fallback (extremely unlikely)
		return dir / (stem + "_X" + ext);
	}

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

	UUID Project::CreateNewScene(const std::string& baseName, bool setActive)
	{
		TOAST_CORE_INFO("Adding new scene");

		// Ensure scene directory exists
		const std::filesystem::path scenesDirAbs = mPath / "Assets" / "Scenes";
		std::filesystem::create_directories(scenesDirAbs);

		// Sanitize + build a unique path on disk
		std::string safeStem = SanitizeFileStem(baseName);
		std::filesystem::path desiredAbs = scenesDirAbs / (safeStem + ".tscene");
		std::filesystem::path newAbs = MakeUniquePath(desiredAbs);

		// Scene display name should match the file stem we ended up with (unique suffix, etc.)
		const std::string finalStem = newAbs.stem().string();

		// Create + serialize the scene
		Ref<Scene> scene = CreateRef<Scene>(finalStem);
		UUID id = scene->GetUUID();

		SceneSerializer serializer(scene.get());
		serializer.Serialize(newAbs.string(), finalStem);

		// Register in project (store relative path)
		std::filesystem::path newRel = std::filesystem::relative(newAbs, mPath);
		mScenes.emplace(id, ProjectSceneEntry{ id, newRel });

		if (setActive)
			mActiveSceneID = id;

		return id;
	}

	bool Project::RenameScene(UUID id, const std::string& newName)
	{
		auto it = mScenes.find(id);
		if (it == mScenes.end())
			return false;

		const std::string oldName = it->second.Path.stem().string();

		TOAST_CORE_TRACE("Renaming scene from %s to %s", oldName.c_str(), newName.c_str());

		// Old absolute path
		const std::filesystem::path oldRel = it->second.Path;
		const std::filesystem::path oldAbs = mPath / oldRel;

		if (!std::filesystem::exists(oldAbs))
			return false;

		// New absolute path (keep it in Assets/Scenes)
		const std::filesystem::path scenesDirAbs = mPath / "Assets" / "Scenes";

		std::string safeStem = SanitizeFileStem(newName);
		std::filesystem::path desiredAbs = scenesDirAbs / (safeStem + ".tscene");
		std::filesystem::path newAbs = MakeUniquePath(desiredAbs);

		// New relative path stored in project
		std::filesystem::path newRel = std::filesystem::relative(newAbs, mPath);

		// 1) Rename/move the file on disk (so we keep any external references sane)
		//    If rename fails (e.g. cross-device), you can fallback to copy+remove.
		std::error_code ec;
		std::filesystem::rename(oldAbs, newAbs, ec);
		if (ec)
		{
			// Fallback: copy + remove
			ec.clear();
			std::filesystem::copy_file(oldAbs, newAbs, std::filesystem::copy_options::overwrite_existing, ec);
			if (ec)
				return false;

			ec.clear();
			std::filesystem::remove(oldAbs, ec);
			if (ec)
				return false;
		}

		// 2) Load the scene, update the internal scene name, and re-serialize to the new file
		//    NOTE: adjust SetName(...) if your Scene uses a different API.
		Ref<Scene> scene = CreateRef<Scene>();
		{
			SceneSerializer deserializer(scene.get());
			if (!deserializer.Deserialize(newAbs.string()))
			{
				// If deserialize fails, we still have the renamed file; treat as failure so caller can handle.
				return false;
			}
		}

		// Update scene display/name (adjust this line to your engine)
		scene->SetName(newName);

		{
			SceneSerializer serializer(scene.get());
			serializer.Serialize(newAbs.string(), newName);
		}

		// 3) Update project registry
		it->second.Path = newRel;

		return true;
	}

	void Project::CreateDefaultScene()
	{
		if (!mScenes.empty())
			return;

		std::filesystem::path scenesDir = mPath / "Assets" / "Scenes";

		std::filesystem::path scenePath = scenesDir / "DefaultScene.tscene";

		Ref<Scene> scene = CreateRef<Scene>("Default Scene");
		UUID ID = scene->GetUUID();

		SceneSerializer serializer(scene.get());
		serializer.Serialize(scenePath.string(), "DefaultScene");

		mScenes.emplace(ID, ProjectSceneEntry{ ID, std::filesystem::relative(scenePath, mPath) });
		mActiveSceneID = ID;
	}

}