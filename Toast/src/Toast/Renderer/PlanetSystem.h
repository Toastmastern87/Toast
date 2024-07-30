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

using namespace DirectX;

namespace Toast {

	class PlanetSystem
	{
	public:
		static std::mutex planetDataMutex;
		static std::future<void> generationFuture;
		static std::atomic<bool> newPlanetReady;

		static const int16_t MAX_SUBDIVISION = 20;

		using Edge = std::pair<uint32_t, uint32_t>;

		struct EdgeHash {
			std::size_t operator()(const Edge& edge) const {
				// Hash combining based on Boost's hash_combine
				std::size_t hash = std::hash<uint32_t>()(edge.first);
				hash ^= std::hash<uint32_t>()(edge.second) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
				return hash;
			}
		};

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

		static void RegeneratePlanet(std::unordered_map<Vertex, uint32_t, Vertex::Hasher, Vertex::Equal>& vertexMap, Ref<Frustum>& frustum, DirectX::XMFLOAT3& scale, DirectX::XMMATRIX noScaleTransform, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, std::vector<double>& distanceLUT, std::vector<double>& faceLevelDotLUT, std::vector<double>& heightMultLUT, DirectX::XMVECTOR camPos, int16_t subdivisions, float radius, bool backfaceCull, bool frustumCullActivated, TerrainData& terrainDataUpdated, double minAltitude, double maxAltitude, uint32_t octaves, float frequencey, float amplitude, uint32_t* seed = nullptr);

		static void Shutdown();

		static void GenerateDistanceLUT(std::vector<double>& distanceLUT, float radius, float FoV, float viewportSizeX);
		static void GenerateFaceDotLevelLUT(std::vector<double>& faceLevelDotLUT, float planetRadius, float maxHeight);
		static void GenerateHeightMultLUT(std::vector<double>& heightMultLUT, double planetRadius, double maxHeight);
	private:
		static Vector2 GetUVFromPosition(Vector3& pos, double width, double height);
		static DirectX::XMFLOAT2 GetGPUUVFromPosition(Vector3& pos);
		static void GetBasePlanet(std::vector<Vector3>& vertices, std::vector<uint32_t>& indices, DirectX::XMFLOAT3& scale);

		static void GeneratePlanet(std::unordered_map<Vertex, uint32_t, Vertex::Hasher, Vertex::Equal>& vertexMap, Ref<Frustum>& frustum, DirectX::XMFLOAT3& scale, DirectX::XMMATRIX noScaleTransform, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, std::vector<double>& distanceLUT, std::vector<double>& faceLevelDotLUT, std::vector<double>& heightMultLUT, DirectX::XMVECTOR camPos, int16_t subdivisions, float radius, bool backfaceCull, bool frustumCullActivated, TerrainData& terrainData, double minAltitude, double maxAltitude, uint32_t octaves, float frequencey, float amplitude, uint32_t* seed = nullptr);

		static void SubdivideFace(TerrainData& terrainDataUpdated, double radius, Vector3& cameraPosPlanetSpace, Matrix& planetScaleTransform, Ref<Frustum>& frustum, int& triangleAdded, std::unordered_map<Vertex, uint32_t, Vertex::Hasher, Vertex::Equal>& vertexMap, Matrix planetTransform, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, Vector3& a, Vector3& b, Vector3& c, int16_t& subdivision, int16_t maxSubdivisions, std::vector<double>& distanceLUT, std::vector<double>& faceLevelDotLUT, std::vector<double>& heightMultLUT, bool backfaceCull, bool frustumCullActivated, bool frustumCull);

		static NextPlanetFace CheckFaceSplit(TerrainData& terrainDataUpdated, double radius, Vector3& cameraPosPlanetSpace, Matrix& planetScaleTransform, Ref<Frustum>& frustum, int16_t subdivision, int16_t maxSubdivisions, Vector3 a, Vector3 b, Vector3 c, std::vector<double>& distanceLUT, std::vector<double>& faceLevelDotLUT, std::vector<double>& heightMultLUT, bool backfaceCull, bool frustumCullActivated, bool frustumCull);

		static uint32_t GetOrAddVertex(std::unordered_map<Vertex, uint32_t, Vertex::Hasher, Vertex::Equal>& vertexMap, const Vertex& vertex, std::vector<Vertex>& vertices);
	};

}