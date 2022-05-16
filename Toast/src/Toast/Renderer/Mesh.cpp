#include "tpch.h"
#include "Mesh.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#include <filesystem>
#include <math.h>

namespace Toast {

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
				submesh.MaterialIndex = 0;
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

							if ((attribute->component_type == cgltf_component_type_r_32f) && (attribute->type == cgltf_type_vec3))
							{

								TOAST_CORE_INFO("Mesh postion data correct format");
								vertexCount = (int)attribute->count;
								mVertices.resize(vertexCount);
								TOAST_CORE_INFO("Attribute count: %d", (int)attribute->count);
							
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

			cgltf_free(data);

			TOAST_CORE_INFO("Mesh loaded!");
			for each (Vertex v in mVertices)
				TOAST_CORE_INFO("Vertex x: %f, y: %f, z: %f", v.Position.x, v.Position.y, v.Position.z);
			for each (uint16_t i in mIndices)
				TOAST_CORE_INFO("Indices: %d ", i);

			//Temporary until I sort everything with material out and loading meshes from files
			mMaterial = MaterialLibrary::Get("Standard");

			mVertexBuffer = CreateRef<VertexBuffer>(&mVertices[0], (sizeof(Vertex) * (uint32_t)mVertices.size()), (uint32_t)mVertices.size(), 0);
			mIndexBuffer = CreateRef<IndexBuffer>(&mIndices[0], (uint32_t)mIndices.size());
		}

		//mImporter = std::make_unique<Assimp::Importer>();

		//const aiScene* scene = mImporter->ReadFile(mFilePath, sMeshImportFlags);
		//if (!scene || !scene->HasMeshes())
		//{
		//	TOAST_CORE_ERROR("Failed to load mesh '%s'", mFilePath.c_str());
		//	return;
		//}

		//mScene = scene;

		//mTransform = Mat4FromAssimpMat4(scene->mRootNode->mTransformation);

		//uint32_t vertexCount = 0;
		//uint32_t indexCount = 0;

		//mSubmeshes.reserve(scene->mNumMeshes);
		//for (unsigned m = 0; m < scene->mNumMeshes; m++) 
		//{
		//	aiMesh* mesh = scene->mMeshes[m];

		//	Submesh& submesh = mSubmeshes.emplace_back();
		//	submesh.BaseVertex = vertexCount;
		//	submesh.BaseIndex = indexCount;
		//	submesh.MaterialIndex = mesh->mMaterialIndex;
		//	submesh.VertexCount = mesh->mNumVertices;
		//	submesh.IndexCount = mesh->mNumFaces * 3;
		//	submesh.MeshName = mesh->mName.C_Str();

		//	vertexCount += mesh->mNumVertices;
		//	indexCount += submesh.IndexCount;

		//	TOAST_CORE_ASSERT(mesh->HasPositions(), "Meshes require positions!");
		//	TOAST_CORE_ASSERT(mesh->HasNormals(), "Meshes require positions!");

		//	for (size_t i = 0; i < mesh->mNumVertices; i++) 
		//	{
		//		Vertex vertex;
		//		vertex.Position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
		//		vertex.Normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };

		//		if (mesh->HasTangentsAndBitangents()) 
		//		{
		//			vertex.Tangent = { mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z };
		//			vertex.Binormal = { mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z };
		//		}

		//		if (mesh->HasTextureCoords(0))
		//			vertex.Texcoord = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };

		//		mVertices.push_back(vertex);
		//	}

		//	for (size_t i = 0; i < mesh->mNumFaces; i++) 
		//	{
		//		TOAST_CORE_ASSERT(mesh->mFaces[i].mNumIndices == 3, "Must have 3 indices!");
		//		mIndices.push_back(mesh->mFaces[i].mIndices[0]);
		//		mIndices.push_back(mesh->mFaces[i].mIndices[1]);
		//		mIndices.push_back(mesh->mFaces[i].mIndices[2]);
		//	}
		//}

		//TraverseNodes(scene->mRootNode);

		////Temporary until I sort everything with material out and loading meshes from files
		//mMaterial = MaterialLibrary::Get("Standard");

		//mVertexBuffer = CreateRef<VertexBuffer>(&mVertices[0], (sizeof(Vertex) * (uint32_t)mVertices.size()), (uint32_t)mVertices.size(), 0);
		//mIndexBuffer = CreateRef<IndexBuffer>(&mIndices[0], (uint32_t)mIndices.size());

		////Materials
		//Texture2D* whiteTexture = (Texture2D*)(TextureLibrary::Get("assets/textures/White.png"));
		//if (scene->HasMaterials())
		//{
		//	for (uint32_t i = 0; i < scene->mNumMaterials; i++)
		//	{
		//		auto aiMaterial = scene->mMaterials[i];
		//		auto aiMaterialName = aiMaterial->GetName();

		//		mMaterial = MaterialLibrary::Load(aiMaterialName.C_Str(), ShaderLibrary::Get("assets/shaders/ToastPBR.hlsl"));

		//		aiString aiTexPath;

		//		// Albedo
		//		DirectX::XMFLOAT4 albedoColor = { 0.8f, 0.8f, 0.8f, 1.0f };
		//		aiColor3D aiColor;
		//		if (aiMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, aiColor) == AI_SUCCESS) 
		//		{
		//			//TOAST_CORE_INFO("	Albedo Color exists: %f, %f, %f, %f", aiColor.r, aiColor.g, aiColor.b, 1.0f);
		//			albedoColor = { aiColor.r, aiColor.g, aiColor.b, 1.0f };
		//		}

		//		bool hasAlbedoMap = aiMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &aiTexPath) == AI_SUCCESS;
		//		int useAlbedoMap = 0;
		//		if (hasAlbedoMap)
		//		{
		//			std::filesystem::path path = mFilePath;
		//			auto parentPath = path.parent_path();
		//			std::string texturePath = parentPath.string();
		//			std::string completePath = texturePath.append("\\").append(aiTexPath.C_Str());
		//			//TOAST_CORE_INFO("	Albedo Map exists: %s", aiTexPath.C_Str());
		//			//TOAST_CORE_INFO("	Albedo map filepath: %s", completePath.c_str());
		//			albedoColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		//			useAlbedoMap = 1;
		//			mMaterial->SetTexture(3, D3D11_PIXEL_SHADER, TextureLibrary::LoadTexture2D(completePath.c_str()));
		//		}
		//		else
		//		{
		//			mMaterial->SetTexture(3, D3D11_PIXEL_SHADER, whiteTexture);
		//		}
		//		mMaterial->Set<DirectX::XMFLOAT4>("Albedo", albedoColor);
		//		mMaterial->Set<int>("AlbedoTexToggle", useAlbedoMap);

		//		// Emission
		//		float emission = 0.0f;
		//		aiColor3D aiEmission;
		//		if (aiMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, aiEmission) == AI_SUCCESS) 
		//			emission = aiEmission.r;
		//		
		//		mMaterial->Set<float>("Emission", emission);

		//		// Normals
		//		bool hasNormalMap = aiMaterial->GetTexture(aiTextureType_HEIGHT, 0, &aiTexPath) == AI_SUCCESS;
		//		int useNormalMap = 0;
		//		if (hasNormalMap)
		//		{
		//			std::filesystem::path path = mFilePath;
		//			auto parentPath = path.parent_path();
		//			std::string texturePath = parentPath.string();
		//			std::string completePath = texturePath.append("\\").append(aiTexPath.C_Str());
		//			//TOAST_CORE_INFO("	Normal Map exists: %s", aiTexPath.C_Str());
		//			//TOAST_CORE_INFO("	Normal map filepath: %s", completePath.c_str());
		//			useNormalMap = 1;
		//			mMaterial->SetTexture(4, D3D11_PIXEL_SHADER, TextureLibrary::LoadTexture2D(completePath.c_str()));
		//		}
		//		else
		//		{
		//			mMaterial->SetTexture(4, D3D11_PIXEL_SHADER, whiteTexture);
		//		}
		//		mMaterial->Set<int>("NormalTexToggle", useNormalMap);

		//		// Metalness
		//		float metalness = 0.0f;
		//		if (aiMaterial->Get(AI_MATKEY_REFLECTIVITY, metalness) != aiReturn_SUCCESS)
		//			metalness = 0.0f;
		//		else 
		//		{
		//			//TOAST_CORE_INFO("	metalvalue: %f", metalness);
		//		}

		//		bool hasMetalnessMap = aiMaterial->GetTexture(aiTextureType_SPECULAR, 0, &aiTexPath) == AI_SUCCESS;
		//		int useMetalnessMap = 0;
		//		if (hasMetalnessMap)
		//		{
		//			std::filesystem::path path = mFilePath;
		//			auto parentPath = path.parent_path();
		//			std::string texturePath = parentPath.string();
		//			std::string completePath = texturePath.append("\\").append(aiTexPath.C_Str());
		//			//TOAST_CORE_INFO("	Metalness Map exists: %s", aiTexPath.C_Str());
		//			//TOAST_CORE_INFO("	Metalness map filepath: %s", completePath.c_str());
		//			metalness = 1.0f;
		//			useMetalnessMap = 1;
		//			mMaterial->SetTexture(5, D3D11_PIXEL_SHADER, TextureLibrary::LoadTexture2D(completePath.c_str()));
		//		}
		//		else
		//		{
		//			mMaterial->SetTexture(5, D3D11_PIXEL_SHADER, whiteTexture);
		//		}
		//		mMaterial->Set<float>("Metalness", metalness);
		//		mMaterial->Set<int>("MetalnessTexToggle", useMetalnessMap);

		//		// Roughness
		//		float shininess;
		//		if (aiMaterial->Get(AI_MATKEY_SHININESS, shininess) != aiReturn_SUCCESS)
		//			shininess = 80.0f; // Default value

		//		float roughness = 1.0f - sqrt(shininess / 100.0f);
		//		//TOAST_CORE_INFO("	roughness: %f", roughness);

		//		bool hasRoughnessMap = aiMaterial->GetTexture(aiTextureType_SHININESS, 0, &aiTexPath) == AI_SUCCESS;
		//		int useRoughnessMap = 0;
		//		if (hasRoughnessMap)
		//		{
		//			std::filesystem::path path = mFilePath;
		//			auto parentPath = path.parent_path();
		//			std::string texturePath = parentPath.string();
		//			std::string completePath = texturePath.append("\\").append(aiTexPath.C_Str());
		//			//TOAST_CORE_INFO("	Roughness Map exists: %s", aiTexPath.C_Str());
		//			//TOAST_CORE_INFO("	Roughness map filepath: %s", completePath.c_str());
		//			roughness = 1.0f;
		//			useRoughnessMap = 1;
		//			mMaterial->SetTexture(6, D3D11_PIXEL_SHADER, TextureLibrary::LoadTexture2D(completePath.c_str()));
		//		}
		//		else
		//		{
		//			mMaterial->SetTexture(6, D3D11_PIXEL_SHADER, whiteTexture);
		//		}
		//		mMaterial->Set<float>("Roughness", roughness);
		//		mMaterial->Set<int>("RoughnessTexToggle", useRoughnessMap);
		//	}
		//}
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