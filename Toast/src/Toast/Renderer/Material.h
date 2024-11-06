#pragma once

#include <string>
#include <unordered_map>
#include <d3d11.h>

#include "Toast/Core/Base.h"

#include "Toast/Renderer/Shader.h"
#include "Toast/Renderer/Texture.h"

namespace Toast {

	struct PBRParameters 
	{
		DirectX::XMFLOAT3 Albedo;
		float Emission = 0.0;
		float Metalness = 0.0;
		float Roughness = 0.0;
		int AlbedoTexToggle = 0;
		int NormalTexToggle = 0;
		int MetalRoughTexToggle = 0;
	};

	class Material
	{
	public:
		Material();
		Material(const std::string& name);
		~Material() = default;

		std::string& GetName() { return mName; }
		void SetName(std::string& name) { mName = name; }

		void SetUseAlbedo(const uint32_t useAlbedo) { mPBRParameters.AlbedoTexToggle = useAlbedo; }
		bool GetUseAlbedo() const { return mPBRParameters.AlbedoTexToggle; }
		void SetAlbedoTexture(Texture2D* texture) { mAlbedoTexture = texture; }
		Texture2D* GetAlbedoTexture() const { return mAlbedoTexture; }

		void SetUseNormal(const uint32_t useNormal) { mPBRParameters.NormalTexToggle = useNormal; }
		bool GetUseNormal() const { return mPBRParameters.NormalTexToggle; }
		void SetNormalTexture(Texture2D* texture) { mNormalTexture = texture; }
		Texture2D* GetNormalTexture() const { return mNormalTexture; }

		void SetUseMetalRough(const uint32_t useMetalRough) { mPBRParameters.MetalRoughTexToggle = useMetalRough; }
		bool GetUseMetalRough() const { return mPBRParameters.MetalRoughTexToggle; }
		void SetMetalRoughTexture(Texture2D* texture) { mMetalRoughTexture = texture; }
		Texture2D* GetMetalRoughTexture() const { return mMetalRoughTexture; }

		void SetAlbedo(const DirectX::XMFLOAT3& albedo) { mPBRParameters.Albedo = albedo; }
		DirectX::XMFLOAT3& GetAlbedo() { return mPBRParameters.Albedo; }

		void SetEmission(const float emission) { mPBRParameters.Emission = emission; }
		float& GetEmission() { return mPBRParameters.Emission; }

		void SetMetalness(const float metalness) { mPBRParameters.Metalness = metalness; }
		float& GetMetalness() { return mPBRParameters.Metalness; }

		void SetRoughness(const float roughness) { mPBRParameters.Roughness = roughness; }
		float& GetRoughness() { return mPBRParameters.Roughness; }
	private:
		std::string mName = "No name";

		PBRParameters mPBRParameters;

		Texture2D* mAlbedoTexture;
		Texture2D* mNormalTexture;
		Texture2D* mMetalRoughTexture;
	};

	class MaterialLibrary
	{
	public:
		static void Add(const std::string name, const Ref<Material>& material);
		static void Add(const Ref<Material>& material);
		static Ref<Material> Load();
		static Ref<Material> Load(const std::string& name, bool serialize = false);

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