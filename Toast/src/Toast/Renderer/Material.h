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
		Material() = default;
		Material(const std::string& name, const Ref<Shader>& shader);
		~Material() = default;

		void SetData(const std::string& cbName, void* data);

		const Ref<Shader> GetShader() const { return mShader; }
		void SetShader(const Ref<Shader>& shader);

		std::string& GetName() { return mName; }
		void SetName(std::string& name) { mName = name; mDirty = true; }

		Ref<Texture2D> GetTexture(std::string name) { return mTextures[name]; }
		void SetTexture(std::string name, Ref<Texture2D>& texture);
		std::unordered_map<std::string, Ref<Texture2D>> GetTextures() const { return mTextures; }

		Ref<ConstantBuffer> GetCBuffer(std::string name) { return mConstantBuffers[name]; }

		void SetUpTextureBindings();
		void SetUpCBufferBindings();
		void Bind();

		void SetDirty(bool dirty) { mDirty = dirty; }
		const bool IsDirty() const { return mDirty; }
	private:
		Ref<Shader> mShader;

		std::unordered_map<std::string, Ref<Texture2D>> mTextures;
		std::unordered_map<std::string, Ref<ConstantBuffer>> mConstantBuffers;

		std::string mName = "No name";
		
		bool mDirty = false;
	};

	class MaterialLibrary
	{
	public:
		static void Add(const std::string name, const Ref<Material>& material);
		static void Add(const Ref<Material>& material);
		static Ref<Material> Load();
		static Ref<Material> Load(const std::string& name, const Ref<Shader>& shader);

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