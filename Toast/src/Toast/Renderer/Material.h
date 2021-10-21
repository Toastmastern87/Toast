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

		struct CBufferBindInfo
		{
			D3D11_SHADER_TYPE ShaderType	{ D3D11_VERTEX_SHADER };
			uint32_t BindSlot				{ 0 };
			Ref<ConstantBuffer> CBuffer		{ nullptr };
		};
	public:
		Material() = default;
		Material(const std::string& name, const Ref<Shader>& shader);
		~Material() = default;

		void SetData(const std::string& cbName, void* data);

		const Ref<Shader> GetShader() const { return mShader; }
		void SetShader(const Ref<Shader>& shader);

		std::string& GetName() { return mName; }
		void SetName(std::string& name) { mName = name; }

		std::vector<TextureBindInfo> GetTextureBindings() const { return mTextureBindings; }

		void SetTexture(uint32_t bindslot, D3D11_SHADER_TYPE shaderType, Ref<Texture2D>& texture);
		void SetTexture(uint32_t bindslot, D3D11_SHADER_TYPE shaderType, Ref<TextureCube>& texture);
		Ref<Texture> GetTexture(std::string name);
		void SetTextureSampler(uint32_t bindslot, D3D11_SHADER_TYPE shaderType, Ref<TextureSampler>& sampler);

		void SetUpResourceBindings();
		void Bind();
	private:
		Ref<Shader> mShader;

		// New way
		std::vector<TextureBindInfo> mTextureBindings;
		std::vector<SamplerBindInfo> mSamplerBindings;
		std::vector<CBufferBindInfo> mCBufferBindings;

		std::string mName = "No name";
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