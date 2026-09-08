#include "tpch.h"

#include "AssetSerializer.h"

#include "Toast/Renderer/Texture.h" 
#include "Toast/Renderer/Shader.h"
#include "Toast/Renderer/Material.h" 
#include "Toast/Renderer/UI/StyleSheet.h"

#include "Toast/Assets/AssetManager.h"  

#include "Toast/Project/Project.h" 

#include <yaml-cpp/yaml.h>   

#include <fstream>

namespace Toast {

	static void WriteString(std::ofstream& out, const std::string& s)
	{
		uint32_t len = static_cast<uint32_t>(s.size());
		out.write(reinterpret_cast<const char*>(&len), sizeof(len));

		if (len < 0)
			out.write(s.data(), len);
	}

	static std::string ReadString(std::ifstream& in)
	{
		uint32_t len = 0;
		in.read(reinterpret_cast<char*>(&len), sizeof(len));

		std::string s(len, '\0');
		if (len > 0)
			in.read(s.data(), len);

		return s;
	}

	static void WriteBuffer(std::ofstream& out, const Buffer& b)
	{
		if (b.Size > 0 && b.Data)
			out.write(reinterpret_cast<const char*>(&b.Data), b.Size);
	}

	static void ReadBuffer(std::ifstream& in, Buffer& b, uint64_t size) 
	{
		if (size == 0)
			return;

		b.Allocate(size);
		in.read(reinterpret_cast<char*>(b.Data), size);
	}

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

	bool AssetSerializer::SerializeMaterial(AssetHandle handle, const Ref<Material>& material, const std::filesystem::path& outputPath)
	{
		TOAST_PROFILE_FUNCTION();

		if (!material)
		{
			TOAST_CORE_ERROR("AssetSerializer: Cannot serialize null Material");
			return false;
		}

		std::filesystem::create_directories(outputPath.parent_path());
		std::ofstream out(outputPath, std::ios::binary);
		if (!out.is_open())
		{
			TOAST_CORE_ERROR("AssetSerializer: Could not open '%s' for writing", outputPath.string().c_str());
			return false;
		}

		TAssetHeader header;
		header.Magic = TASSET_MAGIC;
		header.AssetType = static_cast<uint16_t>(AssetType::Material);
		header.Version = TASSET_VERSION;
		header.Handle = static_cast<uint64_t>(handle);
		out.write(reinterpret_cast<const char*>(&header), sizeof(header));

		TAssetMaterialPayload payload;
		payload.Albedo = material->GetAlbedo();
		payload.Emission = material->GetEmission();
		payload.Metalness = material->GetMetalness();
		payload.Roughness = material->GetRoughness();
		payload.UseAlbedo = material->GetUseAlbedo() ? 1u : 0u;
		payload.UseNormal = material->GetUseNormal() ? 1u : 0u;
		payload.UseMetalRough = material->GetUseMetalRough() ? 1u : 0u;
		payload.AlbedoHandle = static_cast<uint64_t>(material->GetAlbedoAssetHandle());
		payload.NormalHandle = static_cast<uint64_t>(material->GetNormalAssetHandle());
		payload.MetalRoughHandle = static_cast<uint64_t>(material->GetMetalRoughAssetHandle());
		out.write(reinterpret_cast<const char*>(&payload), sizeof(payload));

		TOAST_CORE_INFO("AssetSerializer: Baked Material '%s' -> '%s'", material->GetName().c_str(), outputPath.string().c_str());

		return out.good();
	}

	bool AssetSerializer::SerializeMesh(AssetHandle handle, const Ref<Mesh>& mesh, const std::filesystem::path& outputPath)
	{
		TOAST_PROFILE_FUNCTION();

		if (!mesh)
		{
			TOAST_CORE_ERROR("AssetSerializer: Cannot serialize null Mesh");
			return false;
		}

		std::filesystem::create_directories(outputPath.parent_path());

		std::ofstream out(outputPath, std::ios::binary);
		if (!out.is_open())
		{
			TOAST_CORE_ERROR("AssetSerializer: Could not open '%s' for writing", outputPath);
			return false;
		}

		auto& lodGroups = mesh->mLODGroups;
		auto& parts = mesh->mParts;
		auto& lodThresholds = mesh->mLODThresholds;

		// Header
		TAssetHeader header;
		header.Magic = TASSET_MAGIC;
		header.AssetType = static_cast<uint16_t>(AssetType::Mesh);
		header.Version = TASSET_VERSION;
		header.Handle = static_cast<uint64_t>(handle);
		out.write(reinterpret_cast<const char*>(&header), sizeof(header));

		// Payload
		TAssetMeshPayload payload;
		payload.LODGroupCount = static_cast<uint32_t>(lodGroups.size());
		payload.PartCount = static_cast<uint32_t>(parts.size());
		payload.LODThresholdCount = static_cast<uint32_t>(lodThresholds.size());
		payload.Topology = static_cast<uint32_t>(mesh->mTopology);
		payload.HasLODs = mesh->mHasLODs ? 1u : 0u;
		payload.IsAnimated = mesh->mIsAnimated ? 1u : 0u;
		payload.Instanced = mesh->mInstanced ? 1u : 0u;
		payload.MaxNrOfIntanceObjects = mesh->mMaxNrOfInstanceObjects;
		out.write(reinterpret_cast<const char*>(&payload), sizeof(payload));

		// LOD Threshold
		if (!lodThresholds.empty())
			out.write(reinterpret_cast<const char*>(lodThresholds.data()), lodThresholds.size() * sizeof(float));

		// LOD Groups
		for (const auto& lod : lodGroups)
		{
			uint32_t vertexCount = static_cast<uint32_t>(lod->Vertices.size());
			uint32_t indexCount = static_cast<uint32_t>(lod->Indices.size());
			uint32_t submeshCount = static_cast<uint32_t>(lod->Submeshes.size());

			out.write(reinterpret_cast<const char*>(&vertexCount), sizeof(vertexCount));
			out.write(reinterpret_cast<const char*>(&indexCount), sizeof(indexCount));
			out.write(reinterpret_cast<const char*>(&submeshCount), sizeof(submeshCount));

			// Vertex is a 5 contiguous XMFLOATn members, write it all in one go, no padding or nothing is in the Vertex.
			if (vertexCount > 0)
				out.write(reinterpret_cast<const char*>(lod->Vertices.data()), vertexCount * sizeof(Vertex));

			if (indexCount > 0)
				out.write(reinterpret_cast<const char*>(lod->Indices.data()), indexCount * sizeof(uint32_t));

			for (const Submesh& submesh : lod->Submeshes)
			{
				TAssetMeshSubmesh rec;
				rec.BaseVertex = submesh.BaseVertex;
				rec.BaseIndex = submesh.BaseIndex;
				rec.IndexCount = submesh.IndexCount;
				rec.VertexCount = submesh.VertexCount;
				rec.PartIndex = submesh.PartIndex;
				rec.MaterialHandle = static_cast<uint64_t>(submesh.MaterialHandle);

				out.write(reinterpret_cast<const char*>(&rec), sizeof(rec));
			}
		}

		// Parts
		for (const Part& part : parts)
		{
			WriteString(out, part.Name);

			TAssetMeshPartHeader ph;
			ph.RestTranslation = part.RestTranslation;
			ph.RestRotation = part.RestRotation;
			ph.RestScale = part.RestScale;
			ph.RestTransformCaptured = part.RestTransformCaptured ? 1u : 0u;
			ph.IsAnimated = part.IsAnimated ? 1u : 0u;
			ph.LODTransformCount = static_cast<uint32_t>(part.LODTransforms.size());
			ph.LODAnimationCount = static_cast<uint32_t>(part.LODAnimations.size());
			out.write(reinterpret_cast<const char*>(&ph), sizeof(ph));

			// Per LOD node transform
			for (const PartLODTransform& xf : part.LODTransforms)
			{
				TAssetMeshPartLODTransform rec;
				rec.LocalTranslation = xf.LocalTranslation;
				rec.LocalRotation = xf.LocalRotation;
				rec.LocalScale = xf.LocalScale;
				rec.Captured = xf.Captured ? 1u: 0u;

				// We don't store XMMATRIX here due to it being SIMD-backed, instead storing it as XMFLOAT4X4
				DirectX::XMStoreFloat4x4(&rec.Parent, xf.Parent);

				out.write(reinterpret_cast<const char*>(&rec), sizeof(rec));
			}

			// Per LOD Animation maps
			for (const auto& lodAnims : part.LODAnimations)
			{
				uint32_t entryCount = static_cast<uint32_t>(lodAnims.size());
				out.write(reinterpret_cast<const char*>(&entryCount), sizeof(entryCount));

				for (const auto& [animName, anim] : lodAnims)
				{
					WriteString(out, animName);

					TAssetMeshAnimation ar;
					if (anim)
					{
						ar.Duration = anim->Duration;
						ar.SampleCount = anim->SampleCount;
						ar.TranslationSampleCount = anim->TranslationSampleCount;
						ar.RotationSampleCount = anim->RotationSampleCount;
						ar.ScaleSampleCount = anim->ScaleSampleCount;
						ar.TranslationSize = anim->TranslationBuffer.Size;
						ar.TranslationTimestampSize = anim->TranslationTimestampBuffer.Size;
						ar.RotationSize = anim->RotationBuffer.Size;
						ar.RotationTimestampSize = anim->RotationTimestampBuffer.Size;
						ar.ScaleSize = anim->ScaleBuffer.Size;
						ar.ScaleTimestampSize = anim->ScaleTimestampBuffer.Size;
					}
					out.write(reinterpret_cast<const char*>(&ar), sizeof(ar));

					WriteString(out, anim ? anim->Name : std::string());

					if (anim)
					{
						WriteBuffer(out, anim->TranslationBuffer);
						WriteBuffer(out, anim->TranslationTimestampBuffer);
						WriteBuffer(out, anim->RotationBuffer);
						WriteBuffer(out, anim->RotationTimestampBuffer);
						WriteBuffer(out, anim->ScaleBuffer);
						WriteBuffer(out, anim->ScaleTimestampBuffer);
					}
				}
			}
		}

		TOAST_CORE_INFO("AssetSerializer: Baked Mesh '%s' (%u LODs, %u parts) -> '%s'", mesh->GetFilePath().c_str(), payload.LODGroupCount, payload.PartCount, outputPath.string().c_str());

		return out.good();
	}

	bool AssetSerializer::SerializeStyleSheet(AssetHandle handle, const Ref<StyleSheet>& sheet, const std::filesystem::path& outputPath)
	{
		TOAST_PROFILE_FUNCTION();

		if (!sheet)
		{
			TOAST_CORE_ERROR("AssetSerializer: Cannot serialize a null StyleSheet");
			return false;
		}

		std::filesystem::create_directories(outputPath.parent_path());

		std::ofstream out(outputPath, std::ios::binary);
		if (!out.is_open())
		{
			TOAST_CORE_ERROR("AssetSerializer: Could not open '%s' for writing", outputPath);
			return false;
		}

		// --- Header ---
		TAssetHeader header;
		header.Magic = TASSET_MAGIC;
		header.AssetType = static_cast<uint16_t>(AssetType::StyleSheet);
		header.Version = TASSET_VERSION;
		header.Handle = static_cast<uint64_t>(handle);
		out.write(reinterpret_cast<const char*>(&header), sizeof(header));

		const StyleBlock& block = sheet->GetBlock();
		out.write(reinterpret_cast<const char*>(&block), sizeof(StyleBlock));

		TOAST_CORE_INFO("AssetSerializer: Baked StyleSheet (handle: %llu) -> '%s'", (uint64_t)handle, outputPath.string().c_str());

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

		auto texture = CreateRef<Texture2D>(static_cast<DXGI_FORMAT>(payload.Format), static_cast<DXGI_FORMAT>(payload.SRVFormat), payload.Width, payload.Height, D3D11_USAGE_DEFAULT, static_cast<D3D11_BIND_FLAG>(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET), 1u, 0u, pixelData.data(), payload.RowPitch);

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

	Ref<Material> AssetSerializer::DeserializeMaterial(const std::filesystem::path& inputPath)
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
		if (header.AssetType != static_cast<uint16_t>(AssetType::Material))
		{
			TOAST_CORE_ERROR("AssetSerializer: Expected Material but got type %u in '%s'",
				header.AssetType, inputPath.string().c_str());
			return nullptr;
		}
		if (header.Version > TASSET_VERSION)
		{
			TOAST_CORE_ERROR("AssetSerializer: Unsupported version %u in '%s' (max %u)",
				header.Version, inputPath.string().c_str(), TASSET_VERSION);
			return nullptr;
		}

		TAssetMaterialPayload payload;
		in.read(reinterpret_cast<char*>(&payload), sizeof(payload));
		if (!in.good())
		{
			TOAST_CORE_ERROR("AssetSerializer: Failed reading material payload from '%s'", inputPath.string().c_str());
			return nullptr;
		}
		in.close();

		std::string name = inputPath.stem().string();  // or read a baked name block if Step 4 added one

		return CreateRef<Material>(name, payload.Albedo, payload.Emission, payload.Metalness, payload.Roughness, payload.UseAlbedo != 0, payload.UseNormal != 0, payload.UseMetalRough != 0, AssetHandle(payload.AlbedoHandle), AssetHandle(payload.NormalHandle), AssetHandle(payload.MetalRoughHandle));
	}

	Ref<Mesh> AssetSerializer::DeserializeMesh(const std::filesystem::path& inputPath)
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

		if (header.AssetType != static_cast<uint16_t>(AssetType::Mesh))
		{
			TOAST_CORE_ERROR("AssetSerializer: Expect Mesh but got type %u in '%s'", header.AssetType, inputPath.string().c_str());
			return nullptr;
		}

		if (header.Version > TASSET_VERSION) 
		{
			TOAST_CORE_ERROR("AssetSerializer: Unsupported version %u in '%s' (max %u)", header.Version, inputPath.string().c_str(), TASSET_VERSION);
			return nullptr;
		}

		// Payload
		TAssetMeshPayload payload;
		in.read(reinterpret_cast<char*>(&payload), sizeof(payload));

		std::vector<float> lodThresholds(payload.LODThresholdCount);
		if (payload.LODThresholdCount > 0)
			in.read(reinterpret_cast<char*>(lodThresholds.data()), payload.LODThresholdCount * sizeof(float));

		// LOD Groups
		std::vector<Ref<LODGroup>> lodGroups;
		lodGroups.reserve(payload.LODGroupCount);

		for (uint32_t g = 0; g < payload.LODGroupCount; ++g)
		{
			auto lod = CreateRef<LODGroup>();

			uint32_t vertexCount = 0;
			uint32_t indexCount = 0;
			uint32_t submeshCount = 0;

			in.read(reinterpret_cast<char*>(&vertexCount), sizeof(vertexCount));
			in.read(reinterpret_cast<char*>(&indexCount), sizeof(indexCount));
			in.read(reinterpret_cast<char*>(&submeshCount), sizeof(submeshCount));

			lod->VertexCount = vertexCount;
			lod->IndexCount = indexCount;

			lod->Vertices.resize(vertexCount);
			if(vertexCount > 0)
				in.read(reinterpret_cast<char*>(lod->Vertices.data()), vertexCount * sizeof(Vertex));

			lod->Indices.resize(indexCount);
			if (indexCount > 0)
				in.read(reinterpret_cast<char*>(lod->Indices.data()), indexCount * sizeof(uint32_t));

			lod->Submeshes.reserve(submeshCount);
			for (uint32_t s = 0; s < submeshCount; ++s) 
			{
				TAssetMeshSubmesh rec;
				in.read(reinterpret_cast<char*>(&rec), sizeof(rec));

				Submesh submesh;
				submesh.BaseVertex = rec.BaseVertex;
				submesh.BaseIndex = rec.BaseIndex;
				submesh.IndexCount = rec.IndexCount;
				submesh.VertexCount = rec.VertexCount;
				submesh.PartIndex = rec.PartIndex;
				submesh.MaterialHandle = AssetHandle(rec.MaterialHandle);
				lod->Submeshes.push_back(submesh);
			}

			lodGroups.push_back(lod);
		}

		// Parts
		std::vector<Part> parts;
		parts.reserve(payload.PartCount);

		for (uint32_t p; p < payload.PartCount; ++p)
		{
			Part part;
			part.Name = ReadString(in);

			TAssetMeshPartHeader ph;
			in.read(reinterpret_cast<char*>(&ph), sizeof(ph));

			part.RestTranslation = ph.RestTranslation;
			part.RestRotation = ph.RestRotation;
			part.RestScale = ph.RestScale;
			part.RestTransformCaptured = ph.RestTransformCaptured != 0;
			part.IsAnimated = ph.IsAnimated != 0;

			part.LODTransforms.reserve(ph.LODTransformCount);
			for (uint32_t l; l < ph.LODTransformCount; ++l)
			{
				TAssetMeshPartLODTransform rec;
				in.read(reinterpret_cast<char*>(&rec), sizeof(rec));

				PartLODTransform xf;
				xf.LocalTranslation = rec.LocalTranslation;
				xf.LocalRotation = rec.LocalRotation;
				xf.LocalScale = rec.LocalScale;
				xf.Parent = DirectX::XMLoadFloat4x4(&rec.Parent);
				xf.Captured = rec.Captured != 0;

				part.LODTransforms.push_back(xf);
			}

			part.LODAnimations.reserve(ph.LODAnimationCount);
			for (uint32_t l; l < ph.LODTransformCount; ++l)
			{
				uint32_t entryCount = 0;
				in.read(reinterpret_cast<char*>(&entryCount), sizeof(entryCount));

				for (uint32_t e; e < entryCount; ++e)
				{
					std::string animName = ReadString(in);

					TAssetMeshAnimation ar;
					in.read(reinterpret_cast<char*>(&ar), sizeof(ar));

					auto anim = CreateRef<Animation>();
					anim->Name = ReadString(in);
					anim->Duration = ar.Duration;
					anim->SampleCount = ar.SampleCount;
					anim->TranslationSampleCount = ar.TranslationSampleCount;
					anim->RotationSampleCount = ar.RotationSampleCount;
					anim->ScaleSampleCount = ar.ScaleSampleCount;

					ReadBuffer(in, anim->TranslationBuffer, ar.TranslationSize);
					ReadBuffer(in, anim->TranslationTimestampBuffer, ar.TranslationTimestampSize);
					ReadBuffer(in, anim->RotationBuffer, ar.RotationSize);
					ReadBuffer(in, anim->RotationTimestampBuffer, ar.RotationTimestampSize);
					ReadBuffer(in, anim->ScaleBuffer, ar.ScaleSize);
					ReadBuffer(in, anim->ScaleTimestampBuffer, ar.ScaleTimestampSize);

					part.LODAnimations[l][animName] = anim;
				}
			}

			parts.push_back(std::move(part));
		}

		if (!in.good())
		{
			TOAST_CORE_ERROR("AssetSerializer: Failed reading mesh data from '%s'", inputPath.string().c_str());
			return nullptr;
		}
		in.close();

		auto mesh = CreateRef<Mesh>(std::move(lodGroups), std::move(parts), std::move(lodThresholds), static_cast<PrimitiveTopology>(payload.Topology), payload.HasLODs != 0, payload.IsAnimated != 0, payload.Instanced != 0, payload.MaxNrOfIntanceObjects, inputPath.string());

		TOAST_CORE_INFO("AssetSerializer: Loaded Mesh from '%s' (%u LODs, %u parts)", inputPath.string().c_str(), payload.LODGroupCount, payload.PartCount);

		return mesh;
	}

	Ref<StyleSheet> AssetSerializer::DeserializeStyleSheet(const std::filesystem::path& inputPath)
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
			TOAST_CORE_ERROR("AssetSerializer: Expected StyleSheet but got type %u in '%s'", header.AssetType, inputPath.string().c_str());
			return nullptr;
		}
		if (header.Version > TASSET_VERSION)
		{
			TOAST_CORE_ERROR("AssetSerializer: Unsupported version %u in '%s' (max %u)", header.Version, inputPath.string().c_str(), TASSET_VERSION);
			return nullptr;
		}

		StyleBlock block;
		in.read(reinterpret_cast<char*>(&block), sizeof(StyleBlock));

		if (!in.good())
		{
			TOAST_CORE_ERROR("AssetSerializer: Failed to read StyleBlock from '%s'", inputPath.string().c_str());
			return nullptr;
		}
		in.close();

		auto sheet = CreateRef<StyleSheet>();
		sheet->SetBlock(block);

		TOAST_CORE_INFO("AssetSerializer: Loaded StyleSheet from '%s'", inputPath.string().c_str());

		return sheet;
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