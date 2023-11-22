#include "tpch.h"
#define CGLTF_IMPLEMENTATION
#include "Mesh.h"

#include <filesystem>
#include <math.h>

namespace Toast {

	template<typename T>
	static void LoadAttribute(cgltf_accessor* attribute, T Vertex::* member, std::vector<Vertex>& vertices, uint32_t baseVertex)
	{
		T* data = reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(attribute->buffer_view->buffer->data) + attribute->buffer_view->offset + attribute->offset);

		for (int a = (baseVertex); a < (baseVertex + attribute->count); a++)
			vertices[a].*member = data[a - baseVertex];
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
		mMaterials["Standard"] = MaterialLibrary::Get("Standard");

		// Setting up the constant buffer and data buffer for the mesh rendering
		mModelCBuffer = ConstantBufferLibrary::Load("Model", 80, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_VERTEX_SHADER, 1) });
		mModelCBuffer->Bind();
		mModelBuffer.Allocate(mModelCBuffer->GetSize());
		mModelBuffer.ZeroInitialize();

		TOAST_CORE_INFO("Mesh Initialized!");
		TOAST_CORE_INFO("Number of materials: %d", mMaterials.size());
	}

	Mesh::Mesh(bool isPlanet)
	{
		mIsPlanet = isPlanet;

		// Setting up the constant buffer and data buffer for the planet mesh rendering
		mPlanetCBuffer = ConstantBufferLibrary::Load("Planet", 80, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_PIXEL_SHADER, 4) });
		mPlanetCBuffer->Bind();
		mPlanetBuffer.Allocate(mPlanetCBuffer->GetSize());
		mPlanetBuffer.ZeroInitialize();
	}

	Mesh::Mesh(const std::string& filePath, const bool skyboxMesh)
		: mFilePath(filePath)
	{
		uint32_t vertexCount = 0;
		uint32_t indexCount = 0;

		mModelCBuffer = ConstantBufferLibrary::Load("Model", 80, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_VERTEX_SHADER, 1) });
		mModelCBuffer->Bind();
		mModelBuffer.Allocate(mModelCBuffer->GetSize());
		mModelBuffer.ZeroInitialize();

		if(!skyboxMesh)
			TOAST_CORE_INFO("Loading Mesh: '%s'", mFilePath.c_str());

		cgltf_options options = { 0 };
		cgltf_data* data = NULL;
		cgltf_result result = cgltf_parse_file(&options, filePath.c_str(), &data);

		if (result == cgltf_result_success)
		{
			result = cgltf_load_buffers(&options, data, filePath.c_str());

			mIsAnimated = data->animations_count > 0;

			DirectX::XMFLOAT3 translation;
			DirectX::XMFLOAT4 rotation;
			DirectX::XMFLOAT3 scale;
			//TOAST_CORE_INFO("data->accessors_count: %d", data->accessors_count);
			for (unsigned m = 0; m < data->meshes_count; m++)
			{
				//TOAST_CORE_INFO("Loading Mesh: %s", data->meshes[m].name);

				//TOAST_CORE_INFO("	primitives_count: %d", data->meshes[m].primitives_count);

				for (unsigned int p = 0; p < data->meshes[m].primitives_count; p++)
				{
					Submesh& submesh = mSubmeshes.emplace_back();
					submesh.MaterialName = std::string(data->meshes[m].primitives[p].material->name);
					submesh.MeshName = data->meshes[m].name;

					// TRANSFORM
					submesh.Scale = data->nodes[m].has_scale ? DirectX::XMFLOAT3(data->nodes[m].scale[0], data->nodes[m].scale[1], data->nodes[m].scale[2]) : DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
					submesh.Rotation = data->nodes[m].has_rotation ? DirectX::XMFLOAT4(data->nodes[m].rotation[0], data->nodes[m].rotation[1], data->nodes[m].rotation[2], data->nodes[m].rotation[3]) : DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
					submesh.Translation = data->nodes[m].has_translation ? DirectX::XMFLOAT3(data->nodes[m].translation[0], data->nodes[m].translation[1], data->nodes[m].translation[2]) : DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
					submesh.StartTranslation = submesh.Translation;
					DirectX::XMVECTOR test = {0.0f, 0.0f, 0.0f};

					submesh.Transform = DirectX::XMMatrixIdentity() * DirectX::XMMatrixScaling(submesh.Scale.x, submesh.Scale.y, submesh.Scale.z)
						* (DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&submesh.Rotation)))
						* DirectX::XMMatrixTranslation(submesh.Translation.x, submesh.Translation.y, submesh.Translation.z);

					if (data->meshes[m].primitives[p].type != cgltf_primitive_type_triangles) 
						continue;

					for (unsigned int a = 0; a < data->meshes[m].primitives[p].attributes_count; a++)
					{
						cgltf_accessor* attribute = data->meshes[m].primitives[p].attributes[a].data;

						if (a == 0) 
						{
							submesh.BaseVertex = vertexCount;
							submesh.VertexCount = (uint32_t)attribute->count;
							vertexCount += submesh.VertexCount;
							mVertices.resize(vertexCount);
						}

						LoadAttribute(attribute, data->meshes[m].primitives[p].attributes[a].type, mVertices, submesh.BaseVertex);
					}

					// INDICES
					if (data->meshes[m].primitives[p].indices != NULL)
					{
						cgltf_accessor* attribute = data->meshes[m].primitives[p].indices;

						submesh.IndexCount = attribute->count;
						submesh.BaseIndex = indexCount;
						indexCount += submesh.IndexCount;

						mIndices.resize(indexCount);

						if (attribute->component_type == cgltf_component_type_r_16u)
						{
							uint16_t* indices = reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(attribute->buffer_view->buffer->data) + attribute->buffer_view->offset + attribute->offset);
							for (int i = submesh.BaseIndex; i < (submesh.BaseIndex + attribute->count); i++)
								mIndices[i] = (uint32_t)indices[i - submesh.BaseIndex];
						}
					}
				}
			}

			// MATERIALS
			TOAST_CORE_INFO("Number of materials: %d", data->materials_count);
			for (int m = 0; m < data->materials_count; m++) 
			{
				TOAST_CORE_INFO("Material name: %s", data->materials[m].name);
				Texture2D* whiteTexture = (Texture2D*)(TextureLibrary::Get("assets/textures/White.png"));
				if (data->materials[m].has_pbr_metallic_roughness)
				{
					//TOAST_CORE_INFO("is PBR material");

					std::string materialName(data->materials[m].name);
					mMaterials.insert({ data->materials[m].name,  MaterialLibrary::Load(materialName, ShaderLibrary::Get("assets/shaders/ToastPBR.hlsl")) });

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
						mMaterials[data->materials[m].name]->SetTexture(3, D3D11_PIXEL_SHADER, TextureLibrary::LoadTexture2D(completePath.c_str()));
						//TOAST_CORE_INFO("Albedo map found: %s", completePath.c_str());	
					}
					else
					{
						mMaterials[data->materials[m].name]->SetTexture(3, D3D11_PIXEL_SHADER, whiteTexture);
					}
					mMaterials[data->materials[m].name]->Set<DirectX::XMFLOAT4>("Albedo", albedoColor);
					mMaterials[data->materials[m].name]->Set<int>("AlbedoTexToggle", useAlbedoMap);

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
						mMaterials[data->materials[m].name]->SetTexture(4, D3D11_PIXEL_SHADER, TextureLibrary::LoadTexture2D(completePath.c_str()));
						//TOAST_CORE_INFO("Normal map found: %s", completePath.c_str());
					}
					else
					{
						mMaterials[data->materials[m].name]->SetTexture(4, D3D11_PIXEL_SHADER, whiteTexture);
					}
					mMaterials[data->materials[m].name]->Set<int>("NormalTexToggle", useNormalMap);

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
						mMaterials[data->materials[m].name]->SetTexture(5, D3D11_PIXEL_SHADER, TextureLibrary::LoadTexture2D(completePath.c_str()));
					}
					else
					{
						//TOAST_CORE_INFO("data->materials[m].pbr_metallic_roughness.metallic_factor: %f", data->materials[m].pbr_metallic_roughness.metallic_factor);
						//TOAST_CORE_INFO("data->materials[m].pbr_metallic_roughness.roughness_factor: %f", data->materials[m].pbr_metallic_roughness.roughness_factor);
						metalness = data->materials[m].pbr_metallic_roughness.metallic_factor;
						roughness = data->materials[m].pbr_metallic_roughness.roughness_factor;

						mMaterials[data->materials[m].name]->SetTexture(5, D3D11_PIXEL_SHADER, whiteTexture);
					}
					mMaterials[data->materials[m].name]->Set<float>("Metalness", metalness);
					mMaterials[data->materials[m].name]->Set<float>("Roughness", roughness);
					mMaterials[data->materials[m].name]->Set<int>("MetalRoughTexToggle", useMetalRoughMap);
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

				for (auto& submesh : mSubmeshes)
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

			cgltf_free(data);
			
			mVertexBuffer = CreateRef<VertexBuffer>(&mVertices[0], (sizeof(Vertex) * (uint32_t)mVertices.size()), (uint32_t)mVertices.size(), 0);
			mIndexBuffer = CreateRef<IndexBuffer>(&mIndices[0], (uint32_t)mIndices.size());
		}

		//		// Emission
		//		float emission = 0.0f;
		//		aiColor3D aiEmission;
		//		if (aiMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, aiEmission) == AI_SUCCESS) 
		//			emission = aiEmission.r;
		//		
		//		mMaterial->Set<float>("Emission", emission);
	}

	Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const DirectX::XMMATRIX& transform)
	{
		Submesh submesh;
		submesh.BaseVertex = 0;
		submesh.BaseIndex = 0;
		submesh.IndexCount = (uint32_t)indices.size() * 3;
		submesh.Transform = transform;
		submesh.MaterialName = "Standard";
		mSubmeshes.push_back(submesh);

		mMaterials.insert({ "Standard", MaterialLibrary::Get("Standard") });

		mVertexBuffer = CreateRef<VertexBuffer>(&mVertices[0], (sizeof(Vertex) * (uint32_t)mVertices.size()), (uint32_t)mVertices.size(), 0);
		mIndexBuffer = CreateRef<IndexBuffer>(&mIndices[0], (uint32_t)mIndices.size());
	}

	void Mesh::InvalidatePlanet()
	{
		if(mVertices.size() > 0)
		{
		//	if (patchGeometryRebuilt)
		//	{
				mVertexBuffer = nullptr;
				mVertexBuffer = CreateRef<VertexBuffer>(&mVertices[0], (sizeof(Vertex) * (uint32_t)mVertices.size()), (uint32_t)mVertices.size(), 0);

				mIndexBuffer = nullptr;
				mIndexBuffer = CreateRef<IndexBuffer>(&mIndices[0], (uint32_t)mIndices.size());
				mIndexCount = (uint32_t)mIndices.size();

				mSubmeshes.clear();
				Submesh submesh;
				submesh.BaseVertex = 0;
				submesh.BaseIndex = 0;
				submesh.IndexCount = mIndexCount;
				//submesh.Transform = transform;
				//submesh.MaterialName = "Standard";
				mSubmeshes.push_back(submesh);
			}

		//	mInstanceVertexBuffer = nullptr;
		//	mInstanceVertexBuffer = CreateRef<VertexBuffer>(&mPlanetPatches[0], sizeof(PlanetPatch) * (uint32_t)mPlanetPatches.size(), (uint32_t)mPlanetPatches.size(), 1);
		//}

		//mVertexCount = 0;
		//mIndexCount = 0;
		//for (auto& patch : mPlanetPatches) 
		//{
		//	Submesh& submesh = mSubmeshes.emplace_back();
		//	submesh.BaseVertex = mVertexCount;
		//	submesh.BaseIndex = mIndexCount;
		//	submesh.VertexCount = mPlanetVertices.size();
		//	submesh.IndexCount = mIndices.size();

		//	mVertexCount += submesh.VertexCount;
		//	mIndexCount += submesh.IndexCount;
		//}
	}

	void Mesh::OnUpdate(Timestep ts)
	{
		for (auto& submesh : mSubmeshes)
		{
			if (submesh.IsAnimated)
				submesh.OnUpdate(ts);
		}
	}

	void Mesh::ResetAnimations()
	{
		for (auto& submesh : mSubmeshes) 
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

	const Toast::ShaderCBufferElement* Mesh::FindCBufferElementDeclaration(const std::string& materialName, const std::string& cbufferName, const std::string& name)
	{
		const auto& shaderCBuffers = mMaterials[materialName]->GetShader()->GetCBuffersBindings();

		if (shaderCBuffers.size() > 0)
		{
			const ShaderCBufferBindingDesc& buffer = shaderCBuffers.at(cbufferName);

			if (buffer.CBufferElements.find(name) == buffer.CBufferElements.end())
				return nullptr;

			return &buffer.CBufferElements.at(name);
		}

		return nullptr;
	}

	void Mesh::AddSubmesh(uint32_t indexCount)
	{
		Submesh& submesh = mSubmeshes.emplace_back();
		submesh.BaseVertex = mVertexCount;
		submesh.BaseIndex = mIndexCount;
		submesh.MaterialName = "Standard";
		submesh.IndexCount = indexCount;
		TOAST_CORE_INFO("Adding submesh");
	}

	void Mesh::Map(const std::string& materialName)
	{
		// if the planet is a mesh upload Planet data to the GPU
		if (mPlanetCBuffer)
			mPlanetCBuffer->Map(mPlanetBuffer);

		if (mModelCBuffer)
			mModelCBuffer->Map(mModelBuffer);

		if (mMaterials.size() > 0)
			mMaterials[materialName]->Map();
	}

	void Mesh::Bind(const std::string& materialName, bool environment)
	{
		mVertexBuffer->Bind();
		mIndexBuffer->Bind();

		// if the planet is a mesh upload Planet data to the GPU
		if (mPlanetCBuffer)
			mPlanetCBuffer->Bind();

		if(mModelCBuffer)
			mModelCBuffer->Bind();

		if (mMaterials.size() > 0)
			mMaterials[materialName]->Bind(environment);
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