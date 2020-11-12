#include "tpch.h"
#include "Toast/Renderer/Material.h"

#include <yaml-cpp/yaml.h>

#include <iostream>

namespace Toast {

	Material::Material(const std::string& name, const Ref<Shader>& shader)
		: mName(name), mShader(shader)
	{
		SetUpTextureBindings();
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
		std::unordered_map<std::string, uint32_t> textureResources = mShader->GetTextureResources();

		// Set up Textures in the material, fill it with white textures at start as default textures
		for (auto& textureResource : textureResources)
		{
			Ref<Texture2D> defaultTexture = CreateRef<Texture2D>(1, 1, textureResource.second);
			uint32_t defaultTextureData = 0xffffffff;
			defaultTexture->SetData(&defaultTextureData, sizeof(uint32_t));

			mTextures[textureResource.first] = defaultTexture;
		}

		if (mTextures.size() > 0)
			mDirty = true;
	}

	void Material::Bind()
	{
		mShader->Bind();

		for (auto& texture : mTextures) 
		{
			texture.second->Bind();
		}
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
					material->SetTexture(texture["Name"].as<std::string>(), CreateRef<Texture2D>(texture["Path"].as<std::string>(), texture["BindSlot"].as<uint32_t>()));
				}
			}
		}

		return true;
	}

}