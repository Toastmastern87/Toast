#pragma once

#include "Toast/Project/Asset.h"
#include "Toast/Project/AssetRegistry.h"

#include <unordered_map>
#include <functional>
#include <filesystem>

namespace Toast {

	class Texture2D;
	class Project;

	class AssetManager 
	{
	public:
		static void Init();
		static void Shutdown();

		static void SetActiveProject(Ref<Project> project);

		// Imports a file from anywhere on disk (e.g. from a file dialog).
		// Copies it into the project's asset directory and registers it.
		// Any panel can call this — content browser, material editor, inspector, etc.
		// destSubDir is optional: "textures", "textures/materials", or empty for asset root.
		static AssetHandle ImportAsset(const std::filesystem::path& filepath, const std::filesystem::path& destSubDir = "");

		template<typename T>
		static Ref<T> GetAsset(AssetHandle handle)
		{
			static_assert(std::is_base_of_v<Asset, T>, "T must derive from Asset");

			AssetEntry* entry = GetActiveRegistry().Get(handle);
			if (!entry)
				return nullptr;

			if (!entry->Resource)
			{
				if (!LoadAsset(handle))
					return nullptr;
			}

			return std::static_pointer_cast<T>(entry->Resource);
		}

		static bool IsHandleValid(AssetHandle handle);
		static bool IsAssetLoaded(AssetHandle handle);

		static const AssetMetadata* GetMetadata(AssetHandle handle);
		static AssetHandle GetHandleFromPath(const std::filesystem::path& path);
		static AssetType GetAssetTypeFromPath(const std::filesystem::path& path);

		static void UnloadAsset(AssetHandle handle);

		static void RemoveAsset(AssetHandle handle);

		static void SerializeRegistry();
		static void DeserializeRegistry();

		static void BakeAssets(const std::filesystem::path& outputDir);

		static const AssetRegistry& GetRegistry();

		static void Each(AssetType type, const std::function<void(AssetHandle, const AssetMetadata&)>& fn);
		static void EachAll(const std::function<void(AssetHandle, const AssetMetadata&)>& fn);
	private:
		static bool LoadAsset(AssetHandle handle);
		static AssetType DeduceAssetType(const std::filesystem::path& extension);
		static AssetRegistry& GetActiveRegistry();

		inline static Ref<Project> sActiveProject = nullptr;
	};

}