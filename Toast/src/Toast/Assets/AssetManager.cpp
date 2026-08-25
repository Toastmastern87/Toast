#include "tpch.h"

#include "AssetManager.h"

#include "Toast/Assets/AssetSerializer.h"

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

	const std::filesystem::path AssetManager::GetAssetDirectory()
	{
		TOAST_CORE_ASSERT(sActiveProject, "No active project set!");
		return sActiveProject->GetAssetDirectory();
	}

	AssetHandle AssetManager::ImportAsset(const std::filesystem::path& filepath)
	{
		TOAST_PROFILE_FUNCTION();

		AssetRegistry& registry = GetActiveRegistry();

		// If this path is already registered, return the existing handle.
		AssetHandle existing = registry.GetHandleFromPath(filepath);
		if (existing != AssetHandle(0))
			return existing;

		AssetType type = DeduceAssetType(filepath.extension());
		if (type == AssetType::None)
		{
			TOAST_CORE_WARN("AssetManager: Unknown asset type for '%s'", filepath.string().c_str());
			return AssetHandle(0);
		}

		AssetHandle handle;
		AssetEntry& entry = registry[handle];
		entry.Metadata.Type = type;
		entry.Metadata.FilePath = filepath;
		entry.Metadata.IsMemoryAsset = false;

		TOAST_CORE_INFO("AssetManager: Imported '%s' as %s (handle: %llu)",	filepath.string().c_str(), AssetTypeToString(type), (uint64_t)handle);

		return handle;
	}

	AssetHandle AssetManager::ImportExternalAsset(const std::filesystem::path& externalPath, const std::filesystem::path& destSubDir, bool forceOverwrite)
	{
		TOAST_PROFILE_FUNCTION();

		TOAST_CORE_ASSERT(sActiveProject, "No active project set!");

		if (!std::filesystem::exists(externalPath))
		{
			TOAST_CORE_ERROR("AssetManager: External file not found: '%s'", externalPath.string().c_str());
			return AssetHandle(0);
		}

		auto assetDir = sActiveProject->GetAssetDirectory();
		auto absPath = std::filesystem::absolute(externalPath);

		// Check if the file is already inside the project's asset directory.
		// If so, skip the copy and just register it.
		auto [rootEnd, nothing] = std::mismatch(
			assetDir.begin(), assetDir.end(), absPath.begin());

		if (rootEnd == assetDir.end())
		{
			// Already inside the asset folder — just register it
			auto relativePath = std::filesystem::relative(absPath, assetDir);
			return ImportAsset(relativePath);
		}

		// Determine where inside the asset directory to put it.
		std::filesystem::path destDir = assetDir;
		if (!destSubDir.empty())
			destDir /= destSubDir;

		std::filesystem::create_directories(destDir);

		// Copy the file into the project's asset directory.
		std::filesystem::path destPath = destDir / externalPath.filename();

		if (std::filesystem::exists(destPath))
			if (forceOverwrite)
			{
				std::filesystem::copy_file(externalPath, destPath, std::filesystem::copy_options::overwrite_existing);
				TOAST_CORE_INFO("Updated project asset from source: '%s'", destPath.string().c_str());
			}
			else 
				TOAST_CORE_WARN("AssetManager: File '%s' already exists in project, using existing file.", destPath.string().c_str());
		else
		{
			std::filesystem::copy_file(externalPath, destPath);
			TOAST_CORE_INFO("AssetManager: Copied '%s' -> '%s'", externalPath.string().c_str(), destPath.string().c_str());
		}

		// If its a .gltf file look for a .bin and import it as well into the asset folder
		if (externalPath.extension() == ".gltf")
		{
			auto binSrc = externalPath;  binSrc.replace_extension(".bin");

			if (std::filesystem::exists(binSrc))
			{
				auto binDst = destPath;  binDst.replace_extension(".bin");
				std::filesystem::copy_file(binSrc, binDst, std::filesystem::copy_options::overwrite_existing);
				TOAST_CORE_INFO("AssetManager: Copied sidecar '%s'", binSrc.filename().string().c_str());
			}
			else
				TOAST_CORE_WARN("AssetManager: No sidecar .bin next to '%s'; mesh may fail to load", externalPath.filename().string().c_str());
		}


		// Register using the path relative to the asset root.
		auto relativePath = std::filesystem::relative(destPath, sActiveProject->GetAssetDirectory());
		return ImportAsset(relativePath);
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

	AssetEntry* AssetManager::GetEntry(AssetHandle handle)
	{
		return GetActiveRegistry().Get(handle);
	}

	void AssetManager::ReloadAsset(AssetHandle handle)
	{
		AssetEntry* entry = GetActiveRegistry().Get(handle);
		if (!entry || !entry->Resource) return;

		if (entry->Metadata.Type == AssetType::Shader)
		{
			auto shader = std::static_pointer_cast<Shader>(entry->Resource);
			auto sourcePath = sActiveProject->GetAssetDirectory() / entry->Metadata.FilePath;
			shader->Invalidate(sourcePath.string());
		}
	}

	void AssetManager::UnloadAsset(AssetHandle handle)
	{
		if (AssetEntry* entry = GetActiveRegistry().Get(handle))
			entry->Resource = nullptr;
	}

	bool AssetManager::RenameAsset(AssetHandle handle, const std::string& newFileName)
	{
		AssetEntry* entry = GetActiveRegistry().Get(handle);
		if (!entry)
			return false;

		auto assetDir = GetAssetDirectory();
		auto oldRelative = entry->Metadata.FilePath;
		auto newRelative = oldRelative.parent_path() / newFileName;

		// No-op if the name didn't actually change.
		if (oldRelative == newRelative)
			return true;

		auto oldFull = assetDir / oldRelative;
		auto newFull = assetDir / newRelative;

		// Don't clobber an existing different file.
		if (std::filesystem::exists(newFull))
		{
			TOAST_CORE_WARN("AssetManager: '%s' already exists, not renaming.", newRelative.string().c_str());
			return false;
		}

		std::error_code ec;
		if (std::filesystem::exists(oldFull))
		{
			std::filesystem::rename(oldFull, newFull, ec);
			if (ec)
			{
				TOAST_CORE_ERROR("AssetManager: rename failed: %s", ec.message().c_str());
				return false;
			}
		}
		// If oldFull doesn't exist yet (e.g. file not written), just update metadata.

		entry->Metadata.FilePath = newRelative;   // handle unchanged
		return true;
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
			asset = CreateRef<Texture2D>(fullPath.string(), entry->Texture2DSettings.ForceSRGB);
			break;
		case AssetType::Shader:
			asset = CreateRef<Shader>(fullPath.string());
			break;
		case AssetType::Material:
			asset = CreateRef<Material>(fullPath, FromFile{});
			break;
		case AssetType::Mesh:
			asset = CreateRef<Mesh>(fullPath.string());
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

			out << (uint64_t)handle << "|" << AssetTypeToString(entry.Metadata.Type) << "|" << entry.Metadata.FilePath.string() << "|";

			// Type-specific settings
			switch (entry.Metadata.Type)
			{
			case AssetType::Texture2D:
				out << "sRGB=" << (entry.Texture2DSettings.ForceSRGB ? "1" : "0");
				break;
			default:
				break;
			}
				
			out << "\n";
		}

		TOAST_CORE_INFO("AssetManager: Serialized %d assets to '%s'", registry.Count(), outputPath.string().c_str());
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
			std::string handleStr, typeStr, pathStr, settingsStr;

			if (!std::getline(ss, handleStr, '|')) continue;
			if (!std::getline(ss, typeStr, '|'))   continue;
			if (!std::getline(ss, pathStr, '|'))   continue;
			std::getline(ss, settingsStr, '|');

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

			// Parse type-specific settings
			switch (type)
			{
			case AssetType::Texture2D:
				if (settingsStr.find("sRGB=0") != std::string::npos)
					entry.Texture2DSettings.ForceSRGB = false;
				else
					entry.Texture2DSettings.ForceSRGB = true;  // default
				break;
			default:
				break;
			}

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

	void AssetManager::RegisterEngineShader(const std::string& name, AssetHandle handle)
	{
		sEngineShaderHandles[name] = handle;
	}

	AssetHandle AssetManager::GetEngineShaderHandle(const std::string& name)
	{
		auto it = sEngineShaderHandles.find(name);
		if (it == sEngineShaderHandles.end())
		{
			TOAST_CORE_WARN("GetEngineShaderHandle: unknown shader '%s'", name.c_str());
			return 0;
		}
		return it->second;
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
					TOAST_CORE_ERROR("AssetManager::Build: Failed to load asset '%s', skipping.", entry.Metadata.FilePath.string().c_str());
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
				success = AssetSerializer::SerializeTexture2D(handle, texture, fullOutputPath);
				break;
			}
			case AssetType::Shader:                                
			{
				auto shader = std::static_pointer_cast<Shader>(entry.Resource);
				success = AssetSerializer::SerializeShader(handle, shader, fullOutputPath);
				break;
			}
			case AssetType::Material:                                
			{
				auto material = std::static_pointer_cast<Material>(entry.Resource);
				success = AssetSerializer::SerializeMaterial(handle, material, fullOutputPath);
				break;
			}
			case AssetType::Mesh:
			{
				auto mesh = std::static_pointer_cast<Mesh>(entry.Resource);
				success = AssetSerializer::SerializeMesh(handle, mesh, fullOutputPath);
				break;
			}
			default:
				TOAST_CORE_WARN("AssetManager::Build: No baking support for asset type %s, skipping.", AssetTypeToString(entry.Metadata.Type));
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

		TOAST_CORE_INFO("AssetManager: Build complete. %zu assets baked, %zu failed. Output: '%s'",	baked, failed, outputDir.string().c_str());
	}

	AssetType AssetManager::DeduceAssetType(const std::filesystem::path& extension)
	{
		std::string ext = extension.string();
		for (auto& c : ext) c = (char)std::tolower(c);

		if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||	ext == ".bmp" || ext == ".tga" || ext == ".hdr" || ext == ".dds" || ext == ".tif")
			return AssetType::Texture2D;
		if (ext == ".hlsl") 
			return AssetType::Shader;
		if (ext == ".tmtl") 
			return AssetType::Material;
		if (ext == ".gltf" || ext == ".glb")
			return AssetType::Mesh;

		return AssetType::None;
	}

}
