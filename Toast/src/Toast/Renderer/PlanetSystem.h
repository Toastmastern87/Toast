﻿#pragma once

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

	struct CPUVertex
	{
		Vector3 Position;
		Vector3 Normal;
		Vector2 UV;

		CPUVertex() = default;
		CPUVertex(const Vector3& position, const Vector3& normal, const Vector2& uv)
			: Position(position), Normal(normal), UV(uv) {}
		CPUVertex(Vector3 pos)
			: Position(pos) {}

		// Copy constructor
		CPUVertex(const CPUVertex& other)
			: Position(other.Position), Normal(other.Normal), UV(other.UV) {}

		// Optional: Assignment operator
		CPUVertex& operator=(const CPUVertex& other)
		{
			if (this != &other) // Check for self-assignment
			{
				Position = other.Position;
				Normal = other.Normal;
				UV = other.UV;
			}
			return *this;
		}
	};

	struct PlanetNode
	{
		CPUVertex A, B, C;  // The three vertices of the triangle
		std::vector<Ref<PlanetNode>> ChildNodes;
		int16_t SubdivisionLevel = 0;
		Bounds NodeBounds;

		PlanetNode(const CPUVertex& v0, const CPUVertex& v1, const CPUVertex& v2, const int16_t level, Matrix transform = Matrix::Identity())
		{
			A = v0;
			B = v1;
			C = v2;

			A.Position = transform * A.Position;
			B.Position = transform * B.Position;
			C.Position = transform * C.Position;

			SubdivisionLevel = level;

			ComputeBoundsFromTriangle();
		}

		PlanetNode(const PlanetNode& other)
		{
			A = other.A;
			B = other.B;
			C = other.C;
			SubdivisionLevel = other.SubdivisionLevel;
			NodeBounds = other.NodeBounds;

			// Shallow copy of the child nodes
			ChildNodes = other.ChildNodes;
		}

		void ComputeBoundsFromTriangle()
		{
			NodeBounds.mins = {
				(std::min)({A.Position.x, B.Position.x, C.Position.x}),
				(std::min)({A.Position.y, B.Position.y, C.Position.y}),
				(std::min)({A.Position.z, B.Position.z, C.Position.z})
			};
			NodeBounds.maxs = {
				(std::max)({A.Position.x, B.Position.x, C.Position.x}),
				(std::max)({A.Position.y, B.Position.y, C.Position.y}),
				(std::max)({A.Position.z, B.Position.z, C.Position.z})
			};
		}

		void UpdateBoundsFromChildren() {
			// If no children, bounds are already computed from the triangle
			if (ChildNodes.empty()) return;

			// Start with a large inverted bounding box
			Bounds childBounds;
			childBounds.mins = { DBL_MAX, DBL_MAX, DBL_MAX };
			childBounds.maxs = { -DBL_MAX, -DBL_MAX, -DBL_MAX };

			for (auto& child : ChildNodes)
			{
				childBounds.mins.x = (std::min)(childBounds.mins.x, child->NodeBounds.mins.x);
				childBounds.mins.y = (std::min)(childBounds.mins.y, child->NodeBounds.mins.y);
				childBounds.mins.z = (std::min)(childBounds.mins.z, child->NodeBounds.mins.z);

				childBounds.maxs.x = (std::max)(childBounds.maxs.x, child->NodeBounds.maxs.x);
				childBounds.maxs.y = (std::max)(childBounds.maxs.y, child->NodeBounds.maxs.y);
				childBounds.maxs.z = (std::max)(childBounds.maxs.z, child->NodeBounds.maxs.z);
			}

			NodeBounds = childBounds;
		}
	};

	class PlanetSystem
	{
	public:
		static std::mutex planetDataMutex;
		static std::mutex terrainCollidersMutex;
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
	private:
		static std::vector<Ref<PlanetNode>> sPlanetNodes;

		static std::vector<Vector3> sBaseVertices;
		static std::vector<uint32_t> sBaseIndices;
		static std::unordered_map<Vector3, uint32_t, Vector3::Hasher, Vector3::Equal> sBaseVertexMap;
	public:
		static uint32_t HashFace(uint32_t index0, uint32_t index1, uint32_t index2);

		static void SubdivideBasePlanet(PlanetComponent& planet, Ref<PlanetNode>& node, double scale);
		static void SubdivideFace(Ref<PlanetNode>& node, CPUVertex& A, CPUVertex& B, CPUVertex& C, Vector3& cameraPosPlanetSpace, PlanetComponent& planet, const Vector3& planetCenter, Matrix& planetTransform, uint16_t subdivision, const siv::PerlinNoise& perlin, TerrainDetailComponent* terrainDetail);
		static void CalculateBasePlanet(PlanetComponent& planet, double scale);

		static void DetailObjectPlacement(const PlanetComponent& planet, TerrainObjectComponent& objects, DirectX::XMMATRIX noScaleTransform, DirectX::XMVECTOR& camPos);

		static void UpdatePlanet(Ref<Mesh>& renderPlanet, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, TerrainColliderComponent& terrainCollider);

		static double GetHeight(Vector2 uvCoords, TerrainData& terrainData);

		static void RegeneratePlanet(Ref<Frustum>& frustum, DirectX::XMFLOAT3& scale, const Vector3& planetCenter, DirectX::XMMATRIX noScaleTransform, DirectX::XMVECTOR camPos, bool backfaceCull, bool frustumCullActivated, PlanetComponent& planet, std::unordered_map<std::pair<int, int>, Ref<ShapeBox>, PairHash>& terrainColliders, std::unordered_map<std::pair<int, int>, std::vector<Vector3>, PairHash>& terrainColliderPositions, TerrainDetailComponent* terrainDetail = nullptr);

		static void Shutdown();

		static void GenerateDistanceLUT(std::vector<double>& distanceLUT, float radius, float FoV, float viewportSizeX);
		static void GenerateFaceDotLevelLUT(std::vector<double>& faceLevelDotLUT, float planetRadius, float maxHeight);
		static void GenerateHeightMultLUT(std::vector<double>& heightMultLUT, double planetRadius, double maxHeight);
	private:
		static Vector2 GetUVFromPosition(const Vector3 pos, double width, double height);

		static void GetFaceBounds(const std::initializer_list<Vector3>& vertices, Bounds& bounds);

		static void GeneratePlanet(Ref<Frustum>& frustum, DirectX::XMFLOAT3& scale, const Vector3& planetCenter, DirectX::XMMATRIX noScaleTransform, DirectX::XMVECTOR camPos, bool backfaceCull, bool frustumCullActivated, PlanetComponent& planet, std::unordered_map<std::pair<int, int>, Ref<ShapeBox>, PairHash>& terrainColliders, std::unordered_map<std::pair<int, int>, std::vector<Vector3>, PairHash>& terrainColliderPositions, TerrainDetailComponent* terrainDetail = nullptr);

		static void TraverseNode(Ref<PlanetNode>& node, PlanetComponent& planet, Vector3& cameraPosPlanetSpace, const Vector3& planetCenter, bool backfaceCull, bool frustumCullActivated, Ref<Frustum>& frustum, Matrix& planetTransform, const siv::PerlinNoise& perlin, TerrainDetailComponent* terrainDetail);

		static uint32_t GetOrAddVector3(std::unordered_map<Vector3, uint32_t, Vector3::Hasher, Vector3::Equal>& vertexMap, const Vector3& vertex, std::vector<Vector3>& vertices);

		static void AssignFaceToChunk(const Vector3& vecA, const Vector3& vecB, const Vector3& vecC, 
			std::unordered_map<std::pair<int, int>, std::vector<Vector3>, PairHash>& chunks,
			const Vector3& planetCenter);
		static void GetVerticesBounds(const std::vector<Vector3>& vertices, Bounds& bounds);
	};

}