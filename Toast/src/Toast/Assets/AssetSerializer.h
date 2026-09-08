#pragma once

#include "Toast/Assets/Asset.h"

#include <filesystem>

namespace Toast {

	class Texture2D;
	class Shader;
	class Material;
	class Mesh;
	class StyleSheet;

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

	struct TAssetMeshPayload
	{
		uint32_t LODGroupCount = 0;
		uint32_t PartCount = 0;
		uint32_t LODThresholdCount = 0;
		uint32_t Topology = 0;
		uint32_t HasLODs = 0;
		uint32_t IsAnimated = 0;
		uint32_t Instanced = 0;
		uint32_t MaxNrOfIntanceObjects = 0;
	};

	struct TAssetMeshSubmesh
	{
		uint32_t BaseVertex = 0;
		uint32_t BaseIndex = 0;
		uint32_t IndexCount = 0;
		uint32_t VertexCount = 0;
		uint32_t PartIndex = UINT32_MAX;
		uint64_t MaterialHandle = 0;
	};

	struct TAssetMeshPartLODTransform 
	{
		DirectX::XMFLOAT3 LocalTranslation = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT4 LocalRotation = { 0.0f, 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 LocalScale = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT4X4 Parent;
		uint32_t Captured = 0;
	};

	struct TAssetMeshPartHeader 
	{
		DirectX::XMFLOAT3 RestTranslation = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT4 RestRotation = { 0.0f, 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 RestScale = { 0.0f, 0.0f, 0.0f };
		uint32_t RestTransformCaptured = 0;
		uint32_t IsAnimated = 0;
		uint32_t LODTransformCount = 0;
		uint32_t LODAnimationCount = 0;
	};

	struct TAssetMeshAnimation 
	{
		float Duration = 0.0f;
		uint32_t SampleCount = 0;
		uint32_t TranslationSampleCount = 0;
		uint32_t RotationSampleCount = 0;
		uint32_t ScaleSampleCount = 0;
		uint32_t TranslationSize = 0;
		uint32_t TranslationTimestampSize = 0;
		uint32_t RotationSize = 0;
		uint32_t RotationTimestampSize = 0;
		uint32_t ScaleSize = 0;
		uint32_t ScaleTimestampSize = 0;
	};
#pragma pack(pop)

	class AssetSerializer
	{
	public:
		static bool SerializeTexture2D(AssetHandle handle, const Ref<Texture2D>& texture, const std::filesystem::path& outputPath);
		static bool SerializeShader(AssetHandle handle, const Ref<Shader>& shader, const std::filesystem::path& outputPath);
		static bool SerializeMaterial(AssetHandle handle, const Ref<Material>& material, const std::filesystem::path& outputPath);
		static bool SerializeMesh(AssetHandle handle, const Ref<Mesh>& mesh, const std::filesystem::path& outputPath);
		static bool SerializeStyleSheet(AssetHandle handle, const Ref<StyleSheet>& sheet, const std::filesystem::path& outputPath);

		static Ref<Texture2D> DeserializeTexture2D(const std::filesystem::path& inputPath);
		static Ref<Shader> DeserializeShader(const std::filesystem::path& inputPath);
		static Ref<Material> DeserializeMaterial(const std::filesystem::path& inputPath);
		static Ref<Mesh> DeserializeMesh(const std::filesystem::path& inputPath);
		static Ref<StyleSheet> DeserializeStyleSheet(const std::filesystem::path& inputPath);

		static bool ValidateFile(const std::filesystem::path& path, TAssetHeader& outHeader);
	};
}