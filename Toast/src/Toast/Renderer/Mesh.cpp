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
		aiProcess_FlipWindingOrder |
		aiProcess_ConvertToLeftHanded |		// Convert to left hand since Toast engine is running with DirectX
		aiProcess_ValidateDataStructure;    // Validation

	Mesh::Mesh()
	{
		mMaterial = MaterialLibrary::Get("Standard");

		// Setting up the constant buffer and data buffer for the mesh rendering
		mModelCBuffer = ConstantBufferLibrary::Load("Model", 80, D3D11_VERTEX_SHADER, 1);
		mModelCBuffer->Bind();
		mModelBuffer.Allocate(mModelCBuffer->GetSize());
		mModelBuffer.ZeroInitialize();

		//TOAST_CORE_INFO("Mesh Initialized!");
	}

	Mesh::Mesh(const std::string& filePath, const bool skyboxMesh)
		: mFilePath(filePath)
	{
		// Setting up the constant buffer and data buffer for the mesh rendering
		mModelCBuffer = ConstantBufferLibrary::Load("Model", 80, D3D11_VERTEX_SHADER, 1);
		mModelCBuffer->Bind();
		mModelBuffer.Allocate(mModelCBuffer->GetSize());
		mModelBuffer.ZeroInitialize();

		if(!skyboxMesh)
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
		Texture2D* whiteTexture = (Texture2D*)(TextureLibrary::Get("assets/textures/White.png"));
		if (scene->HasMaterials())
		{
			for (uint32_t i = 0; i < scene->mNumMaterials; i++)
			{
				auto aiMaterial = scene->mMaterials[i];
				auto aiMaterialName = aiMaterial->GetName();

				mMaterial = MaterialLibrary::Load(aiMaterialName.C_Str(), ShaderLibrary::Get("assets/shaders/ToastPBR.hlsl"));

				aiString aiTexPath;

				// Albedo
				DirectX::XMFLOAT4 albedoColor = { 0.8f, 0.8f, 0.8f, 1.0f };
				aiColor3D aiColor;
				if (aiMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, aiColor) == AI_SUCCESS) 
				{
					//TOAST_CORE_INFO("	Albedo Color exists: %f, %f, %f, %f", aiColor.r, aiColor.g, aiColor.b, 1.0f);
					albedoColor = { aiColor.r, aiColor.g, aiColor.b, 1.0f };
				}

				bool hasAlbedoMap = aiMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &aiTexPath) == AI_SUCCESS;
				int useAlbedoMap = 0;
				if (hasAlbedoMap)
				{
					std::filesystem::path path = mFilePath;
					auto parentPath = path.parent_path();
					std::string texturePath = parentPath.string();
					std::string completePath = texturePath.append("\\").append(aiTexPath.C_Str());
					//TOAST_CORE_INFO("	Albedo Map exists: %s", aiTexPath.C_Str());
					//TOAST_CORE_INFO("	Albedo map filepath: %s", completePath.c_str());
					albedoColor = { 1.0f, 1.0f, 1.0f, 1.0f };
					useAlbedoMap = 1;
					mMaterial->SetTexture(3, D3D11_PIXEL_SHADER, TextureLibrary::LoadTexture2D(completePath.c_str()));
				}
				else
				{
					mMaterial->SetTexture(3, D3D11_PIXEL_SHADER, whiteTexture);
				}
				mMaterial->Set<DirectX::XMFLOAT4>("Albedo", albedoColor);
				mMaterial->Set<int>("AlbedoTexToggle", useAlbedoMap);

				// Emission
				float emission = 0.0f;
				aiColor3D aiEmission;
				if (aiMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, aiEmission) == AI_SUCCESS) 
					emission = aiEmission.r;
				
				mMaterial->Set<float>("Emission", emission);

				// Normals
				bool hasNormalMap = aiMaterial->GetTexture(aiTextureType_HEIGHT, 0, &aiTexPath) == AI_SUCCESS;
				int useNormalMap = 0;
				if (hasNormalMap)
				{
					std::filesystem::path path = mFilePath;
					auto parentPath = path.parent_path();
					std::string texturePath = parentPath.string();
					std::string completePath = texturePath.append("\\").append(aiTexPath.C_Str());
					//TOAST_CORE_INFO("	Normal Map exists: %s", aiTexPath.C_Str());
					//TOAST_CORE_INFO("	Normal map filepath: %s", completePath.c_str());
					useNormalMap = 1;
					mMaterial->SetTexture(4, D3D11_PIXEL_SHADER, TextureLibrary::LoadTexture2D(completePath.c_str()));
				}
				else
				{
					mMaterial->SetTexture(4, D3D11_PIXEL_SHADER, whiteTexture);
				}
				mMaterial->Set<int>("NormalTexToggle", useNormalMap);

				// Metalness
				float metalness = 0.0f;
				if (aiMaterial->Get(AI_MATKEY_REFLECTIVITY, metalness) != aiReturn_SUCCESS)
					metalness = 0.0f;
				else 
				{
					//TOAST_CORE_INFO("	metalvalue: %f", metalness);
				}

				bool hasMetalnessMap = aiMaterial->GetTexture(aiTextureType_SPECULAR, 0, &aiTexPath) == AI_SUCCESS;
				int useMetalnessMap = 0;
				if (hasMetalnessMap)
				{
					std::filesystem::path path = mFilePath;
					auto parentPath = path.parent_path();
					std::string texturePath = parentPath.string();
					std::string completePath = texturePath.append("\\").append(aiTexPath.C_Str());
					//TOAST_CORE_INFO("	Metalness Map exists: %s", aiTexPath.C_Str());
					//TOAST_CORE_INFO("	Metalness map filepath: %s", completePath.c_str());
					metalness = 1.0f;
					useMetalnessMap = 1;
					mMaterial->SetTexture(5, D3D11_PIXEL_SHADER, TextureLibrary::LoadTexture2D(completePath.c_str()));
				}
				else
				{
					mMaterial->SetTexture(5, D3D11_PIXEL_SHADER, whiteTexture);
				}
				mMaterial->Set<float>("Metalness", metalness);
				mMaterial->Set<int>("MetalnessTexToggle", useMetalnessMap);

				// Roughness
				float shininess;
				if (aiMaterial->Get(AI_MATKEY_SHININESS, shininess) != aiReturn_SUCCESS)
					shininess = 80.0f; // Default value

				float roughness = 1.0f - sqrt(shininess / 100.0f);
				//TOAST_CORE_INFO("	roughness: %f", roughness);

				bool hasRoughnessMap = aiMaterial->GetTexture(aiTextureType_SHININESS, 0, &aiTexPath) == AI_SUCCESS;
				int useRoughnessMap = 0;
				if (hasRoughnessMap)
				{
					std::filesystem::path path = mFilePath;
					auto parentPath = path.parent_path();
					std::string texturePath = parentPath.string();
					std::string completePath = texturePath.append("\\").append(aiTexPath.C_Str());
					//TOAST_CORE_INFO("	Roughness Map exists: %s", aiTexPath.C_Str());
					//TOAST_CORE_INFO("	Roughness map filepath: %s", completePath.c_str());
					roughness = 1.0f;
					useRoughnessMap = 1;
					mMaterial->SetTexture(6, D3D11_PIXEL_SHADER, TextureLibrary::LoadTexture2D(completePath.c_str()));
				}
				else
				{
					mMaterial->SetTexture(6, D3D11_PIXEL_SHADER, whiteTexture);
				}
				mMaterial->Set<float>("Roughness", roughness);
				mMaterial->Set<int>("RoughnessTexToggle", useRoughnessMap);
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

	void Mesh::Map()
	{
		// if the planet is a mesh upload Planet data to the GPU
		if (mIsPlanet)
		{
			mPlanetCBuffer->Map(mPlanetBuffer);
			mPlanetPSCBuffer->Map(mPlanetPSBuffer);
		}

		if (mModelCBuffer)
			mModelCBuffer->Map(mModelBuffer);

		if (mMaterial)
			mMaterial->Map();
	}

	void Mesh::Bind()
	{
		// if the planet is a mesh upload Planet data to the GPU
		if (mIsPlanet)
		{
			mPlanetCBuffer->Bind();
			mPlanetPSCBuffer->Bind();
		}

		if(mModelCBuffer)
			mModelCBuffer->Bind();

		if (mMaterial)
			mMaterial->Bind();
	}

	void Mesh::SetIsPlanet(bool isPlanet)
	{
		mIsPlanet = true;

		// Setting up the constant buffer and data buffer for the planet mesh rendering
		mPlanetCBuffer = ConstantBufferLibrary::Load("Planet", 48, D3D11_VERTEX_SHADER, 2);
		mPlanetCBuffer->Bind();
		mPlanetBuffer.Allocate(mPlanetCBuffer->GetSize());
		mPlanetBuffer.ZeroInitialize();

		// Setting up the constant buffer and data buffer for the planet mesh rendering
		mPlanetPSCBuffer = ConstantBufferLibrary::Load("PlanetPS", 48, D3D11_PIXEL_SHADER, 4);
		mPlanetPSCBuffer->Bind();
		mPlanetPSBuffer.Allocate(mPlanetPSCBuffer->GetSize());
		mPlanetPSBuffer.ZeroInitialize();
	}

}