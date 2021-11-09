#include "tpch.h"
#include "Toast/Renderer/Material.h"
#include "Toast/Renderer/RendererAPI.h"
#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/Shader.h"

#include "Toast/Core/Application.h"

#include "Toast/Utils/PlatformUtils.h"

#include <yaml-cpp/yaml.h>

namespace Toast {

	Material::Material(const std::string& name, const Ref<Shader>& shader)
		: mName(name), mShader(shader)
	{
		SetUpResourceBindings();
	}

	bool& Material::GetBool(const std::string& name)
	{
		return Get<bool>(name);
	}

	int& Material::GetInt(const std::string& name)
	{
		return Get<int>(name);
	}

	float& Material::GetFloat(const std::string& name)
	{
		return Get<float>(name);
	}

	DirectX::XMFLOAT2& Material::GetFloat2(const std::string& name)
	{
		return Get<DirectX::XMFLOAT2>(name);
	}

	DirectX::XMFLOAT3& Material::GetFloat3(const std::string& name)
	{
		return Get<DirectX::XMFLOAT3>(name);
	}

	DirectX::XMFLOAT4& Material::GetFloat4(const std::string& name)
	{
		return Get<DirectX::XMFLOAT4>(name);
	}

	void Material::SetShader(const Ref<Shader>& shader)
	{
		mShader = shader;

		SetUpResourceBindings();
	}

	void Material::SetTexture(uint32_t bindslot, D3D11_SHADER_TYPE shaderType, Ref<Texture2D>& texture)
	{
		for (auto& texturebinding : mTextureBindings)
		{
			if (texturebinding.BindSlot == bindslot && texturebinding.ShaderType == shaderType)
				texturebinding.Texture = texture;
		}
	}

	void Material::SetTexture(uint32_t bindslot, D3D11_SHADER_TYPE shaderType, Ref<TextureCube>& texture)
	{
		for (auto& texturebinding : mTextureBindings)
		{
			if (texturebinding.BindSlot == bindslot && texturebinding.ShaderType == shaderType)
				texturebinding.Texture = texture;
		}
	}

	Ref<Texture> Material::GetTexture(std::string name)
	{
		for (auto& textureBinding : mTextureBindings)
		{
			std::string textureName = mShader->GetResourceName(Shader::BindingType::Texture, textureBinding.BindSlot, textureBinding.ShaderType);

			if (textureName == name)
				return textureBinding.Texture;
		}
	}

	void Material::SetTextureSampler(uint32_t bindslot, D3D11_SHADER_TYPE shaderType, Ref<TextureSampler>& sampler)
	{
		for (auto& textureSampler : mSamplerBindings)
		{
			if (textureSampler.BindSlot == bindslot && textureSampler.ShaderType == shaderType)
				textureSampler.Sampler = sampler;
		}
	}

	void Material::SetUpResourceBindings()
	{
		std::vector<Shader::ResourceBindingDesc> resourceBindings = mShader->GetResourceBindings();
		std::vector<Shader::CBufferElementBindingDesc> cbufferElementBindingDesc = mShader->GetCBufferElementBindings("Material");
		std::unordered_map<std::string, ShaderCBufferBindingDesc> cbufferBindings = mShader->GetCBuffersBindings();

		mTextureBindings.clear();
		mSamplerBindings.clear();

		// Skybox work around, need a better solution to handle this
		if (cbufferBindings.find("Material") != cbufferBindings.end())
		{
			mMaterialBuffer.Allocate(cbufferBindings.at("Material").Size);
			mMaterialBuffer.ZeroInitialize();

			mMaterialCBuffer = ConstantBufferLibrary::Load(cbufferBindings.at("Material").Name, cbufferBindings.at("Material").Size, cbufferBindings.at("Material").ShaderType, cbufferBindings.at("Material").BindPoint);
		}

		for (auto& resource : resourceBindings)
		{
			switch (resource.Type)
			{
			case Shader::BindingType::Texture:
			{
				mTextureBindings.push_back(TextureBindInfo{ resource.Shader, resource.BindPoint, nullptr });
				break;
			}
			case Shader::BindingType::Sampler:
			{
				mSamplerBindings.push_back(SamplerBindInfo{ resource.Shader, resource.BindPoint, TextureLibrary::GetSampler("Default") });
				break;
			}
			}
		}
	}

	void Material::Map()
	{
		if(mMaterialCBuffer)
			mMaterialCBuffer->Map(mMaterialBuffer);
	}

	void Material::Bind()
	{
		mShader->Bind();

		for (auto& textureBinding : mTextureBindings)
		{
			if (textureBinding.Texture) 
				textureBinding.Texture->Bind(textureBinding.BindSlot, textureBinding.ShaderType);
		}

		for (auto& samplerBinding : mSamplerBindings)
		{
			if (samplerBinding.Sampler)
				samplerBinding.Sampler->Bind(samplerBinding.BindSlot, samplerBinding.ShaderType);
		}

		if(mMaterialCBuffer)
			mMaterialCBuffer->Bind();
	}

	const ShaderCBufferElement* Material::FindCBufferElementDeclaration(const std::string& name)
	{
		const auto& shaderCBuffers = mShader->GetCBuffersBindings();

		if (shaderCBuffers.size() > 0)
		{
			const ShaderCBufferBindingDesc& buffer = shaderCBuffers.at("Material");

			if (buffer.CBufferElements.find(name) == buffer.CBufferElements.end())
				return nullptr;
			
			return &buffer.CBufferElements.at(name);
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
		if (!Exists(name))
		{
			auto material = CreateRef<Material>(name, shader);
			Add(name, material);
			return material;
		}
		else
			return Get(name);
	}

	Ref<Material> MaterialLibrary::Load()
	{
		auto material = CreateRef<Material>("New Material", ShaderLibrary::Get("Standard"));
		Add("New Material", material);
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
		out << YAML::Key << "Shader" << YAML::Value << material->GetShader()->GetName();

		out << YAML::Key << "Textures" << YAML::Value << YAML::BeginSeq;
		for (auto& texture : material->GetTextureBindings())
		{
			// First few slots are preserved for PBR textures
			if (texture.Texture && (texture.ShaderType != D3D11_PIXEL_SHADER || texture.BindSlot > 2)) 
			{
				out << YAML::BeginMap;
				out << YAML::Key << "FilePath" << YAML::Value << texture.Texture->GetFilePath();
				out << YAML::Key << "BindSlot" << YAML::Value << texture.BindSlot;
				out << YAML::Key << "ShaderType" << YAML::Value << (uint32_t)texture.ShaderType;
				out << YAML::EndMap;
			}
		}
		out << YAML::EndSeq;
		out << YAML::EndMap;

		std::ofstream fout(filepath);
		fout << out.c_str();
	}

	bool MaterialSerializer::Deserialize(std::vector<std::string> materials)
	{
		for (auto& materialPath : materials)
		{
			D3D11_SHADER_TYPE shaderType;

			std::ifstream stream(materialPath);
			std::stringstream strStream;

			strStream << stream.rdbuf();

			YAML::Node data = YAML::Load(strStream.str());
			if (!data["Material"])
				continue;

			std::string materialName = data["Material"].as<std::string>();
			std::string shaderName = data["Shader"].as<std::string>();

			TOAST_CORE_TRACE("Deserializing material '%s', Shader : '%s'", materialName.c_str(), shaderName.c_str());

			auto& material = MaterialLibrary::Load(materialName, ShaderLibrary::Get(shaderName));

			auto textures = data["Textures"];
			if (textures)
			{
				for (auto texture : textures)
					material->SetTexture(texture["BindSlot"].as<uint32_t>(), (D3D11_SHADER_TYPE)texture["ShaderType"].as<uint32_t>(), TextureLibrary::LoadTexture2D(texture["FilePath"].as<std::string>()));
			}
		}

		return true;
	}

}