#include "tpch.h"

#include "AssetSerializer.h"

#include "Toast/Renderer/Texture.h" 

#include <fstream>

namespace Toast {

	bool AssetSerializer::SerializeTexture2D(AssetHandle handle, const Ref<Texture2D>& texture, const std::filesystem::path& outputPath)
	{
		TOAST_PROFILE_FUNCTION();
		if (!texture)
		{
			TOAST_CORE_ERROR("AssetSerializer: Cannot serialize null Texture2D");
			return false;
		}

		const auto& imageData = texture->GetImageData();
		if (imageData.empty())
		{
			TOAST_CORE_ERROR("AssetSerializer: Texture2D has no pixel data to serialize (handle: %llu)",
				(uint64_t)handle);
			return false;
		}

		// Ensure the output directory exists.
		std::filesystem::create_directories(outputPath.parent_path());

		std::ofstream out(outputPath, std::ios::binary);
		if (!out.is_open())
		{
			TOAST_CORE_ERROR("AssetSerializer: Could not open '%s' for writing", outputPath.string().c_str());
			return false;
		}

		// Write the common header.
		TAssetHeader header;
		header.Magic = TASSET_MAGIC;
		header.AssetType = static_cast<uint16_t>(AssetType::Texture2D);
		header.Version = TASSET_VERSION;
		header.Handle = static_cast<uint64_t>(handle);
		out.write(reinterpret_cast<const char*>(&header), sizeof(header));

		// Write the Texture2D-specific payload.
		TAssetTexture2DPayload payload;
		payload.Width = texture->GetWidth();
		payload.Height = texture->GetHeight();
		payload.Format = static_cast<uint32_t>(texture->GetFormat());
		payload.SRVFormat = static_cast<uint32_t>(texture->GetSRVFormat());
		payload.MipLevels = texture->GetMipLevelCount();
		payload.RowPitch = texture->GetRowPitch();
		payload.DataSize = imageData.size();
		out.write(reinterpret_cast<const char*>(&payload), sizeof(payload));

		// Write the raw pixel data.
		out.write(reinterpret_cast<const char*>(imageData.data()), imageData.size());
	
		TOAST_CORE_INFO("AssetSerializer: Baked Texture2D '%s' (%ux%u, %zu bytes) -> '%s'",	texture->GetFilePath().c_str(),	payload.Width, payload.Height, payload.DataSize, outputPath.string().c_str());

		return true;
	}

	Ref<Texture2D> AssetSerializer::DeserializeTexture2D(const std::filesystem::path& inputPath)
	{
		TOAST_PROFILE_FUNCTION();

		std::ifstream in(inputPath, std::ios::binary);
		if (!in.is_open())
		{
			TOAST_CORE_ERROR("AssetSerializer: Could not open '%s' for reading", inputPath.string().c_str());
			return nullptr;
		}

		// Read and validate the header.
		TAssetHeader header;
		in.read(reinterpret_cast<char*>(&header), sizeof(header));

		if (header.Magic != TASSET_MAGIC)
		{
			TOAST_CORE_ERROR("AssetSerializer: Invalid magic number in '%s'", inputPath.string().c_str());
			return nullptr;
		}

		if (header.AssetType != static_cast<uint16_t>(AssetType::Texture2D))
		{
			TOAST_CORE_ERROR("AssetSerializer: Expected Texture2D but got type %u in '%s'",	header.AssetType, inputPath.string().c_str());
			return nullptr;
		}

		if (header.Version > TASSET_VERSION)
		{
			TOAST_CORE_ERROR("AssetSerializer: Unsupported version %u in '%s' (max supported: %u)", header.Version, inputPath.string().c_str(), TASSET_VERSION);
			return nullptr;
		}

		// Read the Texture2D payload.
		TAssetTexture2DPayload payload;
		in.read(reinterpret_cast<char*>(&payload), sizeof(payload));

		// Read the raw pixel data.
		std::vector<uint8_t> pixelData(payload.DataSize);
		in.read(reinterpret_cast<char*>(pixelData.data()), payload.DataSize);

		if (!in.good())
		{
			TOAST_CORE_ERROR("AssetSerializer: Failed to read pixel data from '%s'", inputPath.string().c_str());
			return nullptr;
		}

		in.close();

		auto texture = CreateRef<Texture2D>(
			static_cast<DXGI_FORMAT>(payload.Format), 
			static_cast<DXGI_FORMAT>(payload.SRVFormat), 
			payload.Width,
			payload.Height,
			D3D11_USAGE_DEFAULT,
			static_cast<D3D11_BIND_FLAG>(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET),
			1u,                                   // samples
			0u,                                    // cpuAccessFlags
			pixelData.data(),                      // initialData
			payload.RowPitch                       // rowPitch
		);

		TOAST_CORE_INFO("AssetSerializer: Loaded Texture2D from '%s' (%ux%u, %zu bytes)", inputPath.string().c_str(), payload.Width, payload.Height, payload.DataSize);

		return texture;
	}

	bool AssetSerializer::ValidateFile(const std::filesystem::path& path, TAssetHeader& outHeader)
	{
		std::ifstream in(path, std::ios::binary);
		if (!in.is_open())
			return false;

		in.read(reinterpret_cast<char*>(&outHeader), sizeof(outHeader));

		return in.good() && outHeader.Magic == TASSET_MAGIC;
	}
}