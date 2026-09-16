#pragma once

#include "Asset.h"

#include <string>
#include <filesystem>
#include <unordered_map>

namespace Toast {

	struct AssetMetadata
	{
		AssetType					Type       = AssetType::None;
		std::filesystem::path		FilePath;       // Relative to your asset root directory
		bool						IsMemoryAsset = false;  // True for runtime-created assets (no file on disk)
	};

	struct Texture2DImportSettings
	{
		bool ForceSRGB = true;

		// 9-slice insets in pixels
		// All zero = not a 9 - slice texture
		uint32_t SliceLeft = 0;
		uint32_t SliceTop = 0;
		uint32_t SliceRight = 0;
		uint32_t SliceBottom = 0;

		// The region of the texture the artwork actually occupies, in source pixels.
		// Insets are measured from THESE edges, not the texture's.
		// Width/height of 0 means "the whole texture"
		uint32_t ContentX = 0;
		uint32_t ContentY = 0;
		uint32_t ContentWidth = 0;
		uint32_t ContentHeight = 0;
	};

	struct AssetEntry
	{
		AssetMetadata  Metadata;
		Ref<Asset>     Resource = nullptr;  // null until first GetAsset<> call

		// Type-specific import settings — only one will be used per entry
		Texture2DImportSettings Texture2DSettings;
	};

	class AssetRegistry 
	{
	public:
		AssetEntry* Get(AssetHandle handle)
		{
			auto it = mRegistry.find(handle);
			return it != mRegistry.end() ? &it->second : nullptr;
		}

		const AssetEntry* Get(AssetHandle handle) const
		{
			auto it = mRegistry.find(handle);
			return it != mRegistry.end() ? &it->second : nullptr;
		}

		AssetHandle GetHandleFromPath(const std::filesystem::path& path) const
		{
			for (auto& [handle, meta] : mRegistry)
			{
				if(meta.Metadata.FilePath == path)
					return handle;
			}
			return AssetHandle(0);
		}

		AssetEntry& operator[](AssetHandle handle)
		{
			return mRegistry[handle];
		}

		bool Contains(AssetHandle handle) 
		{
			return mRegistry.find(handle) != mRegistry.end();
		}

		size_t Remove(AssetHandle handle)
		{
			return mRegistry.erase(handle);
		}

		void Clear() { mRegistry.clear(); }

		size_t Count() const { return mRegistry.size(); }

		// Iterators to handle looping:  for (auto& [handle, meta] : registry) { ... }
		auto begin() { return mRegistry.begin(); }	
		auto end() { return mRegistry.end(); }
		auto begin() const { return mRegistry.cbegin(); }
		auto end() const { return mRegistry.cend(); }
	private:
		std::unordered_map<AssetHandle, AssetEntry> mRegistry;
	};
}