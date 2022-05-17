#include "tpch.h"
#include "Mesh.h"

#include <filesystem>
#include <math.h>

#define CGLTF_IMPLEMENTATION
#include <../cgltf/include/cgltf.h>

namespace Toast {

	template<typename T>
	static void LoadAttribute(cgltf_accessor* attribute, cgltf_attribute_type attributetype, std::vector<Vertex>& vertices)
	{
		T* data = reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(attribute->buffer_view->buffer->data) + attribute->buffer_view->offset + attribute->offset);
		for (int a = 0; a < attribute->count; a++) 
		{
			switch (attributetype) 
			{
			case cgltf_attribute_type_position:
				vertices[a].Position = data[a];
			case cgltf_attribute_type_normal:
				vertices[a].Normal = data[a];
			case cgltf_attribute_type_tangent:
				vertices[a].Tangent = data[a];
			case cgltf_attribute_type_texcoord:
				vertices[a].Texcoord = data[a];
			}
		}		
	}

	Mesh::Mesh()
	{
		mMaterial = MaterialLibrary::Get("Standard");

		// Setting up the constant buffer and data buffer for the mesh rendering
		mModelCBuffer = ConstantBufferLibrary::Load("Model", 80, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_VERTEX_SHADER, 1) });
		mModelCBuffer->Bind();
		mModelBuffer.Allocate(mModelCBuffer->GetSize());
		mModelBuffer.ZeroInitialize();

		//TOAST_CORE_INFO("Mesh Initialized!");
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
			TOAST_CORE_INFO("cgltf result success!");
			TOAST_CORE_INFO("Number of meshes: %d", data->meshes_count);

			result = cgltf_load_buffers(&options, data, filePath.c_str());
			if(result == cgltf_result_success)
				TOAST_CORE_INFO("cgltf data buffers result success!");

			// TRANSFORM
			DirectX::XMFLOAT3 translation;
			DirectX::XMFLOAT4 rotation;
			DirectX::XMFLOAT3 scale;

			if (data->nodes[0].has_scale)
				scale = { data->nodes[0].scale[0], data->nodes[0].scale[1], data->nodes[0].scale[2] };
			else
				scale = { 1.0f, 1.0f, 1.0f };
				
			if (data->nodes[0].has_rotation) 
				rotation = { data->nodes[0].rotation[0], data->nodes[0].rotation[1], data->nodes[0].rotation[2], data->nodes[0].rotation[3] };
			else 
				rotation = { 0.0f, 0.0f, 0.0f, 0.0f };
			
			if (data->nodes[0].has_translation) 
				scale = { data->nodes[0].translation[0], data->nodes[0].translation[1], data->nodes[0].translation[2] };
			else 
				translation = { 0.0f, 0.0f, 0.0f };

			mTransform = DirectX::XMMatrixIdentity() * DirectX::XMMatrixScaling(scale.x, scale.y, scale.z)
				* (DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&rotation)))
				* DirectX::XMMatrixTranslation(translation.x, translation.y, translation.z);

			for (unsigned m = 0; m < data->meshes_count; m++) 
			{
				Submesh& submesh = mSubmeshes.emplace_back();
				submesh.BaseVertex = vertexCount;
				submesh.BaseIndex = indexCount;
				submesh.MaterialIndex = 0;// data->meshes[m].primitives[0].material;
				submesh.VertexCount = data->meshes[m].primitives[0].attributes_count;
				vertexCount += submesh.VertexCount;
				submesh.MeshName = data->meshes[m].name;
				submesh.Transform = mTransform;

				for (unsigned int p = 0; p < data->meshes[m].primitives_count; p++)
				{
					if (data->meshes[m].primitives[p].type != cgltf_primitive_type_triangles) 
						continue;
					
					TOAST_CORE_INFO("Mesh holds cgltf_primitive_type_triangles");

					for (unsigned int a = 0; a < data->meshes[m].primitives[p].attributes_count; a++)
					{
						// POSITIONS
						if (data->meshes[m].primitives[p].attributes[a].type == cgltf_attribute_type_position)      
						{
							TOAST_CORE_INFO("Mesh holds postion data");

							cgltf_accessor* attribute = data->meshes[m].primitives[p].attributes[a].data;
							vertexCount = (int)attribute->count;
							mVertices.resize(vertexCount);

							if ((attribute->component_type == cgltf_component_type_r_32f) && (attribute->type == cgltf_type_vec3))
							{

								TOAST_CORE_INFO("Mesh postion data correct format");
								TOAST_CORE_INFO("Attribute count: %d", (int)attribute->count);
							
								//LoadAttribute<DirectX::XMFLOAT3>(attribute, cgltf_attribute_type_position, mVertices);

								DirectX::XMFLOAT3* positions = reinterpret_cast<DirectX::XMFLOAT3*>(reinterpret_cast<uint8_t*>(attribute->buffer_view->buffer->data) + attribute->buffer_view->offset + attribute->offset);
								for (int v = 0; v < attribute->count; v++)
									mVertices[v].Position = positions[v];
							}
						}

						// NORMALS
						if (data->meshes[m].primitives[p].attributes[a].type == cgltf_attribute_type_normal)
						{
							TOAST_CORE_INFO("Mesh holds normal data");

							cgltf_accessor* attribute = data->meshes[m].primitives[p].attributes[a].data;

							if ((attribute->component_type == cgltf_component_type_r_32f) && (attribute->type == cgltf_type_vec3))
							{
								//LoadAttribute<DirectX::XMFLOAT3>(attribute, cgltf_attribute_type_normal, mVertices);

								DirectX::XMFLOAT3* normals = reinterpret_cast<DirectX::XMFLOAT3*>(reinterpret_cast<uint8_t*>(attribute->buffer_view->buffer->data) + attribute->buffer_view->offset + attribute->offset);
								for (int v = 0; v < attribute->count; v++)
									mVertices[v].Normal = normals[v];
							}
						}

						// TANGENTS
						if (data->meshes[m].primitives[p].attributes[a].type == cgltf_attribute_type_tangent)
						{
							TOAST_CORE_INFO("Mesh holds tangent data");

							cgltf_accessor* attribute = data->meshes[m].primitives[p].attributes[a].data;

							if ((attribute->component_type == cgltf_component_type_r_32f) && (attribute->type == cgltf_type_vec3))
							{
								//LoadAttribute<DirectX::XMFLOAT3>(attribute, cgltf_attribute_type_tangent, mVertices);
								DirectX::XMFLOAT3* tangents = reinterpret_cast<DirectX::XMFLOAT3*>(reinterpret_cast<uint8_t*>(attribute->buffer_view->buffer->data) + attribute->buffer_view->offset + attribute->offset);
								for (int v = 0; v < attribute->count; v++)
									mVertices[v].Tangent = tangents[v];
							}
						}

						// TEXCOORDS
						if (data->meshes[m].primitives[p].attributes[a].type == cgltf_attribute_type_texcoord)
						{
							TOAST_CORE_INFO("Mesh holds texcoords data");

							cgltf_accessor* attribute = data->meshes[m].primitives[p].attributes[a].data;

							if ((attribute->component_type == cgltf_component_type_r_32f) && (attribute->type == cgltf_type_vec2))
							{
								//LoadAttribute<DirectX::XMFLOAT2>(attribute, cgltf_attribute_type_texcoord, mVertices);
								DirectX::XMFLOAT2* texCoords = reinterpret_cast<DirectX::XMFLOAT2*>(reinterpret_cast<uint8_t*>(attribute->buffer_view->buffer->data) + attribute->buffer_view->offset + attribute->offset);
								for (int v = 0; v < attribute->count; v++)
									mVertices[v].Texcoord = texCoords[v];
							}
						}
					}

					// INDICES
					if (data->meshes[m].primitives[p].indices != NULL)
					{
						cgltf_accessor* attribute = data->meshes[m].primitives[p].indices;

						submesh.IndexCount = attribute->count;
						indexCount += submesh.IndexCount;

						mIndices.resize(indexCount);

						TOAST_CORE_INFO("Mesh indices count: %d", submesh.IndexCount);

						if (attribute->component_type == cgltf_component_type_r_16u)
						{
							TOAST_CORE_INFO("Mesh indices of type r_16u");

							uint16_t* indices = reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(attribute->buffer_view->buffer->data) + attribute->buffer_view->offset + attribute->offset);
							for (int i = 0; i < attribute->count; i++)
								mIndices[i] = (uint32_t)indices[i];
						}
					}
				}
			}

			// MATERIALS
			TOAST_CORE_INFO("Number of materials: %d", data->materials_count);
			mMaterial = MaterialLibrary::Get("Standard");

			Texture2D* whiteTexture = (Texture2D*)(TextureLibrary::Get("assets/textures/White.png"));
			if (data->materials[0].has_pbr_metallic_roughness) 
			{
				std::string materialName(data->materials[0].name);
				mMaterial = MaterialLibrary::Load(materialName, ShaderLibrary::Get("assets/shaders/ToastPBR.hlsl"));

				// ALBEDO
				DirectX::XMFLOAT4 albedoColor;
				albedoColor.x = (unsigned char)data->materials[0].pbr_metallic_roughness.base_color_factor[0];
				albedoColor.y = (unsigned char)data->materials[0].pbr_metallic_roughness.base_color_factor[1];
				albedoColor.z = (unsigned char)data->materials[0].pbr_metallic_roughness.base_color_factor[2];
				albedoColor.w = (unsigned char)data->materials[0].pbr_metallic_roughness.base_color_factor[3];

				bool hasAlbedoMap = data->materials[0].pbr_metallic_roughness.base_color_texture.texture;
				int useAlbedoMap = 0;
				if (hasAlbedoMap)
				{
					std::string texPath(data->materials[0].pbr_metallic_roughness.base_color_texture.texture->image->uri);
					std::filesystem::path path = mFilePath;
					auto parentPath = path.parent_path();
					std::string texturePath = parentPath.string();
					std::string completePath = texturePath.append("\\").append(texPath.c_str());
					albedoColor = { 1.0f, 1.0f, 1.0f, 1.0f };
					useAlbedoMap = 1;
					mMaterial->SetTexture(3, D3D11_PIXEL_SHADER, TextureLibrary::LoadTexture2D(completePath.c_str()));
					TOAST_CORE_INFO("Albedo map found: %s", completePath.c_str());	
				}
				else
				{
					mMaterial->SetTexture(3, D3D11_PIXEL_SHADER, whiteTexture);
				}
				mMaterial->Set<DirectX::XMFLOAT4>("Albedo", albedoColor);
				mMaterial->Set<int>("AlbedoTexToggle", useAlbedoMap);

				// NORMAL
				bool hasNormalMap = data->materials[0].normal_texture.texture;
				int useNormalMap = 0;
				if (hasNormalMap)
				{
					std::string texPath(data->materials[0].normal_texture.texture->image->uri);
					std::filesystem::path path = mFilePath;
					auto parentPath = path.parent_path();
					std::string texturePath = parentPath.string();
					std::string completePath = texturePath.append("\\").append(texPath.c_str());
					useNormalMap = 1;
					mMaterial->SetTexture(4, D3D11_PIXEL_SHADER, TextureLibrary::LoadTexture2D(completePath.c_str()));
					TOAST_CORE_INFO("Normal map found: %s", completePath.c_str());
				}
				else
				{
					mMaterial->SetTexture(4, D3D11_PIXEL_SHADER, whiteTexture);
				}
				mMaterial->Set<int>("NormalTexToggle", useNormalMap);

				// METALLNESS ROUGHNESS
				bool hasMetalRoughMap = data->materials[0].pbr_metallic_roughness.metallic_roughness_texture.texture;
				int useMetalRoughMap = 0;
				float metalness = 0.0f;
				if (hasMetalRoughMap)
				{
					std::string texPath(data->materials[0].pbr_metallic_roughness.metallic_roughness_texture.texture->image->uri);
					std::filesystem::path path = mFilePath;
					auto parentPath = path.parent_path();
					std::string texturePath = parentPath.string();
					std::string completePath = texturePath.append("\\").append(texPath.c_str());
					metalness = 1.0f;
					useMetalRoughMap = 1;
					mMaterial->SetTexture(5, D3D11_PIXEL_SHADER, TextureLibrary::LoadTexture2D(completePath.c_str()));
				}
				else
				{
					mMaterial->SetTexture(5, D3D11_PIXEL_SHADER, whiteTexture);
				}
				mMaterial->Set<float>("Metalness", metalness);
				mMaterial->Set<int>("MetalRoughTexToggle", useMetalRoughMap);
			}

			cgltf_free(data);

			//TOAST_CORE_INFO("Mesh loaded!");
			//for each (Vertex v in mVertices)
			//	TOAST_CORE_INFO("Vertex x: %f, y: %f, z: %f", v.Position.x, v.Position.y, v.Position.z);
			//for each (uint16_t i in mIndices)
			//	TOAST_CORE_INFO("Indices: %d ", i);
			
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
		mSubmeshes.push_back(submesh);

		mMaterial = MaterialLibrary::Get("Standard");

		mVertexBuffer = CreateRef<VertexBuffer>(&mVertices[0], (sizeof(Vertex) * (uint32_t)mVertices.size()), (uint32_t)mVertices.size(), 0);
		mIndexBuffer = CreateRef<IndexBuffer>(&mIndices[0], (uint32_t)mIndices.size());
	}

	void Mesh::InvalidatePlanet(bool patchGeometryRebuilt)
	{
		if(mPlanetPatches.size() > 0)
		{
			if (patchGeometryRebuilt)
			{
				mVertexBuffer = nullptr;
				mVertexBuffer = CreateRef<VertexBuffer>(&mPlanetVertices[0], (sizeof(PlanetVertex) * (uint32_t)mPlanetVertices.size()), (uint32_t)mPlanetVertices.size(), 0);

				mIndexBuffer = nullptr;
				mIndexBuffer = CreateRef<IndexBuffer>(&mIndices[0], (uint32_t)mIndices.size());
			}

			mInstanceVertexBuffer = nullptr;
			mInstanceVertexBuffer = CreateRef<VertexBuffer>(&mPlanetPatches[0], sizeof(PlanetPatch) * (uint32_t)mPlanetPatches.size(), (uint32_t)mPlanetPatches.size(), 1);
		}

		mSubmeshes.clear();
		mVertexCount = 0;
		mIndexCount = 0;
		for (auto& patch : mPlanetPatches) 
		{
			Submesh& submesh = mSubmeshes.emplace_back();
			submesh.BaseVertex = mVertexCount;
			submesh.BaseIndex = mIndexCount;
			submesh.VertexCount = mPlanetVertices.size();
			submesh.IndexCount = mIndices.size();

			mVertexCount += submesh.VertexCount;
			mIndexCount += submesh.IndexCount;
		}
	}

	void Mesh::OnUpdate(Timestep ts)
	{

	}

	const Toast::ShaderCBufferElement* Mesh::FindCBufferElementDeclaration(const std::string& cbufferName, const std::string& name)
	{
		const auto& shaderCBuffers = mMaterial->GetShader()->GetCBuffersBindings();

		if (shaderCBuffers.size() > 0)
		{
			const ShaderCBufferBindingDesc& buffer = shaderCBuffers.at(cbufferName);

			if (buffer.CBufferElements.find(name) == buffer.CBufferElements.end())
				return nullptr;

			return &buffer.CBufferElements.at(name);
		}
	}

	void Mesh::AddSubmesh(uint32_t indexCount)
	{
		Submesh& submesh = mSubmeshes.emplace_back();
		submesh.BaseVertex = mVertexCount;
		submesh.BaseIndex = mIndexCount;
		submesh.MaterialIndex = 0;
		submesh.IndexCount = indexCount;
	}

	void Mesh::Map()
	{
		// if the planet is a mesh upload Planet data to the GPU
		if (mIsPlanet)
			mPlanetCBuffer->Map(mPlanetBuffer);

		if (mModelCBuffer)
			mModelCBuffer->Map(mModelBuffer);

		if (mMaterial)
			mMaterial->Map();
	}

	void Mesh::Bind(bool environment)
	{
		mVertexBuffer->Bind();
		mIndexBuffer->Bind();
		if(mInstanceVertexBuffer)
			mInstanceVertexBuffer->Bind();

		// if the planet is a mesh upload Planet data to the GPU
		if (mIsPlanet)
			mPlanetCBuffer->Bind();

		if(mModelCBuffer)
			mModelCBuffer->Bind();

		if (mMaterial)
			mMaterial->Bind(environment);
	}

	void Mesh::SetIsPlanet(bool isPlanet)
	{
		mIsPlanet = true;

		// Setting up the constant buffer and data buffer for the planet mesh rendering
		mPlanetCBuffer = ConstantBufferLibrary::Load("Planet", 64, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_VERTEX_SHADER, 2), CBufferBindInfo(D3D11_PIXEL_SHADER, 4) });
		mPlanetCBuffer->Bind();
		mPlanetBuffer.Allocate(mPlanetCBuffer->GetSize());
		mPlanetBuffer.ZeroInitialize();
	}

}