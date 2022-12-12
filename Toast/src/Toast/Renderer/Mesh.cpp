#include "tpch.h"
#include "Mesh.h"

#include <filesystem>
#include <math.h>

#define CGLTF_IMPLEMENTATION
#include <../cgltf/include/cgltf.h>

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
			result = cgltf_load_buffers(&options, data, filePath.c_str());

			DirectX::XMFLOAT3 translation;
			DirectX::XMFLOAT4 rotation;
			DirectX::XMFLOAT3 scale;

			for (unsigned m = 0; m < data->meshes_count; m++) 
			{
				Submesh& submesh = mSubmeshes.emplace_back();
				submesh.MaterialIndex = 0;// data->meshes[m].primitives[0].material;
				submesh.MeshName = data->meshes[m].name;
				
				// TRANSFORM
				scale = data->nodes[m].has_scale ? DirectX::XMFLOAT3(data->nodes[m].scale[0], data->nodes[m].scale[1], data->nodes[m].scale[2]) : DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
				rotation = data->nodes[m].has_rotation ? DirectX::XMFLOAT4(data->nodes[m].rotation[0], data->nodes[m].rotation[1], data->nodes[m].rotation[2], data->nodes[m].rotation[3]) : DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
				translation = data->nodes[m].has_translation ? DirectX::XMFLOAT3(data->nodes[m].translation[0], data->nodes[m].translation[1], data->nodes[m].translation[2]) : DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);

				submesh.Transform = DirectX::XMMatrixIdentity() * DirectX::XMMatrixScaling(scale.x, scale.y, scale.z)
						* (DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&rotation)))
						* DirectX::XMMatrixTranslation(translation.x, translation.y, translation.z);

				for (unsigned int p = 0; p < data->meshes[m].primitives_count; p++)
				{
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
			mMaterial = MaterialLibrary::Get("Standard");

			Texture2D* whiteTexture = (Texture2D*)(TextureLibrary::Get("assets/textures/White.png"));
			if (data->materials[0].has_pbr_metallic_roughness) 
			{
				std::string materialName(data->materials[0].name);
				mMaterial = MaterialLibrary::Load(materialName, ShaderLibrary::Get("assets/shaders/ToastPBR.hlsl"));

				// ALBEDO
				DirectX::XMFLOAT4 albedoColor;
				albedoColor.x = data->materials[0].pbr_metallic_roughness.base_color_factor[0];
				albedoColor.y = data->materials[0].pbr_metallic_roughness.base_color_factor[1];
				albedoColor.z = data->materials[0].pbr_metallic_roughness.base_color_factor[2];
				albedoColor.w = data->materials[0].pbr_metallic_roughness.base_color_factor[3];

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
					//TOAST_CORE_INFO("Albedo map found: %s", completePath.c_str());	
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
					//TOAST_CORE_INFO("Normal map found: %s", completePath.c_str());
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
				float roughness = 0.0f;
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
					metalness = data->materials[0].pbr_metallic_roughness.metallic_factor;
					roughness = data->materials[0].pbr_metallic_roughness.roughness_factor;

					mMaterial->SetTexture(5, D3D11_PIXEL_SHADER, whiteTexture);
				}
				mMaterial->Set<float>("Metalness", metalness);
				mMaterial->Set<float>("Roughness", roughness);
				mMaterial->Set<int>("MetalRoughTexToggle", useMetalRoughMap);
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

		return nullptr;
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