#pragma once

#include "Toast/Assets/Asset.h"

#include <filesystem>

namespace Toast {

	class Texture2D;

	// -----------------------------------------------------------------
	// Binary .tasset format header — shared by serialize and deserialize.
	// -----------------------------------------------------------------

	constexpr uint32_t TASSET_MAGIC = 0x54415354; // "TAST" in little-endian
	constexpr uint16_t TASSET_VERSION = 1;

#pragma pack(push, 1)
	struct TAssetHeader
	{
		uint32_t Magic = TASSET_MAGIC;
		uint16_t AssetType = 0;
		uint16_t Version = TASSET_VERSION;
		uint64_t Handle = 0;
	};

	struct TAssetTexture2DPayload
	{
		uint32_t Width = 0;
		uint32_t Height = 0;
		uint32_t Format = 0;     // DXGI_FORMAT
		uint32_t SRVFormat = 0;     // DXGI_FORMAT for the SRV
		uint32_t MipLevels = 0;
		uint32_t RowPitch = 0;
		uint64_t DataSize = 0;     // Size of raw pixel data in bytes
		// Followed by DataSize bytes of raw pixel data
	};
#pragma pack(pop)

	class AssetSerializer
	{
	public:
		static bool SerializeTexture2D(AssetHandle handle, const Ref<Texture2D>& texture, const std::filesystem::path& outputPath);

		static Ref<Texture2D> DeserializeTexture2D(const std::filesystem::path& inputPath);

		static bool ValidateFile(const std::filesystem::path& path, TAssetHeader& outHeader);
	};
}