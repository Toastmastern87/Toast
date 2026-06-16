#pragma once

#include "Toast/Assets/Asset.h"

#include <filesystem>

namespace Toast {

	class Texture2D;
	class Shader;
	class Material;

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

	struct TAssetShaderPayload
	{
		uint32_t StageCount = 0; 
		uint32_t ElementCount = 0; 
	};

	struct TAssetShaderStageEntry
	{
		uint32_t StageType = 0;    // D3D11_SHADER_TYPE value
		uint64_t BlobSize = 0;    // size in bytes of this stage's byte code
	};

	struct TAssetShaderInputElement
	{
		uint32_t Type = 0;  // DXGI_FORMAT
		uint32_t Size = 0;  // mSize
		uint64_t Offset = 0;  // mOffset (size_t on disk as u64)
		uint32_t SemanticIndex = 0;  // mSemanticIndex
		uint32_t InputClassification = 0;  // D3D11_INPUT_CLASSIFICATION
		uint32_t InstanceDataStepRate = 0;  // mInstanceDataStepRate
		uint32_t InputSlot = 0;  // mInputSlot
	};

	struct TAssetMaterialPayload
	{
		DirectX::XMFLOAT4 Albedo = { 1, 1, 1, 1 };
		float Emission = 0.0f;
		float Metalness = 0.0f;
		float Roughness = 0.0f;

		uint32_t UseAlbedo = 0;   // 0/1
		uint32_t UseNormal = 0;
		uint32_t UseMetalRough = 0;

		uint64_t AlbedoHandle = 0; // 0 = unused
		uint64_t NormalHandle = 0;
		uint64_t MetalRoughHandle = 0;
	};
#pragma pack(pop)

	class AssetSerializer
	{
	public:
		static bool SerializeTexture2D(AssetHandle handle, const Ref<Texture2D>& texture, const std::filesystem::path& outputPath);
		static bool SerializeShader(AssetHandle handle, const Ref<Shader>& shader, const std::filesystem::path& outputPath);
		static bool SerializeMaterial(AssetHandle handle, const Ref<Material>& material, const std::filesystem::path& outputPath);

		static Ref<Texture2D> DeserializeTexture2D(const std::filesystem::path& inputPath);
		static Ref<Shader> DeserializeShader(const std::filesystem::path& inputPath);
		static Ref<Material> DeserializeMaterial(const std::filesystem::path& inputPath);

		static bool ValidateFile(const std::filesystem::path& path, TAssetHeader& outHeader);
	};
}