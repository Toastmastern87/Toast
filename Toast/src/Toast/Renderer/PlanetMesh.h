#pragma once

#include "Toast/Core/Base.h"
#include "Toast/Core/Buffer.h"
#include "Toast/Core/Math/Math.h"

#include "Toast/Assets/Asset.h"

#include "Toast/Renderer/CubeData.h"
#include "Toast/Renderer/Frustum.h"
#include "Toast/Renderer/RendererBuffer.h"
#include "Toast/Renderer/Shader.h"

#include <DirectXMath.h>
#include <vector>

namespace Toast {

	class PlanetMeshBaker;

	class PlanetMesh
	{
	public:
		struct MidpointHash
		{
			static constexpr uint32_t EMPTY = UINT32_MAX;
			std::vector<uint64_t> keys;
			std::vector<uint32_t> values;
			uint32_t mask = 0;

			void clear(uint32_t capacity)
			{
				// capacity must be power of 2
				uint32_t cap = 1;
				while (cap < capacity * 2) cap <<= 1;
				mask = cap - 1;
				keys.assign(cap, UINT64_MAX);
				values.assign(cap, EMPTY);
			}

			uint32_t find(uint64_t key) const
			{
				uint32_t slot = (uint32_t)(key * 2654435761ull) & mask;
				while (true)
				{
					if (keys[slot] == key) return values[slot];
					if (keys[slot] == UINT64_MAX) return EMPTY;
					slot = (slot + 1) & mask;
				}
			}

			void insert(uint64_t key, uint32_t value)
			{
				uint32_t slot = (uint32_t)(key * 2654435761ull) & mask;
				while (keys[slot] != UINT64_MAX)
					slot = (slot + 1) & mask;
				keys[slot] = key;
				values[slot] = value;
			}
		};

		enum class NextPlanetFace
		{
			CULL, LEAF, SPLIT, SPLITCULL
		};

		struct PlanetPatchCPU
		{
			int level;

			uint32_t i0;
			uint32_t i1;
			uint32_t i2;

			PlanetPatchCPU(int Level, const uint32_t I0, const uint32_t I1, const uint32_t I2)
				: level(Level), i0(I0), i1(I1), i2(I2)
			{
			}
		};

		struct PlanetPatchGPU
		{
			int level;                 // 4

			DirectX::XMFLOAT3 V0;      // 12  unit
			DirectX::XMFLOAT3 V1;      // 12  unit
			DirectX::XMFLOAT3 V2;      // 12  unit

			DirectX::XMFLOAT3 P0_rel_hi; // 12  meters: (V0*R - camHi)
			DirectX::XMFLOAT3 P1_rel_hi; // 12
			DirectX::XMFLOAT3 P2_rel_hi; // 12

			DirectX::XMFLOAT3 PatchOriginPS;
		};
		TOAST_STATIC_ASSERT(sizeof(PlanetPatchGPU) == 88, "PlanetPatchGPU must match PlanetPatch in PlanetMeshBake.hlsl (88 bytes)");

		struct PlanetVertex
		{
			uint16_t I;
			uint16_t J;

			PlanetVertex(uint16_t i, uint16_t j)
			{
				I = i;
				J = j;
			}
		};
		TOAST_STATIC_ASSERT(sizeof(PlanetVertex) == 4, "PlanetVertex must match the R16G16_UINT TEXCOORD0 input in InitShaderLayout (4 bytes)");

		struct DeferredLeafPatch
		{
			uint32_t ia, ib, ic;
			int16_t subdivision;
		};
	public:
		PlanetMesh() = default;

		void Init();
		void InitShaderLayout();
		void GeneratePatchGeometry();

		void OnUpdate(Frustum* frustum, DirectX::XMMATRIX viewMatrixPlanetRendering, Vector3& cameraPosPS, Vector3& renderingCameraPosPS, Vector3& planetCenterWS, double radius, double maxHeight, const CubeData<float>* terrainData, uint32_t materialCount);
		void RecursiveFace(Frustum* frustum, uint32_t ia, uint32_t ib, uint32_t ic, int16_t subdivision, Vector3& cameraPosPS, bool splitCull);
		void EmitLeafPatchChecked(uint32_t ia, uint32_t ib, uint32_t ic, int16_t subdivision, Vector3& cameraPosPS);
		NextPlanetFace CheckFaceSplit(Frustum* frustum, const  Vector3& a, const  Vector3& b, const  Vector3& c, int16_t subdivision, Vector3& cameraPosPS, bool frustumCheckNeeded, double hA, double hB, double hC);

		Ref<ShaderLayout> GetShaderInputLayout() { return mShaderInputLayout; }

		uint32_t GetIndexCount() { return static_cast<uint32_t>(mIndices.size()); }
		uint16_t GetPatchLevels() { return mPatchLevels; }
		uint32_t GetPatchCount() { return static_cast<uint32_t>(mPatches.size()); }

		void BuildGPUData();
		void BindGPUData();
		void UnbindGPUData();
		void Bake();

		uint32_t GetMidpoint(uint32_t i1, uint32_t i2);

		void GenerateDistanceLUT();
		void GenerateFaceDotLevelLUT();

		void SetDistanceLUTDirty() { mDistanceLUTIsDirty = true; }

		int16_t GetMaxSubdivisions() const { return mMaxSubdivisions; }

		Vector3 GetCameraHiPS() { return mCamHiPS; }

		friend class SceneSerializer;
		friend class PlanetPanel;
	private:
		double GetCachedHeight(uint32_t idx, int16_t subdivision);

		bool HasMidpoint(uint32_t i1, uint32_t i2) const;
	private:
		double mRadius = 0.0;
		double mMaxHeight = 0.0;

		const int16_t HARDCAPSUBDIVISIONS = 25;
		int16_t mMaxSubdivisions = 0;
		int16_t mPatchLevels = 0;
		bool mPatchIsDirty = true;
		double mNearDistance = 1.0;
		double mFarDistance = 10.0;
		Vector3 mCamHiPS = { 0.0, 0.0, 0.0 };

		const CubeData<float>* mTerrainCubeData;

		std::vector<double> mHeightCache;

		std::vector<PlanetPatchCPU> mPatches;
		std::vector<PlanetPatchGPU> mPatchesGPU;

		std::vector<uint32_t> mStartIndices;
		std::vector<Vector3> mBaseIcosahedronVerts;
		std::vector<PlanetVertex> mVertices;
		std::vector<uint32_t> mIndices;
		std::vector<Vector3> mSphereVertices;
		MidpointHash mMidpointCache;
		std::vector<DeferredLeafPatch> mDeferredLeafPatches;

		Ref<VertexBuffer> mVertexBuffer;
		Ref<VertexBuffer> mInstanceVertexBuffer;
		uint32_t mInstanceBufferCapacity = 0;
		Ref<IndexBuffer> mIndexBuffer;

		Ref<ShaderLayout> mShaderInputLayout;

		Ref<ConstantBuffer> mPlanetMeshCBuffer;
		Buffer mPlanetMeshBuffer;

		std::vector<double> mDistanceLUT;
		bool mDistanceLUTIsDirty = true;
		std::vector<double> mFaceLevelDotLUT;
		bool mFaceLevelDotLUTIsDirty = true;

		double mBackfaceSafeDistSq = 0.0;

		// Settings
		bool mBackfaceCulling = true;
		bool mFrustumCulling = true;

		Ref<PlanetMeshBaker> mMeshBaker;

		AssetHandle mGPassShaderHandle = 0;
	};

}