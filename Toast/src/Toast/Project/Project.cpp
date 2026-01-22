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

	bool Project::DeleteScene(UUID id)
	{
		auto it = mScenes.find(id);
		if (it == mScenes.end())
			return false;

		// Absolute path to serialized scene file
		const std::filesystem::path rel = it->second.Path;
		const std::filesystem::path abs = mPath / rel;

		TOAST_CORE_INFO("Deleting scene '%s' (%s)", rel.stem().string().c_str(), abs.string().c_str());

		// Remove serialized file (best effort, but if it fails we should not keep the entry)
		if (std::filesystem::exists(abs))
		{
			std::error_code ec;
			std::filesystem::remove(abs, ec);
			if (ec)
				TOAST_CORE_ERROR("Failed to delete scene file: %s\n\t%s", abs.string().c_str(), ec.message().c_str());
		}

		mScenes.erase(it);

		return true;
	}

	UUID Project::ImportScene(const std::filesystem::path& srcSceneFileAbs, bool moveInsteadOfCopy /*= false*/)
	{
		// Basic validation
		if (srcSceneFileAbs.empty())
			return UUID{};

		std::error_code ec;

		if (!std::filesystem::exists(srcSceneFileAbs, ec) || ec)
		{
			TOAST_CORE_WARN("ImportScene: source does not exist: %s", srcSceneFileAbs.string().c_str());
			return UUID{};
		}

		if (srcSceneFileAbs.extension() != ".tscene")
		{
			TOAST_CORE_WARN("ImportScene: not a .tscene file: %s", srcSceneFileAbs.string().c_str());
			return UUID{};
		}

		const std::filesystem::path scenesDirAbs = mPath / "Assets" / "Scenes";
		std::filesystem::create_directories(scenesDirAbs, ec);
		if (ec)
		{
			TOAST_CORE_ERROR("ImportScene: failed to create scenes directory: %s (%s)",
				scenesDirAbs.string().c_str(), ec.message().c_str());
			return UUID{};
		}

		// Create a safe destination filename based on the source stem
		std::string safeStem = SanitizeFileStem(srcSceneFileAbs.stem().string());
		if (safeStem.empty())
			safeStem = "Scene";

		std::filesystem::path desiredAbs = scenesDirAbs / (safeStem + ".tscene");
		std::filesystem::path dstAbs = MakeUniquePath(desiredAbs);

		// Copy/move into project
		bool copiedOrMoved = false;

		if (moveInsteadOfCopy)
		{
			std::filesystem::rename(srcSceneFileAbs, dstAbs, ec);
			if (!ec)
			{
				copiedOrMoved = true;
			}
			else
			{
				// Cross-device rename commonly fails; fallback to copy.
				ec.clear();
			}
		}

		if (!copiedOrMoved)
		{
			std::filesystem::copy_file(
				srcSceneFileAbs,
				dstAbs,
				std::filesystem::copy_options::none,
				ec
			);

			if (ec)
			{
				TOAST_CORE_ERROR("ImportScene: failed to copy scene:\n\tFrom: %s\n\tTo:   %s\n\tErr:  %s",
					srcSceneFileAbs.string().c_str(),
					dstAbs.string().c_str(),
					ec.message().c_str());
				return UUID{};
			}

			copiedOrMoved = true;
		}

		// Register in project scenes list
		// Store RELATIVE path in project, consistent with your other code.
		std::filesystem::path rel = std::filesystem::relative(dstAbs, mPath, ec);
		if (ec)
		{
			// Fallback: store a known relative form
			rel = std::filesystem::path("Assets") / "Scenes" / dstAbs.filename();
			ec.clear();
		}

		UUID id;
		mScenes[id] = ProjectSceneEntry(id, rel);

		TOAST_CORE_INFO("Imported scene '%s'\n\tFrom: %s\n\tTo:   %s", rel.stem().string().c_str(), srcSceneFileAbs.string().c_str(), dstAbs.string().c_str());

		return id;
	}

	UUID Project::FindSceneByDisplayName(const std::string& displayName) const
	{
		// Compare with filename stem (Assets/Scenes/Foo.tscene => "Foo")
		for (const auto& [id, entry] : mScenes)
		{
			const std::string stem = entry.Path.stem().string();
			if (stem == displayName)
				return id;
		}

		return UUID{}; // invalid
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