#include "tpch.h"

#include "AssetManager.h"

#include "Toast/Project/Project.h"

#include "Toast/Renderer/Texture.h"

namespace Toast {

	AssetRegistry& AssetManager::GetActiveRegistry()
	{
		TOAST_CORE_ASSERT(sActiveProject, "No active project set in AssetManager!");
		return sActiveProject->GetAssetRegistry();
	}

	void AssetManager::Init()
	{
		TOAST_CORE_INFO("AssetManager initialized.");
	}

	void AssetManager::Shutdown()
	{
		sActiveProject = nullptr;
		TOAST_CORE_INFO("AssetManager shut down.");
	}

	void AssetManager::SetActiveProject(Ref<Project> project)
	{
		TOAST_CORE_ASSERT(project, "Cannot set a null project as active!");
		sActiveProject = project;
		TOAST_CORE_INFO("AssetManager: Active project set to '%s' (asset dir: %s)",	project->GetName().c_str(),	project->GetAssetDirectory().string().c_str());
	}


	AssetHandle AssetManager::ImportAsset(const std::filesystem::path& filepath, const std::filesystem::path& destSubDir)
	{
		TOAST_PROFILE_FUNCTION();

		TOAST_CORE_ASSERT(sActiveProject, "No active project set!");

		if (!std::filesystem::exists(filepath))
		{
			TOAST_CORE_ERROR("AssetManager: External file not found: '%s'", filepath.string().c_str());
			return AssetHandle(0);
		}

		// Determine where inside the asset directory to put it.
		std::filesystem::path destDir = sActiveProject->GetAssetDirectory();
		if (!destSubDir.empty())
			destDir /= destSubDir;

		std::filesystem::create_directories(destDir);

		// Copy the file into the project's asset directory.
		std::filesystem::path destPath = destDir / filepath.filename();

		if (std::filesystem::exists(destPath))
		{
			TOAST_CORE_WARN("AssetManager: File '%s' already exists in project, using existing file.",
				destPath.string().c_str());
		}
		else
		{
			std::filesystem::copy_file(filepath, destPath);
			TOAST_CORE_INFO("AssetManager: Copied '%s' -> '%s'", filepath.string().c_str(), destPath.string().c_str());
		}

		// Register using the path relative to the asset root.
		auto relativePath = std::filesystem::relative(destPath, sActiveProject->GetAssetDirectory());

		AssetRegistry& registry = GetActiveRegistry();

		// If this path is already registered, return the existing handle.
		AssetHandle existing = registry.GetHandleFromPath(relativePath);
		if (existing != AssetHandle(0))
			return existing;

		AssetType type = DeduceAssetType(relativePath.extension());
		if (type == AssetType::None)
		{
			TOAST_CORE_WARN("AssetManager: Unknown asset type for '%s'", relativePath.string().c_str());
			return AssetHandle(0);
		}

		AssetHandle handle;
		AssetEntry& entry = registry[handle];
		entry.Metadata.Type = type;
		entry.Metadata.FilePath = relativePath;
		entry.Metadata.IsMemoryAsset = false;

		TOAST_CORE_INFO("AssetManager: Imported '%s' as %s (handle: %llu)",	relativePath.string().c_str(), AssetTypeToString(type), (uint64_t)handle);

		return handle;
	}

	bool AssetManager::IsHandleValid(AssetHandle handle)
	{
		return handle != AssetHandle(0) && GetActiveRegistry().Contains(handle);
	}

	bool AssetManager::IsAssetLoaded(AssetHandle handle)
	{
		const AssetEntry* entry = GetActiveRegistry().Get(handle);
		return entry && entry->Resource != nullptr;
	}

	const AssetMetadata* AssetManager::GetMetadata(AssetHandle handle)
	{
		const AssetEntry* entry = GetActiveRegistry().Get(handle);
		return entry ? &entry->Metadata : nullptr;
	}

	AssetHandle AssetManager::GetHandleFromPath(const std::filesystem::path& path)
	{
		return GetActiveRegistry().GetHandleFromPath(path);
	}

	AssetType AssetManager::GetAssetTypeFromPath(const std::filesystem::path& path)
	{
		return DeduceAssetType(path.extension());
	}

	void AssetManager::UnloadAsset(AssetHandle handle)
	{
		if (AssetEntry* entry = GetActiveRegistry().Get(handle))
			entry->Resource = nullptr;
	}

	void AssetManager::RemoveAsset(AssetHandle handle)
	{
		GetActiveRegistry().Remove(handle);
	}

	bool AssetManager::LoadAsset(AssetHandle handle)
	{
		TOAST_PROFILE_FUNCTION();

		AssetEntry* entry = GetActiveRegistry().Get(handle);
		if (!entry)
			return false;

		std::filesystem::path fullPath = sActiveProject->GetAssetDirectory() / entry->Metadata.FilePath;

		if (!std::filesystem::exists(fullPath))
		{
			TOAST_CORE_ERROR("AssetManager: File not found: '%s'", fullPath.string().c_str());
			return false;
		}

		Ref<Asset> asset = nullptr;

		switch (entry->Metadata.Type)
		{
		case AssetType::Texture2D:
			asset = CreateRef<Texture2D>(fullPath.string());
			break;
		default:
			TOAST_CORE_ERROR("AssetManager: No loader for asset type %s", AssetTypeToString(entry->Metadata.Type));
			return false;
		}

		if (asset && asset->IsValid())
		{
			entry->Resource = asset;
			return true;
		}

		TOAST_CORE_ERROR("AssetManager: Failed to load '%s'", fullPath.string().c_str());
		return false;
	}

	void AssetManager::SerializeRegistry()
	{
		TOAST_PROFILE_FUNCTION();

		auto outputPath = sActiveProject->GetAssetRegistryPath();
		AssetRegistry& registry = GetActiveRegistry();

		std::ofstream out(outputPath);
		TOAST_CORE_ASSERT(out.is_open(), "AssetManager: Could not open '%s' for writing", outputPath.string().c_str());

		out << "# Toast Engine Asset Registry\n";
		out << "# handle|type|filepath\n";

		for (const auto& [handle, entry] : registry)
		{
			if (entry.Metadata.IsMemoryAsset)
				continue;

			out << (uint64_t)handle << "|"	<< AssetTypeToString(entry.Metadata.Type) << "|" << entry.Metadata.FilePath.string() << "\n";
		}

		TOAST_CORE_INFO("AssetManager: Serialized %zu assets to '%s'", registry.Count(), outputPath.string().c_str());
	}

	void AssetManager::DeserializeRegistry()
	{
		TOAST_PROFILE_FUNCTION();

		auto inputPath = sActiveProject->GetAssetRegistryPath();
		AssetRegistry& registry = GetActiveRegistry();

		registry.Clear();

		std::ifstream in(inputPath);
		if (!in.is_open())
		{
			TOAST_CORE_WARN("AssetManager: No registry file found at '%s', starting with an empty registry.", inputPath.string().c_str());
			return;
		}

		std::string line;
		size_t count = 0;

		while (std::getline(in, line))
		{
			if (line.empty() || line[0] == '#')
				continue;

			std::istringstream ss(line);
			std::string handleStr, typeStr, pathStr;

			if (!std::getline(ss, handleStr, '|')) continue;
			if (!std::getline(ss, typeStr, '|'))   continue;
			if (!std::getline(ss, pathStr, '|'))   continue;

			AssetHandle handle(std::stoull(handleStr));
			AssetType type = AssetTypeFromString(typeStr.c_str());

			if (type == AssetType::None)
			{
				TOAST_CORE_WARN("AssetManager: Unknown type '%s' in registry, skipping.", typeStr.c_str());
				continue;
			}

			AssetEntry& entry = registry[handle];
			entry.Metadata.Type = type;
			entry.Metadata.FilePath = pathStr;
			entry.Metadata.IsMemoryAsset = false;
			count++;
		}

		TOAST_CORE_INFO("AssetManager: Deserialized %zu assets from '%s'", count, inputPath.string().c_str());
	}

	const AssetRegistry& AssetManager::GetRegistry()
	{
		return GetActiveRegistry();
	}

	void AssetManager::Each(AssetType type, const std::function<void(AssetHandle, const AssetMetadata&)>& fn)
	{
		for (const auto& [handle, entry] : GetActiveRegistry())
		{
			if (entry.Metadata.Type == type)
				fn(handle, entry.Metadata);
		}
	}

	void AssetManager::EachAll(const std::function<void(AssetHandle, const AssetMetadata&)>& fn)
	{
		for (const auto& [handle, entry] : GetActiveRegistry())
			fn(handle, entry.Metadata);
	}

	void AssetManager::BakeAssets(const std::filesystem::path& outputDir)
	{
		TOAST_PROFILE_FUNCTION();

		TOAST_CORE_ASSERT(sActiveProject, "No active project set for build!");

		std::filesystem::create_directories(outputDir);

		AssetRegistry& registry = GetActiveRegistry();
		size_t baked = 0;
		size_t failed = 0;

		// Write a runtime registry that maps handles to the baked .tasset paths
		// instead of the original source file paths.
		std::ofstream registryOut(outputDir / "assets.toastreg");
		TOAST_CORE_ASSERT(registryOut.is_open(), "Could not create runtime registry file!");

		registryOut << "# Toast Engine Asset Registry (Runtime)\n";
		registryOut << "# handle|type|filepath\n";

		for (auto& [handle, entry] : registry)
		{
			if (entry.Metadata.IsMemoryAsset)
				continue;

			// Ensure the asset is loaded so we have its data.
			if (!entry.Resource)
			{
				if (!LoadAsset(handle))
				{
					TOAST_CORE_ERROR("AssetManager::Build: Failed to load asset '%s', skipping.",
						entry.Metadata.FilePath.string().c_str());
					failed++;
					continue;
				}
			}

			// Build the output path: same relative structure but with .tasset extension.
			std::filesystem::path relativeBaked = entry.Metadata.FilePath;
			relativeBaked.replace_extension(".tasset");
			std::filesystem::path fullOutputPath = outputDir / relativeBaked;

			bool success = false;

			switch (entry.Metadata.Type)
			{
			case AssetType::Texture2D:
			{
				auto texture = std::static_pointer_cast<Texture2D>(entry.Resource);
				//success = AssetSerializer::SerializeTexture2D(handle, texture, fullOutputPath);
				break;
			}

			default:
				TOAST_CORE_WARN("AssetManager::Build: No baking support for asset type %s, skipping.",
					AssetTypeToString(entry.Metadata.Type));
				continue;
			}

			if (success)
			{
				registryOut << (uint64_t)handle << "|" << AssetTypeToString(entry.Metadata.Type) << "|" << relativeBaked.string() << "\n";
				baked++;
			}
			else
			{
				TOAST_CORE_ERROR("AssetManager::Build: Failed to bake '%s'", entry.Metadata.FilePath.string().c_str());
				failed++;
			}
		}

		registryOut.close();

		TOAST_CORE_INFO("AssetManager: Build complete. %zu assets baked, %zu failed. Output: '%s'",
			baked, failed, outputDir.string().c_str());
	}

	AssetType AssetManager::DeduceAssetType(const std::filesystem::path& extension)
	{
		std::string ext = extension.string();
		for (auto& c : ext) c = (char)std::tolower(c);

		if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
			ext == ".bmp" || ext == ".tga" || ext == ".hdr" || ext == ".dds")
			return AssetType::Texture2D;

		return AssetType::None;
	}

}