#include "tpch.h"
#include "Toast/Renderer/Material.h"
#include "Toast/Renderer/RendererAPI.h"
#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/Shader.h"

#include "Toast/Core/Application.h"

#include <yaml-cpp/yaml.h>

namespace Toast {

	Material::Material(const std::string& name, const Ref<Shader>& shader)
		: mName(name), mShader(shader)
	{
		SetUpResourceBindings();
	}

	void Material::SetData(const std::string& name, void* data)
	{
		uint32_t cbufferIdx;

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();	
		 
		// Check to see if the constant buffer exists
		for (int i = 0; i < mCBufferBindings.size(); i++)
		{
			cbufferIdx = i;

			if (name.compare(mCBufferBindings[i].CBuffer->GetName()) == 0)
				break;

			if((cbufferIdx+1) == mCBufferBindings.size())
			{
				TOAST_CORE_INFO("Trying to write data to a non existent constant buffer: {0}", name.c_str());
				return;
			}
		}

		D3D11_MAPPED_SUBRESOURCE ms;
		deviceContext->Map(mCBufferBindings[cbufferIdx].CBuffer->GetBuffer(), NULL, D3D11_MAP_WRITE_DISCARD, NULL, &ms);
		memcpy(ms.pData, data, mCBufferBindings[cbufferIdx].CBuffer->GetSize());
		deviceContext->Unmap(mCBufferBindings[cbufferIdx].CBuffer->GetBuffer(), NULL);
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

		mDirty = true;
	}

	void Material::SetTexture(uint32_t bindslot, D3D11_SHADER_TYPE shaderType, Ref<TextureCube>& texture)
	{
		for (auto& texturebinding : mTextureBindings)
		{
			if (texturebinding.BindSlot == bindslot && texturebinding.ShaderType == shaderType)
				texturebinding.Texture = texture;
		}

		mDirty = true;
	}

	void Material::SetTextureSampler(uint32_t bindslot, D3D11_SHADER_TYPE shaderType, Ref<TextureSampler>& sampler)
	{
		for (auto& textureSampler : mSamplerBindings)
		{
			if (textureSampler.BindSlot == bindslot && textureSampler.ShaderType == shaderType)
				textureSampler.Sampler = sampler;
		}

		mDirty = true;
	}

	void Material::SetUpResourceBindings()
	{
		std::vector<Shader::ResourceBindingDesc> resourceBindings = mShader->GetResourceBindings();

		mTextureBindings.clear();
		mSamplerBindings.clear();
		mCBufferBindings.clear();

		for (auto& resource : resourceBindings)
		{
			switch (resource.Type)
			{
			case Shader::BindingType::Texture:
			{
				mTextureBindings.push_back(TextureBindInfo{ resource.Shader, resource.BindSlot, nullptr });
				break;
			}
			case Shader::BindingType::Sampler:
			{
				mSamplerBindings.push_back(SamplerBindInfo{ resource.Shader, resource.BindSlot, TextureLibrary::GetSampler("Default") });
				break;
			}

			case Shader::BindingType::Buffer:
			{
				mCBufferBindings.push_back(CBufferBindInfo{ resource.Shader, resource.BindSlot, BufferLibrary::Load(resource.Name, resource.Size, resource.Shader, resource.BindSlot) });
				break;
			}
			}
		}
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

		for (auto& cBuffer : mCBufferBindings)
		{
			if (cBuffer.CBuffer)
				cBuffer.CBuffer->Bind();
		}
	}

	std::unordered_map<std::string, Ref<Material>> MaterialLibrary::mMaterials;

	void MaterialLibrary::Add(std::string name, const Ref<Material>& material)
	{
		TOAST_CORE_TRACE("Adding material: {0}", name);
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

	Ref<Material> MaterialLibrary::Load()
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

		out << YAML::Key << "Textures" << YAML::Value << YAML::BeginSeq;
		for (auto& texture : material->GetTextureBindings())
		{
			// First few slots are preserved for PBR textures
			if (texture.ShaderType != D3D11_PIXEL_SHADER || texture.BindSlot > 2) 
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

		material->SetDirty(false);
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

			TOAST_CORE_TRACE("Deserializing material '{0}', Shader : '{1}'", materialName, shaderName);

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