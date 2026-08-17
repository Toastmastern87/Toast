#include "tpch.h"
#define CGLTF_IMPLEMENTATION
#include "Mesh.h"

#include "Toast/Assets/AssetManager.h"
#include "Toast/Assets/AssetSerializer.h"

#include <algorithm>
#include <filesystem>
#include <math.h>

namespace Toast {

	template<typename T>
	static void LoadAttribute(cgltf_accessor* attribute, T Vertex::* member, std::vector<Vertex>& vertices, uint32_t baseVertex)
	{
		const size_t stride = attribute->buffer_view->stride ? attribute->buffer_view->stride : sizeof(T);
		const uint8_t* dataPtr = reinterpret_cast<const uint8_t*>(attribute->buffer_view->buffer->data) + attribute->buffer_view->offset + attribute->offset;

		for (size_t i = 0; i < attribute->count; ++i)
		{
			const T* data = reinterpret_cast<const T*>(dataPtr + i * stride);
			vertices[baseVertex + i].*member = *data;
		}
	}

	static void LoadAttribute(cgltf_accessor* attribute, cgltf_attribute_type attributetype, std::vector<Vertex>& vertices, uint32_t baseVertex)
	{
		switch (attributetype)
		{
		case cgltf_attribute_type_position:
			LoadAttribute(attribute, &Vertex::Position, vertices, baseVertex);
			break;
		case cgltf_attribute_type_normal:
			LoadAttribute(attribute, &Vertex::Normal, vertices, baseVertex);
			break;
		case cgltf_attribute_type_tangent:
			LoadAttribute(attribute, &Vertex::Tangent, vertices, baseVertex);
			break;
		case cgltf_attribute_type_texcoord:
			LoadAttribute(attribute, &Vertex::Texcoord, vertices, baseVertex);
			break;
		}
	}

	// GetPartBaseName strips the trailing LOD digit; the digit itself is the LOD index.
	static bool GetNodeLODIndex(const std::string& nodeName, uint32_t& outLOD)
	{
		if (nodeName.empty()) return false;
		unsigned char last = static_cast<unsigned char>(nodeName.back());
		if (!std::isdigit(last)) return false;
		outLOD = static_cast<uint32_t>(last - '0');
		return true;
	}

	static DirectX::XMMATRIX NodeMatrixLocal(const cgltf_node* n)
	{
		if (n->has_matrix)
		{
			DirectX::XMFLOAT4X4 m;
			memcpy(&m, n->matrix, sizeof(float) * 16);
			return DirectX::XMLoadFloat4x4(&m);
		}
		else
		{
			DirectX::XMVECTOR translation = DirectX::XMVectorSet(n->has_translation ? (float)n->translation[0] : 0.0f, n->has_translation ? (float)n->translation[1] : 0.0f, n->has_translation ? (float)n->translation[2] : 0.0f, 1.0f);

			DirectX::XMVECTOR quatRot = DirectX::XMVectorSet(n->has_rotation ? (float)n->rotation[0] : 0.0f, n->has_rotation ? (float)n->rotation[1] : 0.0f, n->has_rotation ? (float)n->rotation[2] : 0.0f, n->has_rotation ? (float)n->rotation[3] : 1.0f);

			DirectX::XMVECTOR scale = DirectX::XMVectorSet(n->has_scale ? (float)n->scale[0] : 1.0f, n->has_scale ? (float)n->scale[1] : 1.0f, n->has_scale ? (float)n->scale[2] : 1.0f, 0.0f);

			return DirectX::XMMatrixScalingFromVector(scale) * DirectX::XMMatrixRotationQuaternion(quatRot) * DirectX::XMMatrixTranslationFromVector(translation);
		}
	}

	static bool DecomposeTransform(const DirectX::XMMATRIX& matrix,	DirectX::XMFLOAT3& outTranslation, DirectX::XMFLOAT4& outRotation, DirectX::XMFLOAT3& outScale)
	{
		DirectX::XMVECTOR scale;
		DirectX::XMVECTOR rotationQuat;
		DirectX::XMVECTOR translation;

		if (!DirectX::XMMatrixDecompose(&scale, &rotationQuat, &translation, matrix))
			return false;

		DirectX::XMStoreFloat3(&outScale, scale);
		DirectX::XMStoreFloat4(&outRotation, rotationQuat);
		DirectX::XMStoreFloat3(&outTranslation, translation);
		return true;
	}

	// Returns the index of the last keyframe at or before `time`, so that
	// keys [i] and [i+1] bracket it. Clamps to the final key when `time`
	// is at or past the end of the track.
	static uint32_t FindKeyIndex(const float* timestamps, uint32_t count, float time)
	{
		if (count == 0) return 0;

		// Stop at the first key whose successor is still ahead of us.
		for (uint32_t i = 0; i + 1 < count; ++i)
			if (time < timestamps[i + 1]) return i;

		return count - 1;   // past the last key — caller holds the final value
	}

	// Fraction (0..1) of the way from key [i] to key [i+1] at `time`.
	// This is the blend factor handed to lerp/slerp.
	static float KeyAlpha(const float* ts, uint32_t count, uint32_t i, float time)
	{
		if (i + 1 >= count) return 0.0f;    // no successor — nothing to blend toward

		float span = ts[i + 1] - ts[i];
		if (span <= 0.0f) return 0.0f;      // duplicate timestamps: avoid divide-by-zero

		return std::clamp((time - ts[i]) / span, 0.0f, 1.0f);
	}

	// Samples a vector3 track at `time`, slerping between the two
	// keyframes that bracket it. Returns `fallback` (the rest pose) when
	// the track has no keys.
	static DirectX::XMFLOAT3 SampleVec3(const Buffer& values, const Buffer& timestamps, uint32_t count, float time, const DirectX::XMFLOAT3& fallback)
	{
		if (count == 0) return fallback;

		const auto* v = values.As<DirectX::XMFLOAT3>();
		const auto* ts = timestamps.As<float>();

		if (count == 1) return v[0];

		uint32_t i = FindKeyIndex(ts, count, time);
		if (i + 1 >= count) return v[count - 1];

		float a = KeyAlpha(ts, count, i, time);
		DirectX::XMFLOAT3 out;
		DirectX::XMStoreFloat3(&out, DirectX::XMVectorLerp(DirectX::XMLoadFloat3(&v[i]), DirectX::XMLoadFloat3(&v[i + 1]), a));

		return out;
	}

	// Samples a quaternion track at `time`, slerping between the two
	// keyframes that bracket it. Returns `fallback` (the rest pose) when
	// the track has no keys.
	static DirectX::XMFLOAT4 SampleQuat(const Buffer& values, const Buffer& timestamps, uint32_t count, float time, const DirectX::XMFLOAT4& fallback)
	{
		if (count == 0) 
			return fallback;

		const auto* v = values.As<DirectX::XMFLOAT4>();
		const auto* ts = timestamps.As<float>();

		if (count == 1) 
			return v[0];

		uint32_t i = FindKeyIndex(ts, count, time);
		if (i + 1 >= count) 
			return v[count - 1];

		float a = KeyAlpha(ts, count, i, time);

		DirectX::XMVECTOR q = DirectX::XMQuaternionSlerp(DirectX::XMLoadFloat4(&v[i]), DirectX::XMLoadFloat4(&v[i + 1]), a);

		DirectX::XMFLOAT4 out;
		DirectX::XMStoreFloat4(&out, DirectX::XMQuaternionNormalize(q));
		return out;
	}

	static std::string GetPartBaseName(const std::string& name)
	{
		if (name.empty())
			return {};
		if (std::isdigit(static_cast<unsigned char>(name.back())))
			return name.substr(0, name.size() - 1);
		return name;
	}

	Mesh::Mesh()
	{
		mLODGroups.emplace_back(CreateRef<LODGroup>());

		TOAST_CORE_INFO("Mesh Initialized!");
	}

	Mesh::Mesh(const std::string& filePath, Vector3 colorOverride, bool isInstanced, uint32_t maxNrOfInstanceObjects)
		: mFilePath(filePath), mColorOverride(colorOverride), mMaxNrOfInstanceObjects(maxNrOfInstanceObjects)
	{
		mInstanced = isInstanced;

		cgltf_options options = { };
		cgltf_data* data = NULL;
		cgltf_result result = cgltf_parse_file(&options, filePath.c_str(), &data);

		if (result == cgltf_result_success)
		{
			TOAST_CORE_INFO("Opening File: %s", filePath.c_str());

			for (size_t i = 0; i < data->nodes_count; ++i)
			{

				const cgltf_node* node = &data->nodes[i];
				std::string nodeName(node->name);
				if (nodeName.find("LOD") != std::string::npos) 
					node->children_count > 0 ? mHasLODs = true : mHasLODs = false;
			}

			if (!mHasLODs)
			{
				TOAST_CORE_INFO("No LOD Groups found in %s, load Mesh without LODs", filePath.c_str());
				LoadMesh(data);
			}
			else
			{
				TOAST_CORE_INFO("LOD Groups found in %s, load Mesh with LODs", filePath.c_str());
				LoadMeshWithLODs(data);
			}

			cgltf_free(data);
		}
	}

	Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const DirectX::XMMATRIX& transform)
	{
		mLODGroups.emplace_back(CreateRef<LODGroup>());

		Submesh submesh;
		submesh.BaseVertex = 0;
		submesh.BaseIndex = 0;
		submesh.IndexCount = (uint32_t)indices.size();
		submesh.MaterialName = "Standard";

		mLODGroups[0]->Submeshes.push_back(submesh);

		mLODGroups[0]->Vertices = vertices;
		mLODGroups[0]->Indices = indices;

		mLODGroups[0]->VBuffer = CreateRef<VertexBuffer>(&mLODGroups[0]->Vertices[0], (sizeof(Vertex) * (uint32_t)mLODGroups[0]->Vertices.size()), (uint32_t)mLODGroups[0]->Vertices.size(), 0);
		mLODGroups[0]->IBuffer = CreateRef<IndexBuffer>(&mLODGroups[0]->Indices[0], (uint32_t)mLODGroups[0]->Indices.size());
	}

	void Mesh::LoadMesh(cgltf_data* data)
	{
		uint32_t vertexCount = 0;
		uint32_t indexCount = 0;

		cgltf_result result;
		cgltf_options options = {};

		result = cgltf_load_buffers(&options, data, mFilePath.c_str());

		mIsAnimated = data->animations_count > 0;

		mLODGroups.emplace_back(CreateRef<LODGroup>());

		DirectX::XMFLOAT3 translation;
		DirectX::XMFLOAT4 rotation;
		DirectX::XMFLOAT3 scale;
		//TOAST_CORE_INFO("data->accessors_count: %d", data->accessors_count);
		for (unsigned m = 0; m < data->meshes_count; m++)
		{
			for (unsigned int p = 0; p < data->meshes[m].primitives_count; p++)
			{
				if (data->meshes[m].primitives[p].type != cgltf_primitive_type_triangles)
					continue;

				Submesh& submesh = mLODGroups[0]->Submeshes.emplace_back();
				submesh.MaterialName = std::string(data->meshes[m].primitives[p].material->name);

				for (unsigned int a = 0; a < data->meshes[m].primitives[p].attributes_count; a++)
				{
					cgltf_accessor* attribute = data->meshes[m].primitives[p].attributes[a].data;

					if (a == 0) 
					{
						submesh.BaseVertex = vertexCount;
						submesh.VertexCount = static_cast<uint32_t>(attribute->count);
						vertexCount += submesh.VertexCount;
						mLODGroups[0]->Vertices.resize(vertexCount);
					}

					LoadAttribute(attribute, data->meshes[m].primitives[p].attributes[a].type, mLODGroups[0]->Vertices, submesh.BaseVertex);
				}

				// Color override
				if (mColorOverride.z != 0.0)
				{
					for (auto& vertex : mLODGroups[0]->Vertices)
						vertex.Color = { (float)mColorOverride.x, (float)mColorOverride.y, (float)mColorOverride.z };
				}

				// INDICES
				if (data->meshes[m].primitives[p].indices != NULL)
				{
					cgltf_accessor* indexAccessor = data->meshes[m].primitives[p].indices;
					const uint16_t* indices = reinterpret_cast<const uint16_t*>(reinterpret_cast<const uint8_t*>(indexAccessor->buffer_view->buffer->data) + indexAccessor->buffer_view->offset + indexAccessor->offset);

					submesh.IndexCount = indexAccessor->count;
					submesh.BaseIndex = indexCount;
					indexCount += submesh.IndexCount;
					mLODGroups[0]->Indices.resize(indexCount);

					for (size_t i = 0; i < indexAccessor->count; ++i)
					{
						cgltf_size idx = cgltf_accessor_read_index(indexAccessor, i);
						mLODGroups[0]->Indices[submesh.BaseIndex + i] = (uint32_t)idx + submesh.BaseVertex;
					}
				}

				TOAST_CORE_INFO("Mesh  loaded with material '%s', number of indices: %d", submesh.MaterialName.c_str(), submesh.IndexCount);
			}
		}

		// MATERIALS
		TOAST_CORE_INFO("Number of materials: %d", data->materials_count);
		for (int m = 0; m < data->materials_count; m++) 
		{
			TOAST_CORE_INFO("Material name: %s", data->materials[m].name);

			std::string materialName(data->materials[m].name);
				
			if (data->materials[m].has_pbr_metallic_roughness)
			{
				auto material = CreateRef<Material>(materialName);

				// ALBEDO
				DirectX::XMFLOAT4 albedoColor;
				albedoColor.x = data->materials[m].pbr_metallic_roughness.base_color_factor[0];
				albedoColor.y = data->materials[m].pbr_metallic_roughness.base_color_factor[1];
				albedoColor.z = data->materials[m].pbr_metallic_roughness.base_color_factor[2];
				albedoColor.w = data->materials[m].pbr_metallic_roughness.base_color_factor[3];

				bool hasAlbedoMap = data->materials[m].pbr_metallic_roughness.base_color_texture.texture;
				int useAlbedoMap = 0;
				if (hasAlbedoMap)
				{
					std::string texPath(data->materials[m].pbr_metallic_roughness.base_color_texture.texture->image->uri);
					std::filesystem::path path = mFilePath;
					auto parentPath = path.parent_path();
					std::string texturePath = parentPath.string();
					std::string completePath = texturePath.append("\\").append(texPath.c_str());
					albedoColor = { 1.0f, 1.0f, 1.0f, 1.0f };
					useAlbedoMap = 1;
					material->SetAlbedolAssetHandle(AssetManager::ImportExternalAsset(completePath, "Textures"));
					//TOAST_CORE_INFO("Albedo map found: %s", completePath.c_str());	
				}

				material->SetAlbedo(albedoColor);
				material->SetUseAlbedo(useAlbedoMap);

				// NORMAL
				bool hasNormalMap = data->materials[m].normal_texture.texture;
				int useNormalMap = 0;
				if (hasNormalMap)
				{
					std::string texPath(data->materials[m].normal_texture.texture->image->uri);
					std::filesystem::path path = mFilePath;
					auto parentPath = path.parent_path();
					std::string texturePath = parentPath.string();
					std::string completePath = texturePath.append("\\").append(texPath.c_str());
					useNormalMap = 1;
					material->SetNormalAssetHandle(AssetManager::ImportExternalAsset(completePath, "Textures"));
					//TOAST_CORE_INFO("Normal map found: %s", completePath.c_str());
				}
				material->SetUseNormal(useNormalMap);

				// METALLNESS ROUGHNESS
				bool hasMetalRoughMap = data->materials[m].pbr_metallic_roughness.metallic_roughness_texture.texture;
				int useMetalRoughMap = 0;
				float metalness = 0.0f;
				float roughness = 0.0f;
				if (hasMetalRoughMap)
				{
					std::string texPath(data->materials[m].pbr_metallic_roughness.metallic_roughness_texture.texture->image->uri);
					std::filesystem::path path = mFilePath;
					auto parentPath = path.parent_path();
					std::string texturePath = parentPath.string();
					std::string completePath = texturePath.append("\\").append(texPath.c_str());
					metalness = 1.0f;
					useMetalRoughMap = 1;
					material->SetMetalRoughAssetHandle(AssetManager::ImportExternalAsset(completePath, "Textures"));
				}
				else
				{
					//TOAST_CORE_INFO("data->materials[m].pbr_metallic_roughness.metallic_factor: %f", data->materials[m].pbr_metallic_roughness.metallic_factor);
					//TOAST_CORE_INFO("data->materials[m].pbr_metallic_roughness.roughness_factor: %f", data->materials[m].pbr_metallic_roughness.roughness_factor);
					metalness = data->materials[m].pbr_metallic_roughness.metallic_factor;
					roughness = data->materials[m].pbr_metallic_roughness.roughness_factor;
				}
				material->SetMetalness(metalness);
				material->SetRoughness(roughness);
				material->SetUseMetalRough(useMetalRoughMap);

				// Write the .tmtl ourselves (no source file exists yet), then register it
				// like any in-project asset. Mirrors the texture flow, minus the "file
				// already exists" assumption.
				std::filesystem::path relativePath = std::filesystem::path("Materials") / (materialName + ".tmtl");
				std::filesystem::path fullPath = AssetManager::GetAssetDirectory() / relativePath;

				material->SaveToFile(fullPath);                            // create the .tmtl from glTF data
				AssetHandle materialHandle = AssetManager::ImportAsset(relativePath);  // register ? handle

				// Make this instance the live asset for its handle.
				if (AssetEntry* entry = AssetManager::GetEntry(materialHandle))
					entry->Resource = material;

				mMaterials.insert({ data->materials[m].name, material });
			}
		}
		TOAST_CORE_INFO("Number of materials loaded: %d", mMaterials.size());

		// ANIMATIONS
		TOAST_CORE_INFO("Number of animations in mesh: %d", data->animations_count);
		for (unsigned int a = 0; a < data->animations_count; a++)
		{
			Ref<Animation> animation = CreateRef<Animation>();

			//TOAST_CORE_INFO("data->animations[a].samplers_count: %d", data->animations[a].samplers_count);
			for (unsigned int s = 0; s < data->animations[a].samplers_count; s++) 
			{
				animation->SampleCount = data->animations[a].samplers[s].input->count;
				animation->Duration = data->animations[a].samplers[s].input->max[0];
				animation->TranslationBuffer = Buffer(data->animations[a].samplers[s].output->buffer_view->size);
				animation->TranslationBuffer.Write((uint8_t*)(data->animations[a].samplers[s].output->buffer_view->buffer->data) + data->animations[a].samplers[s].output->buffer_view->offset, data->animations[a].samplers[s].output->buffer_view->size);
			}

			//TOAST_CORE_INFO("data->animations[a].channels_count: %d", data->animations[a].channels_count);
			//for (unsigned int c = 0; c < data->animations[a].channels_count; c++)
				//animation->AnimationChannel = data->animations[a].channels[c];

			//for (auto& submesh : mLODGroups[0]->Submeshes)
			//{
			//	if (strcmp(data->animations[a].channels->target_node->name, submesh.MeshName.c_str()) == 0)
			//	{
			//		std::string name = std::string(data->animations[a].name);
			//		animation->Name = name;
			//		submesh.IsAnimated = true;
			//		submesh.Animations[name] = animation;
			//		TOAST_CORE_INFO("Submesh %s have an animation named %s, its now added to the submesh animation map", submesh.MeshName.c_str(), animation->Name.c_str());
			//	}
			//}

		}
			
		mLODGroups[0]->VBuffer = CreateRef<VertexBuffer>(mLODGroups[0]->Vertices.data(), (sizeof(Vertex) * (uint32_t)mLODGroups[0]->Vertices.size()), (uint32_t)mLODGroups[0]->Vertices.size(), 0);

		if(mInstanced && mMaxNrOfInstanceObjects > 0)
			mLODGroups[0]->InstancedVBuffer = CreateRef<VertexBuffer>((sizeof(DirectX::XMFLOAT3) * mMaxNrOfInstanceObjects), mMaxNrOfInstanceObjects, 1);

		mLODGroups[0]->IBuffer = CreateRef<IndexBuffer>(mLODGroups[0]->Indices.data(), (uint32_t)mLODGroups[0]->Indices.size());
	}

	void Mesh::LoadMeshWithLODs(cgltf_data* data)
	{
		cgltf_result result;
		cgltf_options options = {};

		result = cgltf_load_buffers(&options, data, mFilePath.c_str());

		for (size_t i = 0; i < data->nodes_count; ++i)
		{
			const cgltf_node* node = &data->nodes[i];
			std::string nodeName(node->name);

			if (node->name && strstr(node->name, "CraneWireLeft"))
				TOAST_CORE_INFO("NODE '%s' has_matrix=%d has_rotation=%d rot=(%.4f,%.4f,%.4f,%.4f)",
					node->name, node->has_matrix ? 1 : 0, node->has_rotation ? 1 : 0,
					node->has_rotation ? (float)node->rotation[0] : 0.0f,
					node->has_rotation ? (float)node->rotation[1] : 0.0f,
					node->has_rotation ? (float)node->rotation[2] : 0.0f,
					node->has_rotation ? (float)node->rotation[3] : 1.0f);
			
			if (nodeName.find("LOD") != std::string::npos)
			{
				mLODGroups.emplace_back(CreateRef<LODGroup>());
				Ref<LODGroup> currentLOD = mLODGroups.back();
				uint32_t lodIndex = (uint32_t)mLODGroups.size() - 1;
				uint32_t vertexCount = 0;
				uint32_t indexCount = 0;

				DirectX::XMMATRIX identity = DirectX::XMMatrixIdentity();

				for (cgltf_size j = 0; j < node->children_count; ++j)
					ProcessLODNode(node->children[j], currentLOD, lodIndex, identity, vertexCount, indexCount);

				currentLOD->VBuffer = CreateRef<VertexBuffer>(currentLOD->Vertices.data(), (sizeof(Vertex) * (uint32_t)currentLOD->Vertices.size()), (uint32_t)currentLOD->Vertices.size(), 0);

				if (mInstanced && mMaxNrOfInstanceObjects > 0)
					currentLOD->InstancedVBuffer = CreateRef<VertexBuffer>((sizeof(DirectX::XMFLOAT3) * mMaxNrOfInstanceObjects), mMaxNrOfInstanceObjects, 1);

				currentLOD->IBuffer = CreateRef<IndexBuffer>(currentLOD->Indices.data(), (uint32_t)currentLOD->Indices.size());
			}
		}

		// MATERIALS
		TOAST_CORE_INFO("Number of materials: %d", data->materials_count);
		for (int m = 0; m < data->materials_count; m++)
		{
			TOAST_CORE_INFO("Material name: %s", data->materials[m].name);

			std::string materialName(data->materials[m].name);
			if (data->materials[m].has_pbr_metallic_roughness)
			{
				auto material = CreateRef<Material>(materialName);

				// ALBEDO
				DirectX::XMFLOAT4 albedoColor;
				albedoColor.x = data->materials[m].pbr_metallic_roughness.base_color_factor[0];
				albedoColor.y = data->materials[m].pbr_metallic_roughness.base_color_factor[1];
				albedoColor.z = data->materials[m].pbr_metallic_roughness.base_color_factor[2];
				albedoColor.w = data->materials[m].pbr_metallic_roughness.base_color_factor[3];

				bool hasAlbedoMap = data->materials[m].pbr_metallic_roughness.base_color_texture.texture;
				int useAlbedoMap = 0;
				if (hasAlbedoMap)
				{
					std::string texPath(data->materials[m].pbr_metallic_roughness.base_color_texture.texture->image->uri);
					std::filesystem::path path = mFilePath;
					auto parentPath = path.parent_path();
					std::string texturePath = parentPath.string();
					std::string completePath = texturePath.append("\\").append(texPath.c_str());
					albedoColor = { 1.0f, 1.0f, 1.0f, 1.0f };
					useAlbedoMap = 1;
					material->SetAlbedolAssetHandle(AssetManager::ImportExternalAsset(completePath, "Textures"));
					TOAST_CORE_INFO("Albedo map found for %s: %s", materialName.c_str(), completePath.c_str());
				}

				material->SetAlbedo(albedoColor);
				material->SetUseAlbedo(useAlbedoMap);

				// NORMAL
				bool hasNormalMap = data->materials[m].normal_texture.texture;
				int useNormalMap = 0;
				if (hasNormalMap)
				{
					std::string texPath(data->materials[m].normal_texture.texture->image->uri);
					std::filesystem::path path = mFilePath;
					auto parentPath = path.parent_path();
					std::string texturePath = parentPath.string();
					std::string completePath = texturePath.append("\\").append(texPath.c_str());
					useNormalMap = 1;
					material->SetNormalAssetHandle(AssetManager::ImportExternalAsset(completePath, "Textures"));
					TOAST_CORE_INFO("Normal map found for %s: %s", materialName.c_str(), completePath.c_str());
				}
				material->SetUseNormal(useNormalMap);

				// METALLNESS ROUGHNESS
				bool hasMetalRoughMap = data->materials[m].pbr_metallic_roughness.metallic_roughness_texture.texture;
				int useMetalRoughMap = 0;
				float metalness = 0.0f;
				float roughness = 0.0f;
				if (hasMetalRoughMap)
				{
					std::string texPath(data->materials[m].pbr_metallic_roughness.metallic_roughness_texture.texture->image->uri);
					std::filesystem::path path = mFilePath;
					auto parentPath = path.parent_path();
					std::string texturePath = parentPath.string();
					std::string completePath = texturePath.append("\\").append(texPath.c_str());
					metalness = 1.0f;
					useMetalRoughMap = 1;
					material->SetMetalRoughAssetHandle(AssetManager::ImportExternalAsset(completePath, "Textures"));
					TOAST_CORE_INFO("Metalness/Roughness map found for %s: %s", materialName.c_str(), completePath.c_str());
				}
				else
				{
					//TOAST_CORE_INFO("data->materials[m].pbr_metallic_roughness.metallic_factor: %f", data->materials[m].pbr_metallic_roughness.metallic_factor);
					//TOAST_CORE_INFO("data->materials[m].pbr_metallic_roughness.roughness_factor: %f", data->materials[m].pbr_metallic_roughness.roughness_factor);
					metalness = data->materials[m].pbr_metallic_roughness.metallic_factor;
					roughness = data->materials[m].pbr_metallic_roughness.roughness_factor;
				}
				material->SetMetalness(metalness);
				material->SetRoughness(roughness);
				material->SetUseMetalRough(useMetalRoughMap);

				// Write the .tmtl ourselves (no source file exists yet), then register it
				// like any in-project asset. Mirrors the texture flow, minus the "file
				// already exists" assumption.
				std::filesystem::path relativePath = std::filesystem::path("Materials") / (materialName + ".tmtl");
				std::filesystem::path fullPath = AssetManager::GetAssetDirectory() / relativePath;

				material->SaveToFile(fullPath);                            // create the .tmtl from glTF data
				AssetHandle materialHandle = AssetManager::ImportAsset(relativePath);  // register ? handle

				// Make this instance the live asset for its handle.
				if (AssetEntry* entry = AssetManager::GetEntry(materialHandle))
					entry->Resource = material;

				mMaterials.insert({ data->materials[m].name, material });
			}
		}
		TOAST_CORE_INFO("Number of materials loaded: %d", mMaterials.size());

		// ANIMATIONS
		// Helper: strip trailing _1, _2, _N suffix for the different LODs animations
		auto StripLODSuffix = [](const std::string& name) -> std::string
			{
				auto pos = name.rfind('.');
				if (pos != std::string::npos)
				{
					std::string suffix = name.substr(pos + 1);
					bool isNumber = !suffix.empty() && std::all_of(suffix.begin(), suffix.end(), ::isdigit);
					if (isNumber)
						return name.substr(0, pos);
				}
				return name;
			};

		TOAST_CORE_INFO("Number of animations in mesh: %d", data->animations_count);

		// One shared Animation ref per base name, reused across all LOD groups
		std::unordered_map<std::string, Ref<Animation>> baseAnimations;

		for (unsigned int a = 0; a < data->animations_count; a++)
		{
			mIsAnimated = true;
			std::string animName = std::string(data->animations[a].name);
			std::string baseAnimName = StripLODSuffix(animName);

			for (unsigned int c = 0; c < data->animations[a].channels_count; c++)
			{
				auto& channel = data->animations[a].channels[c];
				if (!channel.target_node) 
					continue;

				std::string targetNode = std::string(channel.target_node->name);
				std::string baseNodeName = GetPartBaseName(targetNode);

				uint32_t nodeLOD = 0;
				if (!GetNodeLODIndex(targetNode, nodeLOD))
					nodeLOD = 0;

				// Unique per animation + node + LOD, so LOD variants no longer overwrite
				std::string animKey = baseAnimName + "_" + baseNodeName + "_" + std::to_string(nodeLOD);

				float animDuration = 0.0f;
				for (unsigned int c = 0; c < data->animations[a].channels_count; c++)
				{
					auto& channel = data->animations[a].channels[c];
					if (!channel.sampler) continue;
					animDuration = std::max(animDuration, (float)channel.sampler->input->max[0]);
				}

				// Create the shared animation only once per base name
				if (baseAnimations.find(animKey) == baseAnimations.end())
				{
					Ref<Animation> animation = CreateRef<Animation>();
					animation->Name = baseAnimName;
					animation->SampleCount = channel.sampler->input->count;
					animation->Duration = animDuration;

					// Timestamp buffer
					size_t timestampSize = channel.sampler->input->count * sizeof(float);
					uint8_t* timestampData = (uint8_t*)channel.sampler->input->buffer_view->buffer->data + channel.sampler->input->buffer_view->offset + channel.sampler->input->offset;

					baseAnimations[animKey] = animation;
					TOAST_CORE_INFO("Created animation '%s' for node '%s'", baseAnimName.c_str(), baseNodeName.c_str());
				}

				auto it = baseAnimations.find(animKey);
				if (it == baseAnimations.end() || !it->second)
				{
					TOAST_CORE_WARN("No valid animation found for key '%s' — skipping submesh mapping", animKey.c_str());
					continue;
				}

				Ref<Animation> animation = it->second;

				// Output buffer
				uint8_t* bufferData = (uint8_t*)channel.sampler->output->buffer_view->buffer->data + channel.sampler->output->buffer_view->offset + channel.sampler->output->offset;
				size_t bufferSize = channel.sampler->output->count * cgltf_num_components(channel.sampler->output->type) * sizeof(float);

				if (channel.target_path == cgltf_animation_path_type_translation)
				{
					animation->TranslationSampleCount = channel.sampler->input->count;
					size_t tsSize = channel.sampler->input->count * sizeof(float);
					uint8_t* tsData = (uint8_t*)channel.sampler->input->buffer_view->buffer->data
						+ channel.sampler->input->buffer_view->offset
						+ channel.sampler->input->offset;
					animation->TranslationTimestampBuffer = Buffer(tsSize);
					animation->TranslationTimestampBuffer.Write(tsData, tsSize);

					animation->TranslationBuffer = Buffer(bufferSize);
					animation->TranslationBuffer.Write(bufferData, bufferSize);
					animation->Duration = std::max(animation->Duration, channel.sampler->input->max[0]);
				}
				else if (channel.target_path == cgltf_animation_path_type_rotation)
				{
					animation->RotationSampleCount = channel.sampler->input->count;
					size_t tsSize = channel.sampler->input->count * sizeof(float);
					uint8_t* tsData = (uint8_t*)channel.sampler->input->buffer_view->buffer->data
						+ channel.sampler->input->buffer_view->offset
						+ channel.sampler->input->offset;
					animation->RotationTimestampBuffer = Buffer(tsSize);
					animation->RotationTimestampBuffer.Write(tsData, tsSize);

					animation->RotationBuffer = Buffer(bufferSize);
					animation->RotationBuffer.Write(bufferData, bufferSize);
					animation->Duration = std::max(animation->Duration, channel.sampler->input->max[0]);
				}
				else if (channel.target_path == cgltf_animation_path_type_scale)
				{
					animation->ScaleSampleCount = channel.sampler->input->count;
					size_t tsSize = channel.sampler->input->count * sizeof(float);
					uint8_t* tsData = (uint8_t*)channel.sampler->input->buffer_view->buffer->data
						+ channel.sampler->input->buffer_view->offset
						+ channel.sampler->input->offset;
					animation->ScaleTimestampBuffer = Buffer(tsSize);
					animation->ScaleTimestampBuffer.Write(tsData, tsSize);

					animation->ScaleBuffer = Buffer(bufferSize);
					animation->ScaleBuffer.Write(bufferData, bufferSize);
					animation->Duration = std::max(animation->Duration, channel.sampler->input->max[0]);
				}


				auto partIt = mPartNameToIndex.find(baseNodeName);
				if (partIt != mPartNameToIndex.end())
				{
					Part& part = mParts[partIt->second];
					part.IsAnimated = true;

					if (part.LODAnimations.size() <= nodeLOD)
						part.LODAnimations.resize(nodeLOD + 1);

					part.LODAnimations[nodeLOD][baseAnimName] = animation;

					TOAST_CORE_INFO("Part '%s' LOD%u mapped to animation '%s'", part.Name.c_str(), nodeLOD, baseAnimName.c_str());
				}
				else
					TOAST_CORE_WARN("Animation '%s' targets node '%s' with no matching part", baseAnimName.c_str(), baseNodeName.c_str());
			}
		}

		// Parts absent from a LOD have no captured transform there. Fill the gap with
		// the nearest captured LOD so the part still resolves if sampled at that level,
		// rather than falling back to identity at the mesh origin.
		for (auto& part : mParts)
		{
			if (part.LODTransforms.size() < mLODGroups.size())
				part.LODTransforms.resize(mLODGroups.size());

			const PartLODTransform* source = nullptr;
			for (auto& xf : part.LODTransforms)
				if (xf.Captured) { source = &xf; break; }

			if (!source)
			{
				TOAST_CORE_WARN("Part '%s' has no captured transform at any LOD", part.Name.c_str());
				continue;
			}

			for (auto& xf : part.LODTransforms)
				if (!xf.Captured) xf = *source;
		}
	}

	void Mesh::InvalidatePlanet()
	{
		if(mLODGroups[0]->Vertices.size() > 0 && mLODGroups[0]->Indices.size() > 0)
		{
			mLODGroups[0]->VBuffer = nullptr;
			mLODGroups[0]->VBuffer = CreateRef<VertexBuffer>(&mLODGroups[0]->Vertices[0], (sizeof(Vertex) * (uint32_t)mLODGroups[0]->Vertices.size()), (uint32_t)mLODGroups[0]->Vertices.size(), 0);

			mLODGroups[0]->IBuffer = nullptr;
			mLODGroups[0]->IBuffer = CreateRef<IndexBuffer>(&mLODGroups[0]->Indices[0], (uint32_t)mLODGroups[0]->Indices.size());
			mLODGroups[0]->IndexCount = (uint32_t)mLODGroups[0]->Indices.size();

			mLODGroups[0]->Submeshes.clear();
			Submesh submesh;
			submesh.BaseVertex = 0;
			submesh.BaseIndex = 0;
			submesh.IndexCount = mLODGroups[0]->IndexCount;
			submesh.MaterialName = "Planet";
			mLODGroups[0]->Submeshes.emplace_back(submesh);
		}
	}

	void Mesh::SetInstanceData(const void* data, uint32_t size, uint32_t numberOfInstances, size_t lodIndex)
	{
		mLODGroups[lodIndex]->NumberOfInstances = numberOfInstances;

		if(mLODGroups[lodIndex]->InstancedVBuffer)
			mLODGroups[lodIndex]->InstancedVBuffer->SetData(data, size);
	}

	int32_t Mesh::FindPartIndex(const std::string& name) const
	{
		auto it = mPartNameToIndex.find(name);
		return it == mPartNameToIndex.end() ? -1 : (int32_t)it->second;
	}

	void Mesh::ProcessLODNode(const cgltf_node* node, Ref<LODGroup> lodGroup, uint32_t lodIndex, const DirectX::XMMATRIX& parentTransform, uint32_t& vertexCount, uint32_t& indexCount)
	{
		DirectX::XMMATRIX localTransform = NodeMatrixLocal(node);
		DirectX::XMMATRIX combinedTransform = DirectX::XMMatrixMultiply(localTransform, parentTransform);

		if (node->mesh)
		{
			for (unsigned int p = 0; p < node->mesh->primitives_count; p++)
			{
				const cgltf_primitive* primitive = &node->mesh->primitives[p];

				if (primitive->type != cgltf_primitive_type_triangles)
					continue;

				Submesh& submesh = lodGroup->Submeshes.emplace_back();
				submesh.MaterialName = primitive->material ? primitive->material->name : "";
				const std::string meshName = node->mesh->name ? node->mesh->name : "";

				for (unsigned int a = 0; a < primitive->attributes_count; a++)
				{
					cgltf_accessor* attribute = primitive->attributes[a].data;

					if (a == 0)
					{
						submesh.BaseVertex = vertexCount;
						submesh.VertexCount = static_cast<uint32_t>(attribute->count);
						vertexCount += submesh.VertexCount;
						lodGroup->Vertices.resize(vertexCount);
					}

					LoadAttribute(attribute, primitive->attributes[a].type, lodGroup->Vertices, submesh.BaseVertex);
				}

				// Color override
				if (mColorOverride.z != 0.0)
				{
					for (auto& vertex : lodGroup->Vertices)
						vertex.Color = { (float)mColorOverride.x, (float)mColorOverride.y, (float)mColorOverride.z };
				}

				// INDICES
				if (primitive->indices != NULL)
				{
					cgltf_accessor* indexAccessor = primitive->indices;
					const uint16_t* indices = reinterpret_cast<const uint16_t*>(reinterpret_cast<const uint8_t*>(indexAccessor->buffer_view->buffer->data) + indexAccessor->buffer_view->offset + indexAccessor->offset);

					submesh.IndexCount = static_cast<uint32_t>(indexAccessor->count);
					submesh.BaseIndex = indexCount;
					indexCount += submesh.IndexCount;
					lodGroup->Indices.resize(indexCount);

					for (size_t v = 0; v < indexAccessor->count; ++v)
					{
						cgltf_size idx = cgltf_accessor_read_index(indexAccessor, v);
						lodGroup->Indices[submesh.BaseIndex + v] = (uint32_t)idx + submesh.BaseVertex;
					}
				}

				std::string basePartName = GetPartBaseName(meshName);

				uint32_t partIndex = GetOrCreatePartIndex(basePartName);
				submesh.PartIndex = partIndex;

				Part& part = mParts[partIndex];

				// Node transforms differ per LOD — LOD0 may bake orientation into the vertices
				// while LOD1 carries it as a node rotation. Capture one entry per LOD group.
				if (part.LODTransforms.size() <= lodIndex)
					part.LODTransforms.resize(lodIndex + 1);

				PartLODTransform& lodXf = part.LODTransforms[lodIndex];
				if (!lodXf.Captured)
				{
					DecomposeTransform(localTransform, lodXf.LocalTranslation, lodXf.LocalRotation, lodXf.LocalScale);
					lodXf.Parent = parentTransform;
					lodXf.Captured = true;
				}

				// Rest* still seeds the part entity in AddMeshPartEntities
				if (!part.RestTransformCaptured)
				{
					DecomposeTransform(combinedTransform, part.RestTranslation, part.RestRotation, part.RestScale);
					part.RestTransformCaptured = true;
				}

				TOAST_CORE_INFO("Mesh '%s' loaded with material '%s', number of indices: %d, Part Name '%s'", meshName.c_str(), submesh.MaterialName.c_str(), submesh.IndexCount, basePartName.c_str());
			}
		}

		for (cgltf_size i = 0; i < node->children_count; ++i)
			ProcessLODNode(node->children[i], lodGroup, lodIndex, combinedTransform, vertexCount, indexCount);
	}

	uint32_t Mesh::GetOrCreatePartIndex(const std::string& partName)
	{
		auto it = mPartNameToIndex.find(partName);
		if (it != mPartNameToIndex.end())
			return it->second;

		uint32_t index = (uint32_t)mParts.size();
		mParts.emplace_back(partName);
		mPartNameToIndex[partName] = index;
		return index;
	}

	void Mesh::AddSubmesh(uint32_t indexCount, size_t LODGroupIndex)
	{
		Submesh& submesh = mLODGroups[LODGroupIndex]->Submeshes.emplace_back();
		submesh.BaseVertex = mLODGroups[LODGroupIndex]->VertexCount;
		submesh.BaseIndex = mLODGroups[LODGroupIndex]->IndexCount;
		submesh.MaterialName = "Standard";
		submesh.IndexCount = indexCount;
		TOAST_CORE_INFO("Adding submesh");
	}

	void Mesh::Bind(size_t lod)
	{
		if (lod >= mLODGroups.size())
			return;

		auto& group = mLODGroups[lod];
		if (group->VBuffer)          
			group->VBuffer->Bind();
		if (group->IBuffer)          
			group->IBuffer->Bind();
		if (group->InstancedVBuffer) 
			group->InstancedVBuffer->Bind();
	}

	bool Mesh::HasAnimation(const std::string& name) const
	{
		for (const auto& part : mParts)
			for (const auto& lodAnims : part.LODAnimations)
				if (lodAnims.find(name) != lodAnims.end())
					return true;
		return false;
	}

	float Mesh::GetAnimationDuration(const std::string& name) const
	{
		float longest = 0.0f;
		for (const auto& part : mParts)
			for (const auto& lodAnims : part.LODAnimations)
			{
				auto it = lodAnims.find(name);
				if (it != lodAnims.end() && it->second)
					longest = (std::max)(longest, it->second->Duration);
			}
		return longest;
	}

	// -----------------------------
	// ---------- PART -------------
	// -----------------------------

	bool Part::Sample(const std::string& animationName, uint32_t lodIndex, float time, DirectX::XMMATRIX& outMatrix) const
	{
		if (lodIndex >= LODAnimations.size()) 
			return false;
		if (lodIndex >= LODTransforms.size()) 
			return false;

		const auto& anims = LODAnimations[lodIndex];
		auto it = anims.find(animationName);
		if (it == anims.end() || !it->second) 
			return false;

		const Animation& a = *it->second;
		const PartLODTransform& lodXf = LODTransforms[lodIndex];
		float t = std::clamp(time, 0.0f, a.Duration);

		// glTF semantics: a channel gives the node's OWN LOCAL value, absolutely.
		// A channel that does not exist falls back to this LOD's node transform —
		// never to a hardcoded default, or the part loses that component entirely.

		DirectX::XMVECTOR animT = DirectX::XMLoadFloat3(&lodXf.LocalTranslation);
		if (a.TranslationSampleCount > 0)
		{
			DirectX::XMFLOAT3 now = SampleVec3(a.TranslationBuffer, a.TranslationTimestampBuffer, a.TranslationSampleCount, t, lodXf.LocalTranslation);
			animT = DirectX::XMLoadFloat3(&now);
		}

		DirectX::XMVECTOR animR = DirectX::XMLoadFloat4(&lodXf.LocalRotation);
		if (a.RotationSampleCount > 0)
		{
			DirectX::XMFLOAT4 now = SampleQuat(a.RotationBuffer, a.RotationTimestampBuffer, a.RotationSampleCount, t, lodXf.LocalRotation);
			animR = DirectX::XMLoadFloat4(&now);
		}

		DirectX::XMVECTOR animS = DirectX::XMLoadFloat3(&lodXf.LocalScale);
		if (a.ScaleSampleCount > 0)
		{
			DirectX::XMFLOAT3 now = SampleVec3(a.ScaleBuffer, a.ScaleTimestampBuffer, a.ScaleSampleCount, t, lodXf.LocalScale);
			animS = DirectX::XMLoadFloat3(&now);
		}

		// Node local (animated) times the ancestor chain — what a glTF viewer evaluates.
		// lodXf.Parent excludes this node's own local, so nothing is applied twice.
		outMatrix = DirectX::XMMatrixScalingFromVector(animS) * DirectX::XMMatrixRotationQuaternion(animR) * DirectX::XMMatrixTranslationFromVector(animT) * lodXf.Parent;

		return true;
	}

}