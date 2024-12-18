#include "tpch.h"
#define CGLTF_IMPLEMENTATION
#include "Mesh.h"

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
						// Convert 16-bit indices to 32-bit and add baseVertex to make them absolute
						mLODGroups[0]->Indices[submesh.BaseIndex + i] = static_cast<uint32_t>(indices[i]) + submesh.BaseVertex;
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
					mMaterials[data->materials[m].name]->SetAlbedoTexture(TextureLibrary::LoadTexture2D(completePath.c_str()));
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
					mMaterials[data->materials[m].name]->SetNormalTexture(TextureLibrary::LoadTexture2D(completePath.c_str()));
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
					mMaterials[data->materials[m].name]->SetMetalRoughTexture(TextureLibrary::LoadTexture2D(completePath.c_str()));
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
				animation->DataBuffer = Buffer(data->animations[a].samplers[s].output->buffer_view->size);
				animation->DataBuffer.Write((uint8_t*)(data->animations[a].samplers[s].output->buffer_view->buffer->data) + data->animations[a].samplers[s].output->buffer_view->offset, data->animations[a].samplers[s].output->buffer_view->size);
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
				TOAST_CORE_INFO("LOD Group found with name: %s", nodeName.c_str());
				mLODGroups.emplace_back(CreateRef<LODGroup>());
				Ref<LODGroup> currentLOD = mLODGroups.back();

				uint32_t vertexCount = 0;
				uint32_t indexCount = 0;

				for (size_t j = 0; j < node->children_count; ++j)
				{
					const cgltf_node* child = node->children[j];

					for (unsigned int p = 0; p < child->mesh->primitives_count; p++)
					{
						const cgltf_primitive* primitive = &child->mesh->primitives[p];

						if (primitive->type != cgltf_primitive_type_triangles)
							continue;

						Submesh& submesh = currentLOD->Submeshes.emplace_back();
						submesh.MaterialName = std::string(primitive->material->name);
						//TOAST_CORE_CRITICAL("Loading submesh in currentLOD[%d] with material name: %s", mLODGroups.size(), primitive->material->name);
						submesh.MeshName = child->mesh->name;
						submesh.Transform = DirectX::XMMatrixIdentity();

						for (unsigned int a = 0; a < primitive->attributes_count; a++)
						{
							cgltf_accessor* attribute = primitive->attributes[a].data;

							if (a == 0)
							{
								submesh.BaseVertex = vertexCount;
								submesh.VertexCount = static_cast<uint32_t>(attribute->count);
								vertexCount += submesh.VertexCount;
								currentLOD->Vertices.resize(vertexCount);
							}

							LoadAttribute(attribute, primitive->attributes[a].type, currentLOD->Vertices, submesh.BaseVertex);
						}

						// Color override
						if (mColorOverride.z != 0.0)
						{
							for (auto& vertex : currentLOD->Vertices)
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
							currentLOD->Indices.resize(indexCount);

							for (size_t v = 0; v < indexAccessor->count; ++v)
							{
								// Convert 16-bit indices to 32-bit and add baseVertex to make them absolute
								currentLOD->Indices[submesh.BaseIndex + v] = static_cast<uint32_t>(indices[v]) + submesh.BaseVertex;
							}
						}

						TOAST_CORE_INFO("Mesh '%s' loaded with material '%s', number of indices: %d", submesh.MeshName.c_str(), submesh.MaterialName.c_str(), submesh.IndexCount);
					}
				}

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
					mMaterials[data->materials[m].name]->SetAlbedoTexture(TextureLibrary::LoadTexture2D(completePath.c_str()));
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
					mMaterials[data->materials[m].name]->SetNormalTexture(TextureLibrary::LoadTexture2D(completePath.c_str()));
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
					mMaterials[data->materials[m].name]->SetMetalRoughTexture(TextureLibrary::LoadTexture2D(completePath.c_str()));
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
	}

	void Mesh::InvalidatePlanet()
	{
		if(mLODGroups[mActiveLODGroup]->Vertices.size() > 0)
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
			for (int i = 0; i <= 2; ++i)
			{
				for (auto& submesh : mLODGroups[i]->Submeshes)
				{
					if (submesh.IsAnimated)
						submesh.OnUpdate(ts);
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

	void Mesh::SetInstanceData(const void* data, uint32_t size, uint32_t numberOfInstances)
	{
		mLODGroups[mActiveLODGroup]->NumberOfInstances = numberOfInstances;

		if(mLODGroups[mActiveLODGroup]->InstancedVBuffer)
			mLODGroups[mActiveLODGroup]->InstancedVBuffer->SetData(data, size);
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
			if (animation.second->IsActive) 
			{
				DirectX::XMVECTOR animatedTranslation = InterpolateTranslation(animation.second->TimeElapsed, animation.second->Name);

				Transform = DirectX::XMMatrixIdentity() * DirectX::XMMatrixScaling(Scale.x, Scale.y, Scale.z)
					* (DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&Rotation)))
					* DirectX::XMMatrixTranslationFromVector(animatedTranslation);

				animation.second->TimeElapsed += ts;

				if (animation.second->TimeElapsed >= animation.second->Duration)
				{
					animation.second->IsActive = false;
					animation.second->TimeElapsed = 0.0f;
				}
			}
		}
	}

	uint32_t Submesh::FindPosition(float animationTime, const std::string& animationName)
	{	
		for (uint32_t i = 0; i < (Animations[animationName]->SampleCount - 1); i++)
		{
			if (animationTime < ((Animations[animationName]->Duration / Animations[animationName]->SampleCount) * i))
				return i;
		}

		return Animations[animationName]->SampleCount - 2;

	}

	DirectX::XMVECTOR Submesh::InterpolateTranslation(float animationTime, const std::string& animationName)
	{
		uint32_t positionIndex = FindPosition(animationTime, animationName);
		uint32_t nextPositionIndex = (positionIndex + 1);
		TOAST_CORE_ASSERT("", nextPositionIndex < Animations[animationName]->SampleCount);
		float deltaTime = (float)(Animations[animationName]->Duration / Animations[animationName]->SampleCount);
		float factor = (animationTime - (float)(deltaTime * positionIndex)) / deltaTime;
		TOAST_CORE_ASSERT("Factor must be below 1.0f", factor <= 1.0f);
		factor = std::clamp(factor, 0.0f, 1.0f);

		DirectX::XMFLOAT3* dataPtr = Animations[animationName]->DataBuffer.As<DirectX::XMFLOAT3>();

		const DirectX::XMVECTOR start = { dataPtr[positionIndex].x, dataPtr[positionIndex].y, dataPtr[positionIndex].z };
		const DirectX::XMVECTOR end = { dataPtr[nextPositionIndex].x, dataPtr[nextPositionIndex].y, dataPtr[nextPositionIndex].z };

		DirectX::XMVECTOR delta = DirectX::XMVectorSubtract(end, start);
		DirectX::XMVECTOR interpolatedTranslation = DirectX::XMVectorAdd(start, DirectX::XMVectorScale(delta, factor));
		return interpolatedTranslation;
	}

}