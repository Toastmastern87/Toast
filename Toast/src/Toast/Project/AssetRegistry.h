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
		bool						IsLoaded   = false;
		bool						IsMemoryAsset = false;  // True for runtime-created assets (no file on disk)
	};

	class AssetRegistry 
	{
	public:
		AssetMetadata* Get(AssetHandle handle)
		{
			auto it = mRegistry.find(handle);
			return it != mRegistry.end() ? &it->second : nullptr;
		}

		const AssetMetadata* Get(AssetHandle handle) const
		{
			auto it = mRegistry.find(handle);
			return it != mRegistry.end() ? &it->second : nullptr;
		}

		AssetHandle GetHandleFromPath(const std::filesystem::path& path) const
		{
			for (auto& [handle, meta] : mRegistry)
			{
				if(meta.FilePath == path)
					return handle;
			}
			return AssetHandle(0);
		}

		AssetMetadata& operator[](AssetHandle handle)
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
		std::unordered_map<AssetHandle, AssetMetadata> mRegistry;
	};
}