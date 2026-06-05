#include "tpch.h"
#define CGLTF_IMPLEMENTATION
#include "Mesh.h"

#include "Toast/Assets/AssetManager.h"

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

	Mesh::Mesh()
	{
		mLODGroups.emplace_back(CreateRef<LODGroup>());

		TOAST_CORE_INFO("Mesh Initialized!");
	}

	Mesh::Mesh(Ref<Material>& planetMaterial)
	{
		mLODGroups.emplace_back(CreateRef<LODGroup>());

		Submesh& submesh = mLODGroups[0]->Submeshes.emplace_back();
		submesh.MaterialName = planetMaterial->GetName();

		mMaterials.insert({ submesh.MaterialName,  MaterialLibrary::Load(submesh.MaterialName, false) });

		TOAST_CORE_INFO("Planet Mesh created");
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
		submesh.Transform = transform;
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
				submesh.MeshName = data->meshes[m].name;
				submesh.Transform = DirectX::XMMatrixIdentity();

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

				TOAST_CORE_INFO("Mesh '%s' loaded with material '%s', number of indices: %d", submesh.MeshName.c_str(), submesh.MaterialName.c_str(), submesh.IndexCount);
			}
		}

		// MATERIALS
		TOAST_CORE_INFO("Number of materials: %d", data->materials_count);
		for (int m = 0; m < data->materials_count; m++) 
		{
			TOAST_CORE_INFO("Material name: %s", data->materials[m].name);
				
			if (data->materials[m].has_pbr_metallic_roughness)
			{
				//TOAST_CORE_INFO("is PBR material");

				std::string materialName(data->materials[m].name);
				mMaterials.insert({ data->materials[m].name,  MaterialLibrary::Load(materialName, false) });

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
					mMaterials[data->materials[m].name]->SetAlbedolAssetHandle(AssetManager::ImportExternalAsset(completePath, "Textures"));
					//TOAST_CORE_INFO("Albedo map found: %s", completePath.c_str());	
				}

				mMaterials[data->materials[m].name]->SetAlbedo(albedoColor);
				mMaterials[data->materials[m].name]->SetUseAlbedo(useAlbedoMap);

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
					mMaterials[data->materials[m].name]->SetNormalAssetHandle(AssetManager::ImportExternalAsset(completePath, "Textures"));
					//TOAST_CORE_INFO("Normal map found: %s", completePath.c_str());
				}
				mMaterials[data->materials[m].name]->SetUseNormal(useNormalMap);

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
					mMaterials[data->materials[m].name]->SetMetalRoughAssetHandle(AssetManager::ImportExternalAsset(completePath, "Textures"));
				}
				else
				{
					//TOAST_CORE_INFO("data->materials[m].pbr_metallic_roughness.metallic_factor: %f", data->materials[m].pbr_metallic_roughness.metallic_factor);
					//TOAST_CORE_INFO("data->materials[m].pbr_metallic_roughness.roughness_factor: %f", data->materials[m].pbr_metallic_roughness.roughness_factor);
					metalness = data->materials[m].pbr_metallic_roughness.metallic_factor;
					roughness = data->materials[m].pbr_metallic_roughness.roughness_factor;
				}
				mMaterials[data->materials[m].name]->SetMetalness(metalness);
				mMaterials[data->materials[m].name]->SetRoughness(roughness);
				mMaterials[data->materials[m].name]->SetUseMetalRough(useMetalRoughMap);

				MaterialSerializer::Serialize(MaterialLibrary::Get(data->materials[m].name));
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
			for (unsigned int c = 0; c < data->animations[a].channels_count; c++)
				//animation->AnimationChannel = data->animations[a].channels[c];

			for (auto& submesh : mLODGroups[0]->Submeshes)
			{
				if (strcmp(data->animations[a].channels->target_node->name, submesh.MeshName.c_str()) == 0)
				{
					std::string name = std::string(data->animations[a].name);
					animation->Name = name;
					submesh.IsAnimated = true;
					submesh.Animations[name] = animation;
					TOAST_CORE_INFO("Submesh %s have an animation named %s, its now added to the submesh animation map", submesh.MeshName.c_str(), animation->Name.c_str());
				}
			}

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
			
			if (nodeName.find("LOD") != std::string::npos)
			{
				mLODGroups.emplace_back(CreateRef<LODGroup>());
				Ref<LODGroup> currentLOD = mLODGroups.back();

				uint32_t vertexCount = 0;
				uint32_t indexCount = 0;

				DirectX::XMMATRIX identity = DirectX::XMMatrixIdentity();

				for (cgltf_size j = 0; j < node->children_count; ++j)
					ProcessLODNode(node->children[j], currentLOD, identity, vertexCount, indexCount);

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

			if (data->materials[m].has_pbr_metallic_roughness)
			{
				//TOAST_CORE_INFO("is PBR material");

				std::string materialName(data->materials[m].name);
				mMaterials.insert({ data->materials[m].name,  MaterialLibrary::Load(materialName, false) });

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
					mMaterials[data->materials[m].name]->SetAlbedolAssetHandle(AssetManager::ImportExternalAsset(completePath, "Textures"));
					TOAST_CORE_INFO("Albedo map found for %s: %s", materialName.c_str(), completePath.c_str());
				}

				mMaterials[data->materials[m].name]->SetAlbedo(albedoColor);
				mMaterials[data->materials[m].name]->SetUseAlbedo(useAlbedoMap);

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
					mMaterials[data->materials[m].name]->SetNormalAssetHandle(AssetManager::ImportExternalAsset(completePath, "Textures"));
					TOAST_CORE_INFO("Normal map found for %s: %s", materialName.c_str(), completePath.c_str());
				}
				mMaterials[data->materials[m].name]->SetUseNormal(useNormalMap);

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
					mMaterials[data->materials[m].name]->SetMetalRoughAssetHandle(AssetManager::ImportExternalAsset(completePath, "Textures"));
					TOAST_CORE_INFO("Metalness/Roughness map found for %s: %s", materialName.c_str(), completePath.c_str());
				}
				else
				{
					//TOAST_CORE_INFO("data->materials[m].pbr_metallic_roughness.metallic_factor: %f", data->materials[m].pbr_metallic_roughness.metallic_factor);
					//TOAST_CORE_INFO("data->materials[m].pbr_metallic_roughness.roughness_factor: %f", data->materials[m].pbr_metallic_roughness.roughness_factor);
					metalness = data->materials[m].pbr_metallic_roughness.metallic_factor;
					roughness = data->materials[m].pbr_metallic_roughness.roughness_factor;
				}
				mMaterials[data->materials[m].name]->SetMetalness(metalness);
				mMaterials[data->materials[m].name]->SetRoughness(roughness);
				mMaterials[data->materials[m].name]->SetUseMetalRough(useMetalRoughMap);

				MaterialSerializer::Serialize(MaterialLibrary::Get(data->materials[m].name));
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
				std::string baseNodeName = StripLODSuffix(targetNode);

				// Unique key per animation+node combination
				std::string animKey = baseAnimName + "_" + baseNodeName;

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

				for (auto& lodGroup : mLODGroups)
				{
					for (auto& submesh : lodGroup->Submeshes)
					{
						std::string baseSubmeshName = StripLODSuffix(submesh.MeshName);
						if (baseSubmeshName == baseNodeName)
						{
							submesh.IsAnimated = true;
							submesh.Animations[baseAnimName] = animation;
							TOAST_CORE_INFO("Submesh '%s' (LOD) mapped to animation '%s'", submesh.MeshName.c_str(), baseAnimName.c_str());
						}
					}
				}
			}
		}
	}

	void Mesh::InvalidatePlanet()
	{
		if(mLODGroups[mActiveLODGroup]->Vertices.size() > 0 && mLODGroups[mActiveLODGroup]->Indices.size() > 0)
		{
			mLODGroups[mActiveLODGroup]->VBuffer = nullptr;
			mLODGroups[mActiveLODGroup]->VBuffer = CreateRef<VertexBuffer>(&mLODGroups[0]->Vertices[0], (sizeof(Vertex) * (uint32_t)mLODGroups[0]->Vertices.size()), (uint32_t)mLODGroups[0]->Vertices.size(), 0);

			mLODGroups[mActiveLODGroup]->IBuffer = nullptr;
			mLODGroups[mActiveLODGroup]->IBuffer = CreateRef<IndexBuffer>(&mLODGroups[0]->Indices[0], (uint32_t)mLODGroups[0]->Indices.size());
			mLODGroups[mActiveLODGroup]->IndexCount = (uint32_t)mLODGroups[0]->Indices.size();

			mLODGroups[mActiveLODGroup]->Submeshes.clear();
			Submesh submesh;
			submesh.BaseVertex = 0;
			submesh.BaseIndex = 0;
			submesh.IndexCount = mLODGroups[mActiveLODGroup]->IndexCount;
			submesh.MaterialName = "Planet";
			mLODGroups[mActiveLODGroup]->Submeshes.emplace_back(submesh);
		}
	}

	void Mesh::OnUpdate(Timestep ts)
	{
		if (mHasLODs)
		{
			// First pass — update all submesh transforms using current TimeElapsed
			for (int i = 0; i < 3; ++i)
			{
				for (auto& submesh : mLODGroups[i]->Submeshes)
				{
					if (submesh.IsAnimated)
						submesh.OnUpdate(ts);
				}
			}

			// Second pass — advance TimeElapsed once per unique animation ref
			std::unordered_set<Animation*> advanced;
			for (int i = 0; i < 3; ++i)
			{
				for (auto& submesh : mLODGroups[i]->Submeshes)
				{
					for (auto& animation : submesh.Animations)
					{
						if (!animation.second || !animation.second->IsActive) continue;
						if (advanced.find(animation.second.get()) != advanced.end()) continue;

						if (animation.second->IsReversed)
						{
							animation.second->TimeElapsed -= (float)ts;
							if (animation.second->TimeElapsed <= 0.0f)
							{
								animation.second->IsActive = false;
								animation.second->TimeElapsed = 0.0f;
								animation.second->IsReversed = false;
							}
						}
						else
						{
							animation.second->TimeElapsed += (float)ts;
							if (animation.second->TimeElapsed >= animation.second->Duration)
							{
								animation.second->IsActive = false;
								animation.second->TimeElapsed = animation.second->Duration;
							}
						}
						advanced.insert(animation.second.get());
					}
				}
			}
		}
		else 
		{
			for (int i = 0; i <= 2; ++i)
			{
				for (auto& submesh : mLODGroups[0]->Submeshes)
				{
					if (submesh.IsAnimated)
						submesh.OnUpdate(ts);
				}
			}
		}
	}

	void Mesh::ResetAnimations()
	{
		for (auto& submesh : mLODGroups[mActiveLODGroup]->Submeshes)
		{
			if (submesh.IsAnimated)
			{
				submesh.Transform = DirectX::XMMatrixIdentity() * DirectX::XMMatrixScaling(submesh.Scale.x, submesh.Scale.y, submesh.Scale.z)
					* (DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&submesh.Rotation)))
					* DirectX::XMMatrixTranslation(submesh.Translation.x, submesh.Translation.y, submesh.Translation.z);

				for (auto& animation : submesh.Animations) 
					animation.second->Reset();
			}
		}
	}

	bool Mesh::IsAnimationComplete(const std::string& name)
	{
		auto& lodGroup = mLODGroups[mActiveLODGroup];
		for (auto& submesh : lodGroup->Submeshes)
		{
			auto it = submesh.Animations.find(name);
			if (it != submesh.Animations.end())
			{
				// Complete = has played at least once AND is no longer active
				if (it->second->HasPlayed && !it->second->IsActive)
					return true;

				// Found the animation but it's still playing (or never started)
				return false;
			}
		}
		return false;
	}

	void Mesh::SetInstanceData(const void* data, uint32_t size, uint32_t numberOfInstances)
	{
		mLODGroups[mActiveLODGroup]->NumberOfInstances = numberOfInstances;

		if(mLODGroups[mActiveLODGroup]->InstancedVBuffer)
			mLODGroups[mActiveLODGroup]->InstancedVBuffer->SetData(data, size);
	}

	void Mesh::ProcessLODNode(const cgltf_node* node, Ref<LODGroup> lodGroup, const DirectX::XMMATRIX& parentTransform, uint32_t& vertexCount, uint32_t& indexCount)
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
				submesh.MeshName = node->mesh->name ? node->mesh->name : "";

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

				auto GetPartBaseName = [](const std::string& name) -> std::string
					{
						if (name.empty())
							return {};

						if (std::isdigit(static_cast<unsigned char>(name.back())))
							return name.substr(0, name.size() - 1);

						return name;
					};


				std::string basePartName = GetPartBaseName(submesh.MeshName);

				if (mParts.find(basePartName) == mParts.end())
					mParts[basePartName] = UUID();

				uint32_t partIndex = GetOrCreatePartIndex(basePartName);
				submesh.PartIndex = partIndex;

				MeshPart& part = mPartsUpdated[partIndex];
				if (!part.InitialTransformCaptured)
				{
					bool ok = DecomposeTransform(combinedTransform, part.InitialTranslation, part.InitialRotation, part.InitialScale);

					TOAST_CORE_ASSERT(ok, "Failed to decompose part transform for '%s'", part.Name.c_str());

					part.InitialTransformCaptured = true;
				}

				TOAST_CORE_INFO("Mesh '%s' loaded with material '%s', number of indices: %d, Part Name '%s'", submesh.MeshName.c_str(), submesh.MaterialName.c_str(), submesh.IndexCount, basePartName.c_str());
			}
		}

		for (cgltf_size i = 0; i < node->children_count; ++i)
			ProcessLODNode(node->children[i], lodGroup, combinedTransform, vertexCount, indexCount);
	}

	uint32_t Mesh::GetOrCreatePartIndex(const std::string& partName)
	{
		auto it = mPartNameToIndex.find(partName);
		if (it != mPartNameToIndex.end())
			return it->second;

		uint32_t index = (uint32_t)mPartsUpdated.size();
		mPartsUpdated.emplace_back(partName);
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

	void Mesh::Bind()
	{
		if (mLODGroups[mActiveLODGroup]->VBuffer)
			mLODGroups[mActiveLODGroup]->VBuffer->Bind();

		if (mLODGroups[mActiveLODGroup]->IBuffer)
			mLODGroups[mActiveLODGroup]->IBuffer->Bind();

		if(mLODGroups[mActiveLODGroup]->InstancedVBuffer)
			mLODGroups[mActiveLODGroup]->InstancedVBuffer->Bind();
	}

	void Submesh::OnUpdate(Timestep ts)
	{
		for (auto& animation : Animations)
		{
			if (!animation.second->IsActive)
				continue;

			DirectX::XMVECTOR animatedTranslation = DirectX::XMVectorZero();
			if (animation.second->TranslationBuffer.Size > 0)
			{
				DirectX::XMFLOAT3* dataPtr = animation.second->TranslationBuffer.As<DirectX::XMFLOAT3>();
				DirectX::XMVECTOR restPos = DirectX::XMLoadFloat3(&dataPtr[0]);
				DirectX::XMVECTOR animated = InterpolateTranslation(animation.second->TimeElapsed, animation.second->Name);
				animatedTranslation = DirectX::XMVectorSubtract(animated, restPos);
			}

			DirectX::XMVECTOR animatedRotation = (animation.second->RotationBuffer.Size > 0) ? InterpolateRotation(animation.second->TimeElapsed, animation.second->Name) : DirectX::XMQuaternionIdentity();

			DirectX::XMVECTOR animatedScale = (animation.second->ScaleBuffer.Size > 0) ? InterpolateScale(animation.second->TimeElapsed, animation.second->Name) : DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f);

			Transform = DirectX::XMMatrixScalingFromVector(animatedScale) * DirectX::XMMatrixRotationQuaternion(animatedRotation) * DirectX::XMMatrixTranslationFromVector(animatedTranslation);
		}
	}

	uint32_t Submesh::FindPosition(float animationTime, float* timestamps, uint32_t sampleCount)
	{
		for (uint32_t i = 0; i < sampleCount - 1; i++)
		{
			if (animationTime < timestamps[i + 1])
				return i;
		}
		return sampleCount - 2;
	}

	DirectX::XMVECTOR Submesh::InterpolateTranslation(float animationTime, const std::string& animationName)
	{
		auto& anim = Animations[animationName];
		float* timestamps = anim->TranslationTimestampBuffer.As<float>();
		uint32_t count = anim->TranslationSampleCount;

		uint32_t idx = FindPosition(animationTime, timestamps, count);
		if (idx >= count - 1)
		{
			DirectX::XMFLOAT3* data = anim->TranslationBuffer.As<DirectX::XMFLOAT3>();
			return DirectX::XMLoadFloat3(&data[count - 1]);
		}
		float t0 = timestamps[idx], t1 = timestamps[idx + 1];
		float factor = std::clamp((animationTime - t0) / (t1 - t0), 0.0f, 1.0f);
		DirectX::XMFLOAT3* data = anim->TranslationBuffer.As<DirectX::XMFLOAT3>();
		DirectX::XMVECTOR start = DirectX::XMLoadFloat3(&data[idx]);
		DirectX::XMVECTOR end = DirectX::XMLoadFloat3(&data[idx + 1]);
		return DirectX::XMVectorAdd(start, DirectX::XMVectorScale(DirectX::XMVectorSubtract(end, start), factor));
	}

	DirectX::XMVECTOR Submesh::InterpolateRotation(float animationTime, const std::string& animationName)
	{
		auto& anim = Animations[animationName];
		float* timestamps = anim->RotationTimestampBuffer.As<float>();
		uint32_t count = anim->RotationSampleCount;

		uint32_t idx = FindPosition(animationTime, timestamps, count);
		if (idx >= count - 1)
		{
			DirectX::XMFLOAT4* data = anim->RotationBuffer.As<DirectX::XMFLOAT4>();
			return DirectX::XMQuaternionNormalize(DirectX::XMLoadFloat4(&data[count - 1]));
		}
		float t0 = timestamps[idx], t1 = timestamps[idx + 1];
		float factor = std::clamp((animationTime - t0) / (t1 - t0), 0.0f, 1.0f);
		DirectX::XMFLOAT4* data = anim->RotationBuffer.As<DirectX::XMFLOAT4>();
		return DirectX::XMQuaternionNormalize(DirectX::XMQuaternionSlerp(
			DirectX::XMLoadFloat4(&data[idx]), DirectX::XMLoadFloat4(&data[idx + 1]), factor));
	}

	DirectX::XMVECTOR Submesh::InterpolateScale(float animationTime, const std::string& animationName)
	{
		auto& anim = Animations[animationName];
		float* timestamps = anim->ScaleTimestampBuffer.As<float>();
		uint32_t count = anim->ScaleSampleCount;

		uint32_t idx = FindPosition(animationTime, timestamps, count);
		if (idx >= count - 1)
		{
			DirectX::XMFLOAT3* data = anim->ScaleBuffer.As<DirectX::XMFLOAT3>();
			return DirectX::XMLoadFloat3(&data[count - 1]);
		}
		float t0 = timestamps[idx], t1 = timestamps[idx + 1];
		float factor = std::clamp((animationTime - t0) / (t1 - t0), 0.0f, 1.0f);
		DirectX::XMFLOAT3* data = anim->ScaleBuffer.As<DirectX::XMFLOAT3>();
		DirectX::XMVECTOR start = DirectX::XMLoadFloat3(&data[idx]);
		DirectX::XMVECTOR end = DirectX::XMLoadFloat3(&data[idx + 1]);
		return DirectX::XMVectorAdd(start, DirectX::XMVectorScale(DirectX::XMVectorSubtract(end, start), factor));
	}

}