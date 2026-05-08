#pragma once

#include <stdint.h>
#include <string>
#include <vector>

#include "Toast/Assets/Asset.h"

namespace Toast {

	enum class NoiseType : int32_t
	{
		Fractal = 0,
		Ridged = 1,
		Turbulence = 2,
		Voronoi = 3
	};

	struct NoiseLayer
	{
		NoiseLayer() = default;

		struct GPUData
		{
			// 16 bytes
			int32_t Type = (int32_t)NoiseType::Fractal;
			int32_t LODActivation = 0;
			int32_t Octaves = 1;
			int32_t PermBase = 0; // Injected at upload time, index into perm table buffer

			// 16 bytes
			float Frequency = 1.0f;
			float Amplitude = 1.0f;
			float Lacunarity = 2.0f;
			float Persistence = 0.5f;

			// 16 bytes
			float BlendWeight = 1.0f;
			float RadialFreqScale = 1.0f;   // 0 = no stretch, higher = more stretched
			float RidgeSharpness = 2.0f;
			float pad0;
		};

		std::string Name = "New Noise Layer";
		uint32_t Seed = 0;
		int Perm[256] = {};

		GPUData GPU;
	};

	struct PBRMaterialLayer 
	{
		PBRMaterialLayer() = default;

		AssetHandle AlbedoHandle = 0;
		AssetHandle NormalHandle = 0;
		AssetHandle RoughnessHandle = 0;
		AssetHandle AOHandle = 0;
		AssetHandle DisplacementHandle = 0;

		float TilingScale = 1.0f;
		float DisplacementStrength = 0.5f; // world units (meters)
	};

	struct PlanetMaterial
	{
		PlanetMaterial() = default;

		struct GPUData
		{ 
			// 16 bytes - Slope selection
			float SlopeMin = 0.0f;
			float SlopeMax = 1.0f;
			float BlendSharpness = 8.0f;
			int32_t NoiseLayerStart = 0; // injected at upload

			// 16 bytes - Noise count + PBR Settings
			int32_t NoiseLayerCount = 0; // injected at upload
			float UVTilingScale = 1.0f;
			float ColorAvgMin = 0.0f;
			float ColorAvgMax = 1.0f;

			// 16 bytes - Albedo color average selection: (r + g + b) / 3
			float UseAlbedo = 1.0f;  // 0 = slope-only, 1 = use color avg range
			DirectX::XMFLOAT3 DebugColor;
		};

		std::string Name = "New Planet Material";
		std::vector<NoiseLayer> NoiseLayers;
		PBRMaterialLayer PBR;

		GPUData GPU;

		NoiseLayer& AddNoiseLayer(const std::string& name, NoiseType type)
		{
			NoiseLayer layer;
			layer.Name = name;
			layer.GPU.Type = (int32_t)type;
			layer.Seed = (uint32_t)(std::hash<std::string>{}(Name + name + std::to_string(NoiseLayers.size())));
			NoiseLayers.emplace_back(std::move(layer));
			return NoiseLayers.back();
		}

		void RemoveNoiseLayer(uint32_t index)
		{
			if (index < NoiseLayers.size())
				NoiseLayers.erase(NoiseLayers.begin() + index);
		}
	};

}