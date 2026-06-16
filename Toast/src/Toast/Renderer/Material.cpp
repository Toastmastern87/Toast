#include "tpch.h"

#include "Toast/Assets/AssetManager.h"
#include "Toast/Assets/AssetSerializer.h"

#include "Toast/Renderer/Material.h"
#include "Toast/Renderer/RendererAPI.h"
#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/Shader.h"

#include "Toast/Core/Application.h"

#include "Toast/Utils/PlatformUtils.h"

#include <yaml-cpp/yaml.h>

namespace YAML {

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

	template<>
	struct convert<Toast::UUID>
	{
		static Node encode(const Toast::UUID& uuid)
		{
			Node node;
			node.push_back((uint64_t)uuid);
			return node;
		}

		static bool decode(const Node& node, Toast::UUID& uuid)
		{
			uuid = node.as<uint64_t>();
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
	}

	Material::Material(const std::string& name)
		: mName(name)
	{
	}

	Material::Material(const std::filesystem::path& tmtlPath, FromFile)
	{
		std::ifstream stream(tmtlPath);
		TOAST_CORE_ASSERT(stream.is_open(), "Material: could not open .tmtl");

		std::stringstream strStream;
		strStream << stream.rdbuf();
		YAML::Node data = YAML::Load(strStream.str());

		TOAST_CORE_ASSERT(data["Material"], "Material .tmtl missing 'Material' key");

		mName = data["Material"].as<std::string>();

		SetAlbedo(data["Albedo"].as<DirectX::XMFLOAT4>());
		SetMetalness(data["Metalness"].as<float>());
		SetRoughness(data["Roughness"].as<float>());

		// Guard Emission so .tmtl files written before the Emission fix still load.
		if (data["Emission"])
			SetEmission(data["Emission"].as<float>());

		SetUseAlbedo(data["UseAlbedoMap"].as<bool>());
		SetUseNormal(data["UseNormalMap"].as<bool>());
		SetUseMetalRough(data["UseMetalRoughMap"].as<bool>());

		if (GetUseAlbedo())
			SetAlbedolAssetHandle(data["AlbedoAssetHandle"].as<AssetHandle>());
		if (GetUseNormal())
			SetNormalAssetHandle(data["NormalAssetHandle"].as<AssetHandle>());
		if (GetUseMetalRough())
			SetMetalRoughAssetHandle(data["MetalRoughAssetHandle"].as<AssetHandle>());
	}

	Material::Material(const std::string& name,
		const DirectX::XMFLOAT4& albedo, float emission, float metalness, float roughness,
		bool useAlbedo, bool useNormal, bool useMetalRough,
		AssetHandle albedoTex, AssetHandle normalTex, AssetHandle metalRoughTex) 
	{
	}

	void Material::SaveToFile(const std::filesystem::path& tmtlPath) const
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Material" << YAML::Value << mName;
		out << YAML::Key << "Albedo" << YAML::Value << mPBRParameters.Albedo;
		out << YAML::Key << "Emission" << YAML::Value << mPBRParameters.Emission; 
		out << YAML::Key << "Metalness" << YAML::Value << mPBRParameters.Metalness;
		out << YAML::Key << "Roughness" << YAML::Value << mPBRParameters.Roughness;
		out << YAML::Key << "UseAlbedoMap" << YAML::Value << GetUseAlbedo();
		out << YAML::Key << "UseNormalMap" << YAML::Value << GetUseNormal();
		out << YAML::Key << "UseMetalRoughMap" << YAML::Value << GetUseMetalRough();
		if (GetUseAlbedo())
			out << YAML::Key << "AlbedoAssetHandle" << YAML::Value << mAlbedoTextureHandle;
		if (GetUseNormal())
			out << YAML::Key << "NormalAssetHandle" << YAML::Value << mNormalTextureHandle;
		if (GetUseMetalRough())
			out << YAML::Key << "MetalRoughAssetHandle" << YAML::Value << mMetalRoughTextureHandle;
		out << YAML::EndMap;

		std::ofstream fout(tmtlPath);
		fout << out.c_str();
	}

}