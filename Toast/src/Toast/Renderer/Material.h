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
		DirectX::XMFLOAT4 Albedo;
		float Emission = 0.0;
		float Metalness = 0.0;
		float Roughness = 0.0;
		uint32_t AlbedoTexToggle = 0;
		uint32_t NormalTexToggle = 0;
		uint32_t MetalRoughTexToggle = 0;
	};

	struct FromFile {};

	class Material : public Asset
	{
	public:
		Material();
		Material(const std::string& name);
		Material(const std::filesystem::path& tmtlPath, FromFile);
		Material(const std::string& name,
			const DirectX::XMFLOAT4& albedo, float emission, float metalness, float roughness,
			bool useAlbedo, bool useNormal, bool useMetalRough,
			AssetHandle albedoTex, AssetHandle normalTex, AssetHandle metalRoughTex);
		~Material() = default;

		std::string& GetName() { return mName; }
		void SetName(std::string& name) { mName = name; }

		AssetType GetAssetType() const override { return AssetType::Material; }

		void SaveToFile(const std::filesystem::path& tmtlPath) const;

		void SetUseAlbedo(const bool useAlbedo) { mPBRParameters.AlbedoTexToggle = useAlbedo ? 1 : 0; }
		bool GetUseAlbedo() const { return mPBRParameters.AlbedoTexToggle ? true : false; }
		void SetAlbedolAssetHandle(AssetHandle handle) { mAlbedoTextureHandle = handle; }
		AssetHandle GetAlbedoAssetHandle() const { return mAlbedoTextureHandle; }

		void SetUseNormal(const bool useNormal) { mPBRParameters.NormalTexToggle = useNormal ? 1 : 0;	}
		bool GetUseNormal() const { return mPBRParameters.NormalTexToggle ? true : false;	}
		void SetNormalAssetHandle(AssetHandle handle) { mNormalTextureHandle = handle; }
		AssetHandle GetNormalAssetHandle() const { return mNormalTextureHandle; }

		void SetUseMetalRough(const bool useMetalRough) { mPBRParameters.MetalRoughTexToggle = useMetalRough ? 1 : 0; }
		bool GetUseMetalRough() const { return mPBRParameters.MetalRoughTexToggle ? true : false; }
		void SetMetalRoughAssetHandle(AssetHandle handle) { mMetalRoughTextureHandle = handle; }
		AssetHandle GetMetalRoughAssetHandle() const { return mMetalRoughTextureHandle; }

		void SetAlbedo(const DirectX::XMFLOAT4 albedo) { mPBRParameters.Albedo = albedo; }
		DirectX::XMFLOAT4& GetAlbedo() { return mPBRParameters.Albedo; }

		void SetEmission(const float emission) { mPBRParameters.Emission = emission; }
		float& GetEmission() { return mPBRParameters.Emission; }

		void SetMetalness(const float metalness) { mPBRParameters.Metalness = metalness; }
		float& GetMetalness() { return mPBRParameters.Metalness; }

		void SetRoughness(const float roughness) { mPBRParameters.Roughness = roughness; }
		float& GetRoughness() { return mPBRParameters.Roughness; }
	private:
		std::string mName = "No name";

		PBRParameters mPBRParameters = {};

		AssetHandle mAlbedoTextureHandle;
		AssetHandle mNormalTextureHandle;
		AssetHandle mMetalRoughTextureHandle;
	};

}