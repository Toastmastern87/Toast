#include "tpch.h"
#include "Toast/Renderer/Material.h"
#include "Toast/Renderer/RendererAPI.h"
#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/Shader.h"

#include "Toast/Core/Application.h"

#include <yaml-cpp/yaml.h>

#include <iostream>

namespace Toast {

	Material::Material(const std::string& name, const Ref<Shader>& shader)
		: mName(name), mShader(shader)
	{
		SetUpTextureBindings();
		SetUpCBufferBindings();
	}

	void Material::SetData(const std::string& name, void* data)
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		// Check to see if the constant buffer exists
		if (mConstantBuffers.find(name) == mConstantBuffers.end())
		{
			TOAST_CORE_INFO("Trying to write data to a non existent constant buffer: {0}", name.c_str());
			return;
		}

		D3D11_MAPPED_SUBRESOURCE ms;
		deviceContext->Map(mConstantBuffers[name]->GetBuffer(), NULL, D3D11_MAP_WRITE_DISCARD, NULL, &ms);
		memcpy(ms.pData, data, mConstantBuffers[name]->GetSize());
		deviceContext->Unmap(mConstantBuffers[name]->GetBuffer(), NULL);
	}

	void Material::SetShader(const Ref<Shader>& shader)
	{
		mShader = shader;

		SetUpTextureBindings();
	}

	void Material::SetTexture(std::string name, Ref<Texture2D>& texture)
	{
		mTextures[name] = texture;
		mDirty = true;
	}

	void Material::SetUpTextureBindings()
	{
		mTextures.clear();
		std::unordered_map<std::string, Shader::Texture2DDesc> textureResources = mShader->GetTextureResources();

		// Set up Textures in the material, fill it with white textures at start as default textures
		for (auto& textureResource : textureResources)
		{
			Ref<Texture2D> defaultTexture = CreateRef<Texture2D>(1, 1, textureResource.second.BindPoint, textureResource.second.ShaderType);
			uint32_t defaultTextureData = 0xffffffff;
			defaultTexture->SetData(&defaultTextureData, sizeof(uint32_t));

			mTextures[textureResource.first] = defaultTexture;
		}

		if (mTextures.size() > 0)
			mDirty = true;
	}

	void Material::SetUpCBufferBindings()
	{
		mConstantBuffers.clear();
		std::unordered_map<std::string, Shader::ConstantBufferDesc> cBufferResources = mShader->GetCBufferResources();

		for (auto& cbuffer : cBufferResources)
			mConstantBuffers[cbuffer.first] = BufferLibrary::Load(cbuffer.first, cbuffer.second.Size, cbuffer.second.ShaderType, cbuffer.second.BindPoint);
	}

	void Material::Bind()
	{
		mShader->Bind();

		for (auto& texture : mTextures) 
			texture.second->Bind();

		for (auto& cbuffer : mConstantBuffers)
			cbuffer.second->Bind();
	}

	std::unordered_map<std::string, Ref<Material>> MaterialLibrary::mMaterials;

	void MaterialLibrary::Add(std::string name, const Ref<Material>& material)
	{
		if (Exists(name))
		{
			name.append("_");
			int i = 1;
			while (Exists(name.append(std::to_string(i))))
			{
				name = name.substr(0, name.find("_")+1);
				i++;
			}

			material->SetName(name);
		}

		mMaterials[name] = material;
	}

	void MaterialLibrary::Add(const Ref<Material>& material)
	{
		auto& name = material->GetName();
		Add(name, material);
	}

	Ref<Material> MaterialLibrary::Load(const std::string& name, const Ref<Shader>& shader)
	{
		auto material = CreateRef<Material>(name, shader);
		Add(name, material);
		return material;
	}

	Toast::Ref<Toast::Material> MaterialLibrary::Load()
	{
		auto material = CreateRef<Material>("No Name", ShaderLibrary::Get("Standard"));
		Add("No Name", material);
		return material;
	}

	Ref<Material> MaterialLibrary::Get(const std::string& name)
	{
		TOAST_CORE_ASSERT(Exists(name), "Material not found!");
		return mMaterials[name];
	}

	bool MaterialLibrary::Exists(const std::string& name)
	{
		return mMaterials.find(name) != mMaterials.end();
	}

	void MaterialLibrary::ChangeName(const std::string& oldName, std::string& newName)
	{
		auto nh = mMaterials.extract(oldName);
		nh.key() = newName;
		mMaterials.insert(move(nh));
	}

	void MaterialLibrary::SerializeLibrary()
	{
		for (auto& material : mMaterials)
		{
			if (material.second->IsDirty())
				MaterialSerializer::Serialize(material.second);
		}
	}

	void MaterialSerializer::Serialize(Ref<Material>& material)
	{
		std::string name = material->GetName();

		std::string filepath = std::string("assets/materials/").append(name.append(".mtl"));

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Material" << YAML::Value << material->GetName();
		out << YAML::Key << "Shader" << YAML::Value << material->GetShader()->GetName();

		if (material->GetTextures().size() > 0) 
		{
			out << YAML::Key << "Textures" << YAML::Value << YAML::BeginSeq;
			for (auto& texture : material->GetTextures())
			{
				out << YAML::BeginMap;
				out << YAML::Key << "Name" << YAML::Value << texture.first;
				out << YAML::Key << "Path" << YAML::Value << texture.second->GetPath();
				out << YAML::Key << "BindSlot" << YAML::Value << texture.second->GetBindPoint();
				out << YAML::Key << "ShaderType" << YAML::Value << (uint32_t)texture.second->GetShaderType();
				out << YAML::EndMap;
			}
			out << YAML::EndSeq;
		}
		out << YAML::EndMap;

		std::ofstream fout(filepath);
		fout << out.c_str();

		material->SetDirty(false);
	}

	bool MaterialSerializer::Deserialize(std::vector<std::string> materials)
	{
		for (auto& materialPath : materials) 
		{
			std::ifstream stream(materialPath);
			std::stringstream strStream;

			strStream << stream.rdbuf();

			YAML::Node data = YAML::Load(strStream.str());
			if (!data["Material"])
				continue;

			std::string materialName = data["Material"].as<std::string>();
			std::string shaderName = data["Shader"].as<std::string>();

			TOAST_CORE_TRACE("Deserializing material '{0}', Shader : '{1}'", materialName, shaderName);

			auto& material = MaterialLibrary::Load(materialName, ShaderLibrary::Get(shaderName));

			auto textures = data["Textures"];
			if (textures)
			{
				for (auto texture : textures)
				{
					std::string path = texture["Path"].as<std::string>();

					if(!path.empty())
						material->SetTexture(texture["Name"].as<std::string>(), CreateRef<Texture2D>(texture["Path"].as<std::string>(), texture["BindSlot"].as<uint32_t>(), (D3D11_SHADER_TYPE)texture["ShaderType"].as<uint32_t>()));
					else 
					{
						Ref<Texture2D> defaultTexture = CreateRef<Texture2D>(1, 1, texture["BindSlot"].as<uint32_t>(), (D3D11_SHADER_TYPE)texture["ShaderType"].as<uint32_t>());
						uint32_t defaultTextureData = 0xffffffff;
						defaultTexture->SetData(&defaultTextureData, sizeof(uint32_t));

						material->SetTexture(texture["Name"].as<std::string>(), defaultTexture);
					}
				}
			}
		}

		return true;
	}

}