#include "tpch.h"
#include "Mesh.h"

#include <filesystem>
#include <math.h>

namespace Toast {

	DirectX::XMMATRIX Mat4FromAssimpMat4(const aiMatrix4x4& matrix)
	{
		DirectX::XMMATRIX result = DirectX::XMMatrixTranspose(DirectX::XMMatrixSet(matrix.a1, matrix.a2, matrix.a3, matrix.a4,
			matrix.b1, matrix.b2, matrix.b3, matrix.b4,
			matrix.c1, matrix.c2, matrix.c3, matrix.c4,
			matrix.d1, matrix.d2, matrix.d3, matrix.d4));
		//the a,b,c,d in assimp is the row ; the 1,2,3,4 is the column
		return result;
	}

	static const uint32_t sMeshImportFlags =
		aiProcess_CalcTangentSpace |        // Create binormals/tangents just in case
		aiProcess_Triangulate |             // Make sure we're triangles
		aiProcess_SortByPType |             // Split meshes by primitive type
		aiProcess_GenNormals |              // Make sure we have legit normals
		aiProcess_GenUVCoords |             // Convert UVs if required 
		aiProcess_OptimizeMeshes |          // Batch draws where possible
		aiProcess_JoinIdenticalVertices |
		aiProcess_ConvertToLeftHanded |		// Convert to left hand since Toast engine is running with DirectX
		aiProcess_ValidateDataStructure;    // Validation

	Mesh::Mesh()
	{
		mMaterial = MaterialLibrary::Get("Standard");
	}

	Mesh::Mesh(const std::string& filePath)
		: mFilePath(filePath)
	{
		struct PBRData 
		{
			DirectX::XMFLOAT4 Albedo;
			float Emission;
			float Metalness;
			float Roughness;
			float UseNormalMap;
		};

		TOAST_CORE_INFO("Loading Mesh: '%s'", mFilePath.c_str());

		mImporter = std::make_unique<Assimp::Importer>();

		const aiScene* scene = mImporter->ReadFile(mFilePath, sMeshImportFlags);
		if (!scene || !scene->HasMeshes())
		{
			TOAST_CORE_ERROR("Failed to load mesh '%s'", mFilePath.c_str());
			return;
		}

		mScene = scene;

		mTransform = Mat4FromAssimpMat4(scene->mRootNode->mTransformation);

		uint32_t vertexCount = 0;
		uint32_t indexCount = 0;

		mSubmeshes.reserve(scene->mNumMeshes);
		for (unsigned m = 0; m < scene->mNumMeshes; m++) 
		{
			aiMesh* mesh = scene->mMeshes[m];

			Submesh& submesh = mSubmeshes.emplace_back();
			submesh.BaseVertex = vertexCount;
			submesh.BaseIndex = indexCount;
			submesh.MaterialIndex = mesh->mMaterialIndex;
			submesh.VertexCount = mesh->mNumVertices;
			submesh.IndexCount = mesh->mNumFaces * 3;
			submesh.MeshName = mesh->mName.C_Str();

			vertexCount += mesh->mNumVertices;
			indexCount += submesh.IndexCount;

			TOAST_CORE_ASSERT(mesh->HasPositions(), "Meshes require positions!");
			TOAST_CORE_ASSERT(mesh->HasNormals(), "Meshes require positions!");

			for (size_t i = 0; i < mesh->mNumVertices; i++) 
			{
				Vertex vertex;
				vertex.Position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
				vertex.Normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };

				if (mesh->HasTangentsAndBitangents()) 
				{
					vertex.Tangent = { mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z };
					vertex.Binormal = { mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z };
				}

				if (mesh->HasTextureCoords(0))
					vertex.Texcoord = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };

				mVertices.push_back(vertex);
			}

			for (size_t i = 0; i < mesh->mNumFaces; i++) 
			{
				TOAST_CORE_ASSERT(mesh->mFaces[i].mNumIndices == 3, "Must have 3 indices!");
				mIndices.push_back(mesh->mFaces[i].mIndices[0]);
				mIndices.push_back(mesh->mFaces[i].mIndices[1]);
				mIndices.push_back(mesh->mFaces[i].mIndices[2]);
			}
		}

		TraverseNodes(scene->mRootNode);

		//Temporary until I sort everything with material out and loading meshes from files
		mMaterial = MaterialLibrary::Get("Standard");

		mVertexBuffer = CreateRef<VertexBuffer>(&mVertices[0], (sizeof(Vertex) * (uint32_t)mVertices.size()), (uint32_t)mVertices.size(), 0);
		mIndexBuffer = CreateRef<IndexBuffer>(&mIndices[0], (uint32_t)mIndices.size());

		//Materials
		if (scene->HasMaterials())
		{
			for (uint32_t i = 0; i < scene->mNumMaterials; i++)
			{
				auto aiMaterial = scene->mMaterials[i];
				auto aiMaterialName = aiMaterial->GetName();

				mMaterial = MaterialLibrary::Load(aiMaterialName.C_Str(), ShaderLibrary::Get("ToastPBR"));

				aiString aiTexPath;

				// Albedo
				DirectX::XMFLOAT4 albedoColor = { 0.8f, 0.8f, 0.8f, 1.0f };
				aiColor3D aiColor;
				if (aiMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, aiColor) == AI_SUCCESS)
					albedoColor = { aiColor.r, aiColor.g, aiColor.b, 1.0f };

				bool hasAlbedoMap = aiMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &aiTexPath) == AI_SUCCESS;
				if (hasAlbedoMap)
				{
					std::filesystem::path path = mFilePath;
					auto parentPath = path.parent_path();
					std::string texturePath = parentPath.string();
					std::string completePath = texturePath.append("\\").append(aiTexPath.C_Str());
					//TOAST_CORE_INFO("	Albedo Map exists: %s", aiTexPath.C_Str());
					//TOAST_CORE_INFO("	Albedo map filepath: %s", completePath.c_str());
					mMaterial->SetTexture(3, D3D11_PIXEL_SHADER, TextureLibrary::LoadTexture2D(completePath.c_str()));
					albedoColor = { 1.0f, 1.0f, 1.0f, 1.0f };
				}

				// Emission
				float emission = 0.0f;
				aiColor3D aiEmission;
				if (aiMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, aiEmission) == AI_SUCCESS)
					emission = aiEmission.r;

				// Normals
				bool hasNormalMap = aiMaterial->GetTexture(aiTextureType_HEIGHT, 0, &aiTexPath) == AI_SUCCESS;
				bool useNormalMap = 0.0f;
				if (hasNormalMap)
				{
					std::filesystem::path path = mFilePath;
					auto parentPath = path.parent_path();
					std::string texturePath = parentPath.string();
					std::string completePath = texturePath.append("\\").append(aiTexPath.C_Str());
					//TOAST_CORE_INFO("	Normal Map exists: %s", aiTexPath.C_Str());
					//TOAST_CORE_INFO("	Normal map filepath: %s", completePath.c_str());
					mMaterial->SetTexture(4, D3D11_PIXEL_SHADER, TextureLibrary::LoadTexture2D(completePath.c_str()));
					useNormalMap = 1.0f;
				}

				// Metalness
				float metalness = 0.0f;
				if (aiMaterial->Get(AI_MATKEY_REFLECTIVITY, metalness) != aiReturn_SUCCESS)
					metalness = 0.0f;

				bool hasMetalnessMap = aiMaterial->GetTexture(aiTextureType_SPECULAR, 0, &aiTexPath) == AI_SUCCESS;
				if (hasMetalnessMap)
				{
					std::filesystem::path path = mFilePath;
					auto parentPath = path.parent_path();
					std::string texturePath = parentPath.string();
					std::string completePath = texturePath.append("\\").append(aiTexPath.C_Str());
					//TOAST_CORE_INFO("	Metalness Map exists: %s", aiTexPath.C_Str());
					//TOAST_CORE_INFO("	Metalness map filepath: %s", completePath.c_str());
					mMaterial->SetTexture(5, D3D11_PIXEL_SHADER, TextureLibrary::LoadTexture2D(completePath.c_str()));
					metalness = 1.0f;
				}

				// Roughness
				float shininess;
				if (aiMaterial->Get(AI_MATKEY_SHININESS, shininess) != aiReturn_SUCCESS)
					shininess = 80.0f; // Default value

				float roughness = 1.0f - sqrt(shininess / 100.0f);

				bool hasRoughnessMap = aiMaterial->GetTexture(aiTextureType_SHININESS, 0, &aiTexPath) == AI_SUCCESS;
				if (hasRoughnessMap)
				{
					std::filesystem::path path = mFilePath;
					auto parentPath = path.parent_path();
					std::string texturePath = parentPath.string();
					std::string completePath = texturePath.append("\\").append(aiTexPath.C_Str());
					//TOAST_CORE_INFO("	Roughness Map exists: %s", aiTexPath.C_Str());
					//TOAST_CORE_INFO("	Roughness map filepath: %s", completePath.c_str());
					mMaterial->SetTexture(6, D3D11_PIXEL_SHADER, TextureLibrary::LoadTexture2D(completePath.c_str()));
					roughness = 1.0f;
				}

				PBRData pbrData = { albedoColor, emission, metalness, roughness, useNormalMap };
				mMaterial->SetData("PBRData", (void*)&pbrData);
			}
		}
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

	void Mesh::InitPlanet()
	{
		mSubmeshes.clear();
		mVertexBuffer = nullptr;
		mInstanceVertexBuffer = nullptr;
		mIndexBuffer = nullptr;

		if (!mPlanetPatches.empty()) {
			mVertexBuffer = CreateRef<VertexBuffer>(&mPlanetVertices[0], (sizeof(PlanetVertex) * (uint32_t)mPlanetVertices.size()), (uint32_t)mPlanetVertices.size(), 0);
			mInstanceVertexBuffer = CreateRef<VertexBuffer>(sizeof(PlanetPatch) * 100000, 100000, 1);
			mIndexBuffer = CreateRef<IndexBuffer>(&mIndices[0], (uint32_t)mIndices.size());

			mInstanceVertexBuffer->SetData(&mPlanetPatches[0], static_cast<uint32_t>(sizeof(PlanetPatch) * mPlanetPatches.size()));
		}
	}

	void Mesh::OnUpdate(Timestep ts)
	{

	}

	void Mesh::CreateFromFile()
	{

	}

	void Mesh::AddSubmesh(uint32_t indexCount)
	{
		Submesh& submesh = mSubmeshes.emplace_back();
		submesh.BaseVertex = mVertexCount;
		submesh.BaseIndex = mIndexCount;
		submesh.MaterialIndex = 0;
		submesh.IndexCount = indexCount;
	}

	void Mesh::GeneratePlanetMesh(DirectX::XMMATRIX planetTransform, DirectX::XMVECTOR& cameraPos, int16_t subdivisions)
	{

	}

	void Mesh::TraverseNodes(aiNode* node, const DirectX::XMMATRIX& parentTransform, uint32_t level)
	{
		DirectX::XMMATRIX transform = DirectX::XMMatrixMultiply(parentTransform, Mat4FromAssimpMat4(node->mTransformation));

		mNodeMap[node].resize(node->mNumMeshes);
		for (uint32_t i = 0; i < node->mNumMeshes; i++) 
		{
			uint32_t mesh = node->mMeshes[i];
			auto& submesh = mSubmeshes[mesh];
			submesh.MeshName = node->mName.C_Str();
			submesh.Transform = transform;
			mNodeMap[node][i] = mesh;
		}

		for(uint32_t i = 0; i < node->mNumChildren; i++)
			TraverseNodes(node->mChildren[i], transform, level + 1);
	}

}