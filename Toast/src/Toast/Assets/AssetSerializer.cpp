#include "tpch.h"

#include "AssetSerializer.h"

#include "Toast/Renderer/Texture.h" 
#include "Toast/Renderer/Shader.h"

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

	bool AssetSerializer::SerializeShader(AssetHandle handle, const Ref<Shader>& shader, const std::filesystem::path& outputPath)
	{
		TOAST_PROFILE_FUNCTION();

		if (!shader)
		{
			TOAST_CORE_ERROR("AssetSerializer: Cannot serialize null Shader");
			return false;
		}

		const auto& blobs = shader->GetRawBlobs();
		if (blobs.empty())
		{
			TOAST_CORE_ERROR("AssetSerializer: Shader has no compiled blobs (handle: %llu)", (uint64_t)handle);
			return false;
		}

		std::filesystem::create_directories(outputPath.parent_path());
		std::ofstream out(outputPath, std::ios::binary);
		if (!out.is_open())
		{
			TOAST_CORE_ERROR("AssetSerializer: Could not open '%s' for writing", outputPath.string().c_str());
			return false;
		}

		// Capture stage order once so entries and blobs stay in sync.
		std::vector<std::pair<D3D11_SHADER_TYPE, ID3D10Blob*>> ordered(blobs.begin(), blobs.end());

		// Layout elements (may be empty for compute-only shaders).
		std::vector<ShaderLayout::ShaderInputElement> elements;
		if (shader->GetLayout())
			elements = shader->GetLayout()->GetElements();

		// --- Header ---
		TAssetHeader header;
		header.Magic = TASSET_MAGIC;
		header.AssetType = static_cast<uint16_t>(AssetType::Shader);
		header.Version = TASSET_VERSION;
		header.Handle = static_cast<uint64_t>(handle);
		out.write(reinterpret_cast<const char*>(&header), sizeof(header));

		// --- Payload ---
		TAssetShaderPayload payload;
		payload.StageCount = static_cast<uint32_t>(ordered.size());
		payload.ElementCount = static_cast<uint32_t>(elements.size());
		out.write(reinterpret_cast<const char*>(&payload), sizeof(payload));

		// --- Stage entries ---
		for (const auto& [stage, blob] : ordered)
		{
			TAssetShaderStageEntry entry;
			entry.StageType = static_cast<uint32_t>(stage);
			entry.BlobSize = static_cast<uint64_t>(blob->GetBufferSize());
			out.write(reinterpret_cast<const char*>(&entry), sizeof(entry));
		}

		// --- Blob bytes (same order) ---
		for (const auto& [stage, blob] : ordered)
			out.write(reinterpret_cast<const char*>(blob->GetBufferPointer()), static_cast<std::streamsize>(blob->GetBufferSize()));

		// --- Input layout elements ---
		for (const auto& el : elements)
		{
			uint32_t nameLen = static_cast<uint32_t>(el.mName.size());
			out.write(reinterpret_cast<const char*>(&nameLen), sizeof(nameLen));
			out.write(el.mName.data(), nameLen);

			TAssetShaderInputElement rec;
			rec.Type = static_cast<uint32_t>(el.mType);
			rec.Size = el.mSize;
			rec.Offset = static_cast<uint64_t>(el.mOffset);
			rec.SemanticIndex = el.mSemanticIndex;
			rec.InputClassification = static_cast<uint32_t>(el.mInputClassification);
			rec.InstanceDataStepRate = el.mInstanceDataStepRate;
			rec.InputSlot = el.mInputSlot;
			out.write(reinterpret_cast<const char*>(&rec), sizeof(rec));
		}

		TOAST_CORE_INFO("AssetSerializer: Baked Shader '%s' (%u stages, %u layout elems) -> '%s'", shader->GetName().c_str(), payload.StageCount, payload.ElementCount, outputPath.string().c_str());

		return out.good();
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

	Ref<Shader> AssetSerializer::DeserializeShader(const std::filesystem::path& inputPath)
	{
		TOAST_PROFILE_FUNCTION();

		std::ifstream in(inputPath, std::ios::binary);
		if (!in.is_open())
		{
			TOAST_CORE_ERROR("AssetSerializer: Could not open '%s' for reading", inputPath.string().c_str());
			return nullptr;
		}

		TAssetHeader header;
		in.read(reinterpret_cast<char*>(&header), sizeof(header));
		if (header.Magic != TASSET_MAGIC)
		{
			TOAST_CORE_ERROR("AssetSerializer: Invalid magic number in '%s'", inputPath.string().c_str());
			return nullptr;
		}
		if (header.AssetType != static_cast<uint16_t>(AssetType::Shader))
		{
			TOAST_CORE_ERROR("AssetSerializer: Expected Shader but got type %u in '%s'", header.AssetType, inputPath.string().c_str());
			return nullptr;
		}
		if (header.Version > TASSET_VERSION)
		{
			TOAST_CORE_ERROR("AssetSerializer: Unsupported version %u in '%s' (max %u)", header.Version, inputPath.string().c_str(), TASSET_VERSION);
			return nullptr;
		}

		TAssetShaderPayload payload;
		in.read(reinterpret_cast<char*>(&payload), sizeof(payload));

		// Stage entries.
		std::vector<TAssetShaderStageEntry> entries(payload.StageCount);
		for (uint32_t i = 0; i < payload.StageCount; ++i)
			in.read(reinterpret_cast<char*>(&entries[i]), sizeof(TAssetShaderStageEntry));

		// Blobs, in entry order.
		std::vector<ShaderStageBlob> stages;
		stages.reserve(payload.StageCount);
		for (const auto& entry : entries)
		{
			ShaderStageBlob s;
			s.Stage = static_cast<D3D11_SHADER_TYPE>(entry.StageType);
			s.Bytecode.resize(static_cast<size_t>(entry.BlobSize));
			in.read(reinterpret_cast<char*>(s.Bytecode.data()), static_cast<std::streamsize>(entry.BlobSize));
			stages.push_back(std::move(s));
		}

		// Input-layout elements.
		std::vector<ShaderLayout::ShaderInputElement> elements;
		elements.reserve(payload.ElementCount);
		for (uint32_t i = 0; i < payload.ElementCount; ++i)
		{
			uint32_t nameLen = 0;
			in.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
			std::string name(nameLen, '\0');
			in.read(name.data(), nameLen);

			TAssetShaderInputElement rec;
			in.read(reinterpret_cast<char*>(&rec), sizeof(rec));

			ShaderLayout::ShaderInputElement el;
			el.mName = std::move(name);
			el.mType = static_cast<DXGI_FORMAT>(rec.Type);
			el.mSize = rec.Size;
			el.mOffset = static_cast<size_t>(rec.Offset);
			el.mSemanticIndex = rec.SemanticIndex;
			el.mInputClassification = static_cast<D3D11_INPUT_CLASSIFICATION>(rec.InputClassification);
			el.mInstanceDataStepRate = rec.InstanceDataStepRate;
			el.mInputSlot = rec.InputSlot;
			elements.push_back(std::move(el));
		}

		if (!in.good())
		{
			TOAST_CORE_ERROR("AssetSerializer: Failed reading shader data from '%s'", inputPath.string().c_str());
			return nullptr;
		}
		in.close();

		std::string name = inputPath.stem().string();
		auto shader = CreateRef<Shader>(name, stages, elements);

		TOAST_CORE_INFO("AssetSerializer: Loaded Shader from '%s' (%u stages, %u layout elems)", inputPath.string().c_str(), payload.StageCount, payload.ElementCount);

		return shader;
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