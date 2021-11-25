#pragma once

#include <string>
#include <unordered_map>
#include <d3d11.h>

#include "Toast/Core/Base.h"

#include "Toast/Renderer/Shader.h"
#include "Toast/Renderer/Texture.h"

namespace Toast {

	class Material
	{
	public:
		struct TextureBindInfo 
		{
			D3D11_SHADER_TYPE ShaderType	{ D3D11_VERTEX_SHADER };
			uint32_t BindSlot				{ 0 };
			Ref<Texture> Texture			{ nullptr };
		};

		struct SamplerBindInfo
		{
			D3D11_SHADER_TYPE ShaderType	{ D3D11_VERTEX_SHADER };
			uint32_t BindSlot				{ 0 };
			Ref<TextureSampler> Sampler		{ nullptr };
		};
	public:
		Material() = default;
		Material(const std::string& name, Shader* shader);
		~Material() = default;

		bool& GetBool(const std::string& name);
		int& GetInt(const std::string& name);
		float& GetFloat(const std::string& name);
		DirectX::XMFLOAT2& GetFloat2(const std::string& name);
		DirectX::XMFLOAT3& GetFloat3(const std::string& name);
		DirectX::XMFLOAT4& GetFloat4(const std::string& name);

		template <typename T>
		void Set(const std::string& name, const T& value) 
		{
			auto decl = FindCBufferElementDeclaration(name);
			TOAST_CORE_ASSERT(decl, "Couldn't find constant buffer element!");
			if (!decl)
				return;

			mMaterialBuffer.Write((byte*)&value, decl->GetSize(), decl->GetOffset());
		}

		template <typename T>
		T& Get(const std::string& name)
		{
			auto decl = FindCBufferElementDeclaration(name);
			TOAST_CORE_ASSERT(decl, "Couldn't find constant buffer element!");

			return mMaterialBuffer.Read<T>(decl->GetOffset());
		}

		Shader* GetShader() const { return mShader; }
		void SetShader(Shader* shader);

		std::string& GetName() { return mName; }
		void SetName(std::string& name) { mName = name; }

		std::vector<TextureBindInfo> GetTextureBindings() const { return mTextureBindings; }

		void SetTexture(uint32_t bindslot, D3D11_SHADER_TYPE shaderType, Ref<Texture2D>& texture);
		void SetTexture(uint32_t bindslot, D3D11_SHADER_TYPE shaderType, Ref<TextureCube>& texture);
		Ref<Texture> GetTexture(std::string name);
		void SetTextureSampler(uint32_t bindslot, D3D11_SHADER_TYPE shaderType, Ref<TextureSampler>& sampler);

		Ref<ConstantBuffer> GetMaterialCBuffer() const { return mMaterialCBuffer; }

		void SetUpResourceBindings();
		void Map();
		void Bind();

	private:
		const ShaderCBufferElement* FindCBufferElementDeclaration(const std::string& name);
	private:
		Shader* mShader;

		std::vector<TextureBindInfo> mTextureBindings;
		std::vector<SamplerBindInfo> mSamplerBindings;

		Ref<ConstantBuffer> mMaterialCBuffer;
		Buffer mMaterialBuffer;

		std::string mName = "No name";
	};

	class MaterialLibrary
	{
	public:
		static void Add(const std::string name, const Ref<Material>& material);
		static void Add(const Ref<Material>& material);
		static Ref<Material> Load();
		static Ref<Material> Load(const std::string& name, Shader* shader);

		static Ref<Material> Get(const std::string& name);
		static std::unordered_map<std::string, Ref<Material>> GetMaterials() { return mMaterials; }

		static bool Exists(const std::string& name);

		static void ChangeName(const std::string& oldName, std::string& newName);

		static void SerializeLibrary();
	private:
		static std::unordered_map<std::string, Ref<Material>> mMaterials;
	};

	class MaterialSerializer 
	{
	public:
		static void Serialize(Ref<Material>& material);
		static bool Deserialize(std::vector<std::string> materials);
	private:
	};
}