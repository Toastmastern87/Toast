#pragma once

#include <future>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include <../vendor/directxtex/include/DirectXTex.h>
#include "../vendor/perlin-noise/include/PerlinNoise.hpp"

#include "Toast/Core/Timestep.h"
#include "Toast/Core/Math/Math.h"

#include "Toast/Renderer/Frustum.h"
#include "Toast/Renderer/Mesh.h"
#include "Toast/Renderer/RenderCommand.h"

#include "Toast/Scene/Components.h"

#include "renderdoc_app.h"

#include <thread>
#include <mutex>
#include <future>

#define MAX_INT_VALUE	65535.0
#define M_PI			3.14159265358979323846
#define M_PIDIV2		(3.14159265358979323846 / 2.0)

namespace Toast {

	class PlanetSystem
	{
	public:
		static std::mutex planetDataMutex;
		static std::future<void> generationFuture;
		static std::atomic<bool> newPlanetReady;
		static std::atomic<bool> planetGenerationOngoing;

		static const int16_t MAX_SUBDIVISION = 20;

		enum class NextPlanetFace
		{
			CULL, LEAF, SPLIT, SPLITCULL
		};

		struct HeightRange {
			double minHeight;
			double maxHeight;
		};
	public:
		static void UpdatePlanet(Ref<Mesh>& renderPlanet, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);

		static void RegeneratePlanet(Ref<Frustum>& frustum, DirectX::XMFLOAT3& scale, DirectX::XMMATRIX noScaleTransform, DirectX::XMVECTOR camPos, bool backfaceCull, bool frustumCullActivated, PlanetComponent& planet, TerrainDetailComponent* terrainDetail = nullptr);

		static void Shutdown();

		static void GenerateDistanceLUT(std::vector<double>& distanceLUT, float radius, float FoV, float viewportSizeX);
		static void GenerateFaceDotLevelLUT(std::vector<double>& faceLevelDotLUT, float planetRadius, float maxHeight);
		static void GenerateHeightMultLUT(std::vector<double>& heightMultLUT, double planetRadius, double maxHeight);
	private:
		static Vector2 GetUVFromPosition(Vector3& pos, double width, double height);
		static DirectX::XMFLOAT2 GetGPUUVFromPosition(Vector3& pos);
		static void GetBasePlanet(std::vector<Vector3>& vertices, std::vector<uint32_t>& indices, DirectX::XMFLOAT3& scale);

		static void GeneratePlanet(Ref<Frustum>& frustum, DirectX::XMFLOAT3& scale, DirectX::XMMATRIX noScaleTransform, DirectX::XMVECTOR camPos, bool backfaceCull, bool frustumCullActivated, PlanetComponent& planet, TerrainDetailComponent* terrainDetail = nullptr);

		static void SubdivideFace(Vector3& cameraPosPlanetSpace, Matrix& planetScaleTransform, Ref<Frustum>& frustum, int& triangleAdded, Matrix planetTransform, Vector3& a, Vector3& b, Vector3& c, int16_t& subdivision, bool backfaceCull, bool frustumCullActivated, bool frustumCull, PlanetComponent& planet);

		static NextPlanetFace CheckFaceSplit(Vector3& cameraPosPlanetSpace, Matrix& planetScaleTransform, Ref<Frustum>& frustum, int16_t subdivision, Vector3 a, Vector3 b, Vector3 c, bool backfaceCull, bool frustumCullActivated, bool frustumCull, PlanetComponent& planet);

		static uint32_t GetOrAddVertex(std::unordered_map<Vertex, uint32_t, Vertex::Hasher, Vertex::Equal>& vertexMap, const Vertex& vertex, std::vector<Vertex>& vertices);
	};

}