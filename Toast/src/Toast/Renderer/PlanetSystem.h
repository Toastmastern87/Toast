#pragma once

#include <future>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include <../vendor/directxtex/include/DirectXTex.h>
#include "../vendor/perlin-noise/include/PerlinNoise.hpp"
#include "../vendor/robinhood/include/robin_hood.h"

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

	struct ClipLevel
	{
		std::pair<int32_t, int32_t> Origin = { 0, 0 };
		bool Dirty = true;
		bool InFrustum = true;
	};

	struct LODDrawInfo
	{
		uint32_t first;   // finest level that is *kept*  (≥ 0)
		uint32_t count;   // how many consecutive levels are drawn
	};

	struct PlanetLevelCB
	{
		int32_t OriginX;
		int32_t OriginY;
		uint32_t CellSize;
		uint32_t GridSize;
	};

	struct PlanetFrameCB
	{
		DirectX::XMFLOAT3 Center;
		float Radius;
		DirectX::XMFLOAT3 BasisTanEast;
		float MaxHeight;
		DirectX::XMFLOAT3 BasisTanNorth;
		float MinHeight;
		DirectX::XMFLOAT3 BasisRadUp;
		float Pad2;
		DirectX::XMFLOAT3 BasisLonEast;
		float Pad4;
		DirectX::XMFLOAT3 BasisLonNorth;
		float Pad3;
		DirectX::XMFLOAT3 BasisSpinUp;
		float Pad5;
	};

	constexpr double kQuant = 0.1;     // 1 cm grid
	constexpr double kInvQ = 1.0 / kQuant;

	struct TerrainData
	{
		uint32_t Width;
		uint32_t Height;
		uint32_t RowPitch;
		uint32_t Stride;
		std::vector<double> HeightData;
	};

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

	struct CPUVertexHasher
	{
		std::size_t operator()(const CPUVertex& v) const noexcept
		{
			// round to grid and pack into 3 * 21‑bit signed ints  (fits in 64‑bit)
			auto q = [](double x) -> int64_t
				{
					return (int64_t)std::llround(x * kInvQ);   // quantised integer
				};

			uint64_t kx = (uint64_t)(q(v.Position.x) & 0x1FFFFF);     // 21 bits
			uint64_t ky = (uint64_t)(q(v.Position.y) & 0x1FFFFF);
			uint64_t kz = (uint64_t)(q(v.Position.z) & 0x1FFFFF);

			return  kx | (ky << 21) | (kz << 42);          // 63 bits, no clash
		}
	};

	struct CPUVertexEqual
	{
		bool operator()(const CPUVertex& a, const CPUVertex& b) const noexcept
		{
			return std::abs(a.Position.x - b.Position.x) < kQuant &&
				std::abs(a.Position.y - b.Position.y) < kQuant &&
				std::abs(a.Position.z - b.Position.z) < kQuant;
		}
	};

	struct PosKey {
		DirectX::XMFLOAT3 p;
		bool operator==(const PosKey& o) const noexcept
		{
			constexpr float eps = 1e-6f;
			return std::abs(p.x - o.p.x) < eps &&
				std::abs(p.y - o.p.y) < eps &&
				std::abs(p.z - o.p.z) < eps;
		}
	};
	struct PosHash {
		size_t operator()(const PosKey& k) const noexcept
		{
			auto q = [](float f) { return uint32_t(std::llround(f * 1e6)); };
			return (q(k.p.x) * 73856093u) ^ (q(k.p.y) * 19349663u) ^ (q(k.p.z) * 83492791u);
		}
	};

	struct EdgeKey {
		uint32_t v0, v1;                  // vertex indices in *base* icosahedron order
		bool operator==(const EdgeKey& o) const { return v0 == o.v0 && v1 == o.v1; }
	};
	struct EdgeKeyHash {
		size_t operator()(const EdgeKey& k) const { return (size_t)k.v0 * 73856093u ^ k.v1; }
	};

	struct PlanetNode
	{
		CPUVertex A, B, C;  // The three vertices of the triangle

		Vector3 Center;
		PlanetNode* Parent = nullptr;
		std::vector<Ref<PlanetNode>> ChildNodes;
		int32_t SubdivisionLevel = 0;
		Bounds NodeBounds;

		PlanetNode* Neighbours[3]{ nullptr, nullptr, nullptr };

		std::vector<DirectX::XMFLOAT3> CachedDetailObjectPosition;

		double SphereRadius = 0.0;

		enum class State : uint8_t
		{
			ActiveLeaf,      // rendered this frame
			WantSplit,
			WantCollapse,
			Culled
		} 
		NodeState = State::ActiveLeaf;

		PlanetNode(const CPUVertex& v0, const CPUVertex& v1, const CPUVertex& v2, const int32_t level, Matrix transform = Matrix::Identity())
		{
			A = v0;
			B = v1;
			C = v2;

			A.Position = transform * A.Position;
			B.Position = transform * B.Position;
			C.Position = transform * C.Position;

			Center = (A.Position + B.Position + C.Position) / 3.0f;

			SubdivisionLevel = level;

			ComputeBoundsFromTriangle();
			ComputeSphereRadius();
		}

		PlanetNode(const PlanetNode& other)
		{
			A = other.A;
			B = other.B;
			C = other.C;

			Center = (A.Position + B.Position + C.Position) / 3.0f;

			SubdivisionLevel = other.SubdivisionLevel;
			NodeBounds = other.NodeBounds;

			// Shallow copy of the child nodes
			ChildNodes = other.ChildNodes;
		}

		inline void ComputeBoundsFromTriangle()
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

		inline void ComputeSphereRadius()
		{
			double r0 = (A.Position - Center).Length();
			double r1 = (B.Position - Center).Length();
			double r2 = (C.Position - Center).Length();
			SphereRadius = (std::max)({ r0, r1, r2 });
		}

		inline void UpdateBoundsFromChildren() {
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

		friend bool operator==(const PlanetNode& lhs, const PlanetNode& rhs) noexcept
		{
			if (lhs.SubdivisionLevel != rhs.SubdivisionLevel) return false;

			// Gather the three vertex positions from each triangle
			std::array<Vector3, 3> L{ lhs.A.Position, lhs.B.Position, lhs.C.Position };
			std::array<Vector3, 3> R{ rhs.A.Position, rhs.B.Position, rhs.C.Position };

			// Sort them into a canonical order so winding does not matter
			auto key = [](const Vector3& p)
				{
					return std::tuple<double, double, double>(p.x, p.y, p.z);
				};
			std::sort(L.begin(), L.end(),
				[&](const Vector3& a, const Vector3& b) { return key(a) < key(b); });
			std::sort(R.begin(), R.end(),
				[&](const Vector3& a, const Vector3& b) { return key(a) < key(b); });

			constexpr double eps = 1e-6;
			auto eq = [&](const Vector3& a, const Vector3& b)
				{
					return std::fabs(a.x - b.x) < eps &&
						std::fabs(a.y - b.y) < eps &&
						std::fabs(a.z - b.z) < eps;
				};
			return eq(L[0], R[0]) && eq(L[1], R[1]) && eq(L[2], R[2]);
		}

		struct Hasher
		{
			size_t operator()(const PlanetNode& n) const noexcept
			{
				// same canonical sort as in operator==
				std::array<Vector3, 3> v{ n.A.Position, n.B.Position, n.C.Position };
				auto key = [](const Vector3& p)
					{
						return std::tuple<double, double, double>(p.x, p.y, p.z);
					};
				std::sort(v.begin(), v.end(),
					[&](const Vector3& a, const Vector3& b) { return key(a) < key(b); });

				// Simple FNV‑1a combine
				auto h = [](double d)
					{
						return std::hash<int64_t>{}(static_cast<int64_t>(std::llround(d * 1e6)));
					};
				size_t seed = 14695981039346656037ULL;      // FNV offset
				auto mix = [&](size_t val) { seed ^= val; seed *= 1099511628211ULL; };
				mix(h(v[0].x)); mix(h(v[0].y)); mix(h(v[0].z));
				mix(h(v[1].x)); mix(h(v[1].y)); mix(h(v[1].z));
				mix(h(v[2].x)); mix(h(v[2].y)); mix(h(v[2].z));
				mix(std::hash<int16_t>{}(n.SubdivisionLevel));
				return seed;
			}
		};
	};

	class PlanetSystem
	{
	public:

		enum class NextPlanetFace
		{
			CULL, LEAF, SPLIT, SPLITCULL
		};

		struct HeightRange {
			double minHeight;
			double maxHeight;
		};

	// NEW PLANET SYSTEM
	private:
		// Base Data
		static inline bool sValidPlanet = false;
		static inline uint32_t sGridSize;
		static inline uint32_t sTempGridSize;
		static inline int32_t sNumLevels;
		static inline int32_t sTempNumLevels;
		static inline std::vector<ClipLevel> sLevels;
		static inline LODDrawInfo sActiveLevels;
		static inline bool sRunOnce = false;

		static inline DirectX::XMFLOAT3 sTranslation = { 0.0f, 0.0f, 0.0f };
		static inline DirectX::XMFLOAT3 sRotationEulerAngles = { 0.0f, 0.0f, 0.0f };
		static inline DirectX::XMFLOAT4 sRotationQuaternion = { 0.0f, 0.0f, 0.0f, 1.0f };
		static inline Quaternion mRotationQuat;
		static inline Quaternion mInvRotationQuat;

		// GPU Data
		static inline Ref<VertexBuffer> sGridVertexBuffer;
		static inline Ref<VertexBuffer> sLODGridVertexBuffer;
		static inline Ref<IndexBuffer> sCenterGridIndexBuffer;
		static inline Ref<IndexBuffer> sRingGridIndexBuffer;
		static inline Ref<IndexBuffer> sLODGridIndexBuffer;
		static inline uint32_t sGridIndexCount = 0;
		static inline uint32_t sRingGridIndexCount = 0;
		static inline uint32_t sLODGridIndexCount = 0;

		static inline Ref<ConstantBuffer> sPlanetFrameCBuffer, sPlanetLevelCBuffer;
		static inline Buffer sPlanetFrameBuffer, sPlanetLevelBuffer;
		static inline ShaderLayout sShaderInputLayout;

		// Terrain Data
		static inline double sRadius = 0.0;
		static inline double sMaxHeight = 0.0;
		static inline double sMinHeight = 0.0;
		static inline DirectX::XMFLOAT3 sBasisLonEast;
		static inline DirectX::XMFLOAT3 sBasisLonNorth;
		static inline DirectX::XMFLOAT3 sBasisSpinUp;
		static inline std::vector<double> sDistanceLUT;
		static inline Texture2D* sBaseHeightMapTexture;
		static inline TerrainData sTerrainData;

		// PBR Data
		static inline DirectX::XMFLOAT3 sAlbedoColor;
		static inline float sRoughness;
		static inline float sMetalness;
		static inline Ref<ConstantBuffer> sPlanetMaterialCBuffer;
		static inline Buffer sPlanetMaterialBuffer;

		// Atmosphere Data
		static inline bool sAtmosphereActivated = false;

		// Environment Textures
		static inline Texture2D* sStarFieldTexture2D;
		static inline Ref<TextureCube> sStarFieldTextureCube;
		static inline Ref<TextureCube> sRadianceMap;
		static inline Ref<TextureCube> sIrradianceMap;
		static inline Texture2D* sSpecularBRDFLUT;

		friend class PlanetPanel;
		friend class SceneSerializer;
	public:
		// NEW PLANET SYSTEM
		static void Initialize();
		static void InitializeLevels();
		static void RebuildGrid();
		static void RebuildRingGridIndices();
		static void RebuildLODEdgeGrid();
		static LODDrawInfo DetermineActiveLODLevels(const Vector3& camPosPlanet);
		static void UpdateLevelOrigins(const Vector3& camPosPlanet);
		static Buffer& PlanetSystem::BuildLevelCB(uint32_t L);

		static void OnUpdate(const Vector3& camPosWS, const Vector3& worldTranslation, DirectX::XMMATRIX viewMatrix);

		static DirectX::XMFLOAT3& GetTranslation() { return sTranslation; }
		static Quaternion GetRotation() { return mRotationQuat; }
		static Quaternion GetInvRotation() { return mInvRotationQuat; }

		static double GetRadius() { return sRadius; }
		static double GetMaxHeight() { return sMaxHeight; }
		static double GetMinHeight() { return sMinHeight; }
		static DirectX::XMFLOAT3& GetBasisLonEast() { return sBasisLonEast; }
		static DirectX::XMFLOAT3& GetBasisLonNorth() { return sBasisLonNorth; }
		static DirectX::XMFLOAT3& GetBasisSpinUp() { return sBasisSpinUp; }

		static bool AtmosphereActivated() { return sAtmosphereActivated; }
		static bool IsValid() { return sValidPlanet; }
		static LODDrawInfo GetLODDrawInfo() { return sActiveLevels; }
		static std::vector<ClipLevel>& GetLevels() { return sLevels; }

		static Ref<VertexBuffer>& GetGridVertexBuffer() { return sGridVertexBuffer; }
		static Ref<VertexBuffer>& GetLODGridVertexBuffer() { return sLODGridVertexBuffer; }
		static Ref<IndexBuffer>& GetCenterGridIndexBuffer() { return sCenterGridIndexBuffer; }
		static Ref<IndexBuffer>& GetRingGridIndexBuffer() { return sRingGridIndexBuffer; }
		static Ref<IndexBuffer>& GetLODGridIndexBuffer() { return sLODGridIndexBuffer; }
		static uint32_t GetGridIndexCount() { return sGridIndexCount; }
		static uint32_t GetRingGridIndexCount() { return sRingGridIndexCount; }
		static uint32_t GetLODGridIndexCount() { return sLODGridIndexCount; }

		static Ref<ConstantBuffer> GetPlanetFrameCBuffer() { return sPlanetFrameCBuffer; }
		static Buffer* GetPlanetFrameBuffer() { return &sPlanetFrameBuffer; }
		static Ref<ConstantBuffer> GetPlanetLevelCBuffer() { return sPlanetLevelCBuffer; }
		static ShaderLayout* GetShaderLayout() { return &sShaderInputLayout; }

		static DirectX::XMFLOAT3& GetAlbedoColor() { return sAlbedoColor; }
		static float& GetMetalness() { return sMetalness; }
		static float& GetRoughness() { return sRoughness; }
		static Texture2D* GetBaseHeightMapTexture() { return sBaseHeightMapTexture; }

		static Texture2D* GetStarFieldTexture2D() { return sStarFieldTexture2D; }
		static Ref<TextureCube> GetStarFieldTextureCube() { return sStarFieldTextureCube; }

		static TerrainData& GetTerrainData() { return sTerrainData; }

		// These functions are used to update the active leaves during runtime.
		static void BuildPhysicsNodes(Matrix& planetNoScaleTransform);
		static void DetailObjectPlacement(TerrainObjectComponent* objects, Matrix& planetNoScaleTransform);

		static void Shutdown();

		static double ComputeCurvatureBias(double desiredSwitchHeight, double radius, double patchWidth, double focalLenPx, double screenErrorPx);
		static void GenerateDistanceLUT(uint32_t maxLevels, double planetRadius, float FoVY, uint32_t viewportWidth, double metersPerFirstCell = 1.0, float screenErrorPx = 2.0f, double spacingBias = 1.2);
		static void GenerateFaceDotLevelLUT(std::vector<double>& faceLevelDotLUT, float planetRadius, float maxHeight);
		static void GenerateHeightMultLUT(std::vector<double>& heightMultLUT, double planetRadius, double maxHeight);
	private:
		static void GetFaceBounds(const std::initializer_list<Vector3>& vertices, Bounds& bounds);

		static uint32_t GetOrAddVector3(std::unordered_map<Vector3, uint32_t, Vector3::Hasher, Vector3::Equal>& vertexMap, const Vector3& vertex, std::vector<Vector3>& vertices);

		static void AssignFaceToChunk(const Vector3& vecA, const Vector3& vecB, const Vector3& vecC, 
			std::unordered_map<std::pair<int, int>, std::vector<Vector3>, PairHash>& chunks,
			const Vector3& planetCenter);
		static void GetVerticesBounds(const std::vector<Vector3>& vertices, Bounds& bounds);
	};

}