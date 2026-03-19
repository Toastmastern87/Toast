#pragma once

#include "Toast/Core/UUID.h"

#include <string>
#include <cstdint>

namespace Toast {

	enum class AssetType : uint16_t
	{
		None = 0,
		Texture2D
	};

	inline const char* AssetTypeToString(AssetType type)
	{
		switch (type)
		{
		case AssetType::None: return "None";
		case AssetType::Texture2D: return "Texture2D";
		default: return "Unknown";
		}
	}

	inline AssetType AssetTypeFromString(const char* str)
	{
		if (strcmp(str, "Texture2D") == 0) return AssetType::Texture2D;
		return AssetType::None; // Default to None for unknown types
	}

	enum class AssetFlag : uint16_t
	{
		None = 0,
		Missing = 1 << 0, // Asset file is missing on disk
		Invalid = 1 << 1, // Asset file is present but failed to load (e.g., corrupted)
	};

	inline AssetFlag operator|(AssetFlag a, AssetFlag b)
	{
		return static_cast<AssetFlag>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
	}

	inline bool HasFlag(AssetFlag flags, AssetFlag flag)
	{
		return (static_cast<uint16_t>(flags) & static_cast<uint16_t>(flag)) != 0;
	}

	using AssetHandle = UUID;

	class Asset
	{
	public:
		virtual ~Asset() = default;

		virtual AssetType GetAssetType() const = 0;

		bool IsValid() const { return !HasFlag(mFlags, AssetFlag::Missing) && !HasFlag(mFlags, AssetFlag::Invalid); }

		AssetFlag GetFlags() const { return mFlags; }
		void SetFlags(AssetFlag flags) { mFlags = flags; }
	private:
		AssetFlag mFlags = AssetFlag::None;
	};

}