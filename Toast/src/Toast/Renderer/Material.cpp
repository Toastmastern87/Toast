#include "tpch.h"
#include "Toast/Renderer/Material.h"
#include "Toast/Renderer/RendererAPI.h"
#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/Shader.h"

#include "Toast/Core/Application.h"

#include "Toast/Utils/PlatformUtils.h"

#include <yaml-cpp/yaml.h>

namespace YAML
{
	template<>
	struct convert<DirectX::XMFLOAT3>
	{
		static Node encode(const DirectX::XMFLOAT3& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, DirectX::XMFLOAT3& rhs)
		{
			if (!node.IsSequence() || node.size() != 3)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			return true;
		}
	};

	template<>
	struct convert<DirectX::XMFLOAT4>
	{
		static Node encode(const DirectX::XMFLOAT4& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.push_back(rhs.w);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, DirectX::XMFLOAT4& rhs)
		{
			if (!node.IsSequence() || node.size() != 4)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			rhs.w = node[3].as<float>();
			return true;
		}
	};

	YAML::Emitter& operator<<(YAML::Emitter& out, const DirectX::XMFLOAT3& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
		return out;
	}

	YAML::Emitter& operator<<(YAML::Emitter& out, const DirectX::XMFLOAT4& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
		return out;
	}
}

namespace Toast {

	Material::Material()
	{
		mAlbedoTexture = dynamic_cast<Texture2D*>(TextureLibrary::Get("assets/textures/Checkerboard.png"));
		mNormalTexture = dynamic_cast<Texture2D*>(TextureLibrary::Get("assets/textures/Checkerboard.png"));
		mMetalRoughTexture = dynamic_cast<Texture2D*>(TextureLibrary::Get("assets/textures/Checkerboard.png"));
	}

	Material::Material(const std::string& name)
		: mName(name)
	{
		mAlbedoTexture = dynamic_cast<Texture2D*>(TextureLibrary::Get("assets/textures/Checkerboard.png"));
		mNormalTexture = dynamic_cast<Texture2D*>(TextureLibrary::Get("assets/textures/Checkerboard.png"));
		mMetalRoughTexture = dynamic_cast<Texture2D*>(TextureLibrary::Get("assets/textures/Checkerboard.png"));
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

	Ref<Material>& MaterialLibrary::Load(const std::string& name, bool serialize)
	{
		auto it = mMaterials.find(name);
		if (it == mMaterials.end())
		{
			auto material = CreateRef<Material>(name);
			mMaterials[name] = material;

			if (serialize)
				MaterialSerializer::Serialize(material);

			return mMaterials[name];
		}
		else
			return it->second;
	}

	Ref<Material>& MaterialLibrary::Load()
	{
		auto material = CreateRef<Material>("New Material");
		Add("New Material", material);
		return material;
	}

	Ref<Material>& MaterialLibrary::Get(const std::string& name)
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

		std::string basepath = "..\\Toaster\\assets\\materials\\";
		std::string fullPath = basepath.append(oldName).append(".tmtl");

		FileDialogs::DeleteFile(fullPath);
	}

	void MaterialLibrary::SerializeLibrary()
	{
		for (auto& material : mMaterials)
			MaterialSerializer::Serialize(material.second);
	}

	void MaterialSerializer::Serialize(Ref<Material>& material)
	{
		std::string name = material->GetName();

		std::string filepath = std::string("assets/materials/").append(name.append(".tmtl"));

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Material" << YAML::Value << material->GetName();

		out << YAML::Key << "Albedo" << YAML::Value << material->GetAlbedo();
		out << YAML::Key << "Metalness" << YAML::Value << material->GetMetalness();
		out << YAML::Key << "Roughness" << YAML::Value << material->GetRoughness();
		out << YAML::Key << "UseAlbedoMap" << YAML::Value << material->GetUseAlbedo();
		out << YAML::Key << "UseNormalMap" << YAML::Value << material->GetUseNormal();
		out << YAML::Key << "UseMetalRoughMap" << YAML::Value << material->GetUseMetalRough();
		if(material->GetUseAlbedo())
			out << YAML::Key << "AlbedoAssetPath" << YAML::Value << material->GetAlbedoTexture()->GetFilePath();
		if (material->GetUseNormal())
			out << YAML::Key << "NormalAssetPath" << YAML::Value << material->GetNormalTexture()->GetFilePath();
		if (material->GetUseMetalRough())
			out << YAML::Key << "MetalRoughAssetPath" << YAML::Value << material->GetMetalRoughTexture()->GetFilePath();

		out << YAML::EndMap;

		std::ofstream fout(filepath);
		fout << out.c_str();
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

			TOAST_CORE_TRACE("Deserializing material '%s'", materialName.c_str());

			auto& material = MaterialLibrary::Load(materialName);

			material->SetAlbedo(data["Albedo"].as<DirectX::XMFLOAT4>());
			material->SetMetalness(data["Metalness"].as<float>());
			material->SetRoughness(data["Roughness"].as<float>());
			material->SetUseAlbedo(data["UseAlbedoMap"].as<bool>());
			material->SetUseNormal(data["UseNormalMap"].as<bool>());
			material->SetUseMetalRough(data["UseMetalRoughMap"].as<bool>());
			if (material->GetUseAlbedo()) 
				material->SetAlbedoTexture(TextureLibrary::LoadTexture2D(data["AlbedoAssetPath"].as<std::string>()));
			if(material->GetUseNormal())
				material->SetNormalTexture(TextureLibrary::LoadTexture2D(data["NormalAssetPath"].as<std::string>()));
			if(material->GetUseMetalRough())
				material->SetMetalRoughTexture(TextureLibrary::LoadTexture2D(data["MetalRoughAssetPath"].as<std::string>()));
		}

		return true;
	}

}