#pragma once

#include <future>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include <../vendor/directxtex/include/DirectXTex.h>
#include "../vendor/robinhood/include/robin_hood.h"

#include "Toast/Core/Timestep.h"

#include "Toast/Core/Math/Math.h"

#include "Toast/Assets/Asset.h"

#include "Toast/Renderer/Frustum.h"
#include "Toast/Renderer/Mesh.h"
#include "Toast/Renderer/PlanetMaterial.h"
#include "Toast/Renderer/RenderCommand.h"

#include "Toast/Scene/Components.h"

#include "renderdoc_app.h"

#include <thread>
#include <mutex>
#include <future>
#include <random>
#include <array>

#define MAX_INT_VALUE	65535.0
#define M_PI			3.14159265358979323846
#define M_PIDIV2		(3.14159265358979323846 / 2.0)

namespace Toast {

	class PhysicsEngine;

	struct Int4
	{
		int32_t x, y, z, w;
		Int4() = default;
		Int4(int32_t _x, int32_t _y, int32_t _z, int32_t _w) : x(_x), y(_y), z(_z), w(_w) {}
	};
	static_assert(sizeof(Int4) == 16, "Int4 must be 16 bytes to match HLSL int4.");

	enum class PlanetMeshMode
	{
		GeometryClipmapping = 0,
		Icosphere = 1
	};

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
		uint32_t DrawMode;
		float ScatterOriginMetersX;
		float ScatterOriginMetersY;
		float FinestCellSize;
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
		float Altitude;
		DirectX::XMFLOAT3 BasisLonEast;
		int NumHeightDetails;
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

	struct HeightDetail
	{
		HeightDetail() = default;

		struct GPUData
		{
			int LODActivation = 0;
			int Octaves = 1;
			float Frequency = 1.0f;
			float Amplitude = 1.0f;
			int32_t PermBase;   // int4 index into perm table buffer
			float pad0, pad1, pad2; // pad to 32 bytes
		};

		std::string Name = "New Height Detail";
		uint32_t Seed = 0;
		int Perm[256];
		GPUData GPUSettings;
	};

	struct TerrainObject
	{
		std::string Name;
		Ref<Mesh> MeshObject;

		uint32_t Seed;
		int LODActivation = 0;

		// Scatter region around the player
		float ScatterRadiusMeters = 1000.0f;
		uint32_t CandidateGridSize = 256;
		float DensityProb = 0.05f;     // [0..1] probability a candidate keeps

		// Variation
		float MinScale = 0.1f;
		float MaxScale = 0.3f;
	};

	struct AtmosphericData 
	{
		float AtmosphereHeight = 0.0f;
		float RayleighScaleHeight = 0.0f;
		float MieScaleHeight = 0.0f;
		float MSGain = 0.0f;
		int RayleighExp10 = -10;
		DirectX::XMFLOAT3 RayleighScattering = { 0.0f, 0.0f, 0.0f };
		float SGain = 0.0f;
		int MieScatteringExp10 = -5;
		DirectX::XMFLOAT3 MieScattering = { 0.0f, 0.0f, 0.0f };
		int MieAbsorptionExp10 = -6;
		DirectX::XMFLOAT3 MieAbsorption = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 MieAnisotropy = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 GroundAlbedo = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 SunsetTint = { 0.0f, 0.0f, 0.0f };
		float OzoneStrength = 0.0f;
		uint32_t StepsTransmittance = 192;
		uint32_t StepsMultiScattering = 128;
		float APFarDynamic = 150000.0f;
	};

	struct PlanetProjectionResult
	{
		uint32_t Level;      // LOD level
		int GWorldX;         // global grid x (gWorld.x)
		int GWorldY;         // global grid y (gWorld.y)
		Vector3 NWSApprox;   // approx normal used by GPU for this grid sample
		double TangentDist;  // tangent-plane distance from camera (meters)
	};

	inline size_t Index2D(uint32_t x, uint32_t y, uint32_t width)
	{
		return static_cast<size_t>(y) * width + x;
	}

	template<typename T>
	struct CubeData
	{
		uint32_t Width = 0;
		uint32_t Height = 0;
		std::array<std::vector<T>, 6> FaceData;
	};

	inline float smoothstep(float a, float b, float x)
	{
		if (a == b) return 0.0f; // avoid divide-by-zero
		float t = std::clamp((x - a) / (b - a), 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}

	class PlanetMeshIcosphere
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

		struct PlanetVertexCPU
		{
			uint16_t I;
			uint16_t J;

			PlanetVertexCPU(uint16_t i, uint16_t j)
			{
				I = i;
				J = j;
			}
		};

		struct PlanetVertexGPU
		{
			uint16_t I;
			uint16_t J;

			PlanetVertexGPU(uint16_t i, uint16_t j)
			{
				I = i;
				J = j;
			}
		};

		struct DeferredLeafPatch
		{
			uint32_t ia, ib, ic;
			int16_t subdivision;
		};
	public:
		PlanetMeshIcosphere() = default;

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

		Ref<ConstantBuffer>& GetPlanetMeshCBuffer() { return mPlanetMeshCBuffer; }

		uint32_t GetMidpoint(uint32_t i1, uint32_t i2);

		void GenerateDistanceLUT();
		void GenerateFaceDotLevelLUT();
		void GenerateHeightMultLUT();

		bool& GetBackfaceCulling() { return mBackfaceCulling; }
		bool& GetFrustumCulling() { return mFrustumCulling; }

		void SetDistanceLUTDirty() { mDistanceLUTIsDirty = true; }

		int16_t GetMaxSubdivisions() const { return mMaxSubdivisions; }

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
		int16_t mHighestSubdivision = 0;
		int16_t mPatchLevels = 0;
		bool mPatchIsDirty = true;
		double mNearDistance = 1.0;
		double mFarDistance = 10.0;
		Vector3 mCamHiPS = { 0.0, 0.0, 0.0 };

		const CubeData<float>* mTerrainCubeData;
		const CubeData<uint32_t> mAlbedoCubeData;
		
		std::vector<double> mHeightCache;

		std::vector<PlanetPatchCPU> mPatches;
		std::vector<PlanetPatchGPU> mPatchesGPU;

		std::vector<uint32_t> mStartIndices;
		std::vector<Vector3> mBaseIcosahedronVerts;
		std::vector<PlanetVertexCPU> mVertices;
		std::vector<PlanetVertexGPU> mVerticesGPU;
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
		std::vector<double> mHeightMultLUT;
		bool mHeightMultLUTIsDirty = true;

		double mBackfaceSafeDistSq = 0.0;

		// Settings
		bool mBackfaceCulling;
		bool mFrustumCulling;

		AssetHandle mIcosphereGPassShaderHandle = 0;
	};

	class PlanetMeshGeoClipmap
	{
	public:
		PlanetMeshGeoClipmap() = default;

		void Init();
		void InitShaderLayout();

		void OnUpdate(PhysicsEngine* physicsEngine, double radius, double maxHeight, const Vector3& playerCamPosPS, const CubeData<float>* terrainData, const Vector3& camTangent, const double& shiftEast, const double& shiftNorth);

		void BuildGPUData();
		void BindGPUData();

		void RebuildGrid();
		void RebuildRingGridIndices();
		void RebuildLODEdgeGrid();
		LODDrawInfo DetermineActiveLODLevels(const Vector3& camPosPS, PhysicsEngine* physicsEngine);
		Buffer& BuildLevelCB(uint32_t L);
		uint32_t GetGridSize() { return mGridSize; }

		LODDrawInfo GetLODDrawInfo() { return mActiveLevels; }
		std::vector<ClipLevel>& GetLevels() { return mLevels; }

		Ref<VertexBuffer>& GetGridVertexBuffer() { return mGridVertexBuffer; }
		Ref<VertexBuffer>& GetLODGridVertexBuffer() { return mLODGridVertexBuffer; }
		Ref<IndexBuffer>& GetCenterGridIndexBuffer() { return mCenterGridIndexBuffer; }
		Ref<IndexBuffer>& GetRingGridIndexBuffer() { return mRingGridIndexBuffer; }
		Ref<IndexBuffer>& GetLODGridIndexBuffer() { return mLODGridIndexBuffer; }
		uint32_t GetGridIndexCount() { return mGridIndexCount; }
		uint32_t GetRingGridIndexCount() { return mRingGridIndexCount; }
		uint32_t GetLODGridIndexCount() { return mLODGridIndexCount; }

		Ref<ConstantBuffer> GetPlanetLevelCBuffer() { return mPlanetLevelCBuffer; }

		void GenerateDistanceLUT(uint32_t maxLevels, double planetRadius, float FoVY, uint32_t viewportWidth, double metersPerFirstCell = 1.0, float screenErrorPx = 2.0f, double spacingBias = 1.2);

		bool IsPlanetMeshValid() const { return mValidMesh; }

		friend class SceneSerializer;
		friend class PlanetPanel;
		friend class Planet;
	private:
		void UpdateLevelOrigins(const Vector3& camTangent);

		bool IsTerrainReady() const;

		double ComputeCurvatureBias(double desiredSwitchHeight, double radius, double patchWidth, double focalLenPx, double screenErrorPx);
	private:
		bool mValidMesh = false;
		bool mRunOnce = false;

		double mRadius = 0.0;
		double mMaxHeight = 0.0;
		double mShiftEastM;
		double mShiftNorthM;

		Ref<ConstantBuffer> mPlanetLevelCBuffer;
		Buffer mPlanetLevelBuffer;

		uint32_t mGridSize = 0;
		uint32_t mTempGridSize = 0;
		int32_t mNumLevels = 0;
		int32_t mTempNumLevels = 0;
		std::vector<ClipLevel> mLevels;
		LODDrawInfo mActiveLevels;

		Ref<VertexBuffer> mGridVertexBuffer;
		Ref<VertexBuffer> mLODGridVertexBuffer;
		Ref<IndexBuffer> mCenterGridIndexBuffer;
		Ref<IndexBuffer> mRingGridIndexBuffer;
		Ref<IndexBuffer> mLODGridIndexBuffer;
		uint32_t mGridIndexCount = 0;
		uint32_t mRingGridIndexCount = 0;
		uint32_t mLODGridIndexCount = 0;

		bool mGridIsDirty = false;
		bool mRingGridIsDirty = false;
		bool mLODGridIsDirty = false;

		std::vector<double> mDistanceLUT;

		const CubeData<float>* mTerrainCubeData;
	};

	class Planet
	{
	private:
		DirectX::XMFLOAT3 mTranslation = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 mRotationEulerAngles = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT4 mRotationQuaternion = { 0.0f, 0.0f, 0.0f, 1.0f };
		Quaternion mRotationQuat;
		Quaternion mInvRotationQuat;
		Vector3 mPlanetCenterCR;
		Vector2 mCamSurfaceMeters;
		double mShiftEastM;
		double mShiftNorthM;

		Vector3 mCameraPlanetSpace;

		// Mesh Data
		Ref<PlanetMeshIcosphere> mIcosphereMesh;
		Ref<PlanetMeshGeoClipmap> mGeoClipmapMesh;
		PlanetMeshMode mMeshMode = PlanetMeshMode::GeometryClipmapping;

		Ref<ConstantBuffer> mPlanetFrameCBuffer, mRenderingSettingsCBuffer;
		Buffer mPlanetFrameBuffer, mRenderingSettingsBuffer;
		ShaderLayout mShaderInputLayout;

		// Terrain Data
		double mRadius = 0.0;
		double mMaxHeight = 0.0;
		double mMinHeight = 0.0;
		double mAltitude = 0.0;
		bool mWallEnhancementEnabled = true;
		float mWallStrength = 0.75f;
		float mWallStepMeters = 3000.0f;
		float mWallSlopeStart = 0.50f;
		float mWallSlopeEnd = 1.40f;
		float mWallSharpStart = 0.40f;
		float mWallSharpEnd = 0.60f;
		float mWallMaxDelta = 2000.0f;
		bool mWallDebugEnabled = false;
		int mWallDebugMode = 0;
		float mTerrainNormalStepMeters = 500.0f;
		bool mErosionEnabled = false;
		float mErosionStrength = 20.0f;
		float mErosionStepMeters = 3000.0f;
		float mErosionTilingMeters = 400.0f;
		float mErosionSlopeStart = 0.18f;     // roughly 35 degrees using 1-dot slope
		float mErosionSlopeFull = 0.29f;      // roughly 45 degrees
		float mErosionSlopeEnd = 0.50f;       // roughly 60 degrees
		float mErosionSlopeFadeOut = 0.65f;
		int mErosionOctaves = 4;
		float mErosionLacunarity = 2.0f;
		float mErosionPersistence = 0.5f;
		bool mErosionDebugEnabled = false;
		int mErosionDebugMode = 0;
		float mErosionGullyWeight = 0.32f;
		float mErosionDetail = 1.20f;
		float mErosionCellScale = 0.95f;
		float mErosionNormalization = 0.30f;
		float mErosionAssumedSlope = 0.55f;
		float mErosionAssumedSlopeBlend = 0.75f;
		float mErosionMaxDistance = 50000.0f;
		float mErosionFadeStart = 20000.0f;
		DirectX::XMFLOAT3 mBasisLonEast;
		DirectX::XMFLOAT3 mBasisLonNorth;
		DirectX::XMFLOAT3 mBasisSpinUp;
		DirectX::XMFLOAT3 mBasisRadUp;
		DirectX::XMFLOAT3 mBasisTanEast;
		DirectX::XMFLOAT3 mBasisTanNorth;
		AssetHandle mBaseHeightMapHandle = 0;
		Ref<TextureCube> mBaseHeightMapTextureCube;
		Ref<TextureCube> mNormalMapTextureCube;
		Ref<TextureCube> mAlbedoMapTextureCube;
		TerrainData mTerrainData;
		CubeData<float> mTerrainCubeData;
		CubeData<uint32_t> mAlbedoCubeData;
		std::vector<HeightDetail> mHeightDetails;
		Ref<StructuredBuffer> mHeightDetailSettingsSB;
		Ref<StructuredBuffer> mHeightDetailPermSB;
		bool mHeightDetailsDirty = true;
		uint32_t mLastHeightDetailCount = 0;
		std::vector<TerrainObject> mTerrainObjects;
		Ref<ConstantBuffer> mTerrainObjectCBuffer;
		Buffer mTerrainObjectBuffer;

		// Materials
		bool mMaterialsEnabled = true;
		float mPBRColorDominance = 0.0f;
		float mColorNoiseFrequency = 0.005f;    // default 0.005
		float mColorNoiseStrength = 0.15f;     // default 0.15
		int mColorNoiseOctaves = 3;      // default 3
		std::vector<PlanetMaterial> mMaterials;
		Ref<StructuredBuffer> mMaterialSB;
		Ref<StructuredBuffer> mMaterialNoiseSB;
		Ref<StructuredBuffer> mMaterialNoisePermSB;
		bool mMaterialsIsDirty = true;
		uint32_t mLastMaterialCount = 0;
		uint32_t mLastMaterailNoiseCount = 0;
		// PBR texture arrays (one slice per material, 5 channels)
		Ref<Texture2DArray> mPBRAlbedoArray;
		Ref<Texture2DArray> mPBRNormalArray;
		Ref<Texture2DArray> mPBRRoughnessArray;
		Ref<Texture2DArray> mPBRAOArray;
		Ref<Texture2DArray> mPBRDisplacementArray;
		bool mPBRTexturesDirty = true;
		uint32_t mLastPBRMaterialCount = 0;
		uint32_t mLastPBRTextureSize = 0;

		// PBR Data
		uint32_t mUseAlbedoMap = 0;
		AssetHandle mAlbedoTextureHandle = 0;
		DirectX::XMFLOAT3 mAlbedoColor = { 0.0f, 0.0f, 0.0f };
		float mRoughness = 0.0f;
		float mMetalness = 0.0f;
		Ref<ConstantBuffer> mPlanetMaterialCBuffer;
		Buffer mPlanetMaterialBuffer;

		// Atmosphere Scattering Data
		bool mAtmosphereActivated = false;
		AtmosphericData mAtmosphere;
		Ref<Texture2D> mTransmittanceLUT;
		Ref<Texture2D> mMultiScatteringLUT;
		Ref<Texture2D> mSkyViewLUT;
		Ref<Texture3D> mAerielPerspectiveLUT;
		Ref<Texture2D> mAPFar;
		Ref<Texture2D> mAPNear;

		// Environment Textures
		AssetHandle mStarFieldTexture2DHandle = 0;
		Ref<TextureCube> mStarFieldTextureCube;
		Texture2D* mSpecularBRDFLUT;

		// Physics
		float mGravityConstant = 0.0f;
		float mSurfaceAirDensity = 0.0f;      
		float mPhysicsScaleHeight = 0.0f;   
		float mAtmosphereCeiling = 0.0f; 
		// Wind / turbulence (drives particle curl noise) 
		float mTurbulenceScale = 0.0f;
		// Finite-difference step for the curl
		float mTurbulenceEpsilon = 0.0f;
		// Wind vector in m/s. The particle system accumulates this into a scroll
		// offset so the noise field drifts over time instead of being a static
		// pattern particles slide through.
		DirectX::XMFLOAT3 mWindVelocity = { 0.0f, 0.0f, 0.0f };

		AssetHandle mGeoClipmapGPassShaderHandle = 0;
		AssetHandle mHeightMapToCubeMapShaderHandle = 0;
		AssetHandle mHeightCubeToNormalCubeShaderHandle = 0;
		AssetHandle mAlbedoMapToCubeShaderHandle = 0;

		friend class SceneSerializer;
		friend class PlanetPanel;
		friend class PlanetMeshIcosphere;
		friend class PlanetMeshGeoClipmap;
	public:
		Planet();

		void Initialize();

		void Shutdown();

		void OnUpdate(Camera* camera, const Quaternion& playerCamRot, const Vector3& playerCamPosWS, const Vector3& renderingCamPosWS, const Vector3& worldTranslation, DirectX::XMMATRIX viewMatrix, PhysicsEngine* physicsEngine, Frustum* frustum, const DirectX::XMFLOAT4X4& renderingCameraViewMatrix, float frustumBias);

		DirectX::XMMATRIX GetTransformRotation();
		DirectX::XMMATRIX GetTransformNoScale();
		DirectX::XMFLOAT3& GetTranslation() { return mTranslation; }
		Quaternion GetRotation() { return mRotationQuat; }
		Quaternion GetInvRotation() { return mInvRotationQuat; }

		Ref<TextureCube> CreateHeightMapCube(const Texture2D* heightMapTexture);
		Ref<TextureCube> CreateNormalMapCube(const TextureCube* heightCube);
		Ref<TextureCube> CreateAlbedoCube(const Texture2D* albedoTexture);

		double GetRadius() const { return mRadius; }
		double GetMaxHeight() { return mMaxHeight; }
		double GetMinHeight() { return mMinHeight; }
		double GetHeightDetailsAtDir(const Vector3& dirPlanet, const DirectX::XMVECTOR& cameraPlanetSpace) const;
		double GetAltitude() const { return mAltitude; }
		DirectX::XMFLOAT3& GetBasisLonEast() { return mBasisLonEast; }
		DirectX::XMFLOAT3& GetBasisLonNorth() { return mBasisLonNorth; }
		DirectX::XMFLOAT3& GetBasisSpinUp() { return mBasisSpinUp; }
		DirectX::XMFLOAT3& GetBasisRadUp() { return mBasisRadUp; }
		DirectX::XMFLOAT3& GetBasisTanEast() { return mBasisTanEast; }
		DirectX::XMFLOAT3& GetBasisTanNorth() { return mBasisTanNorth; }

		Vector3& GetCameraPlanetSpace() { return mCameraPlanetSpace; }

		bool AtmosphereActivated() { return mAtmosphereActivated; }

		Ref<ConstantBuffer> GetPlanetFrameCBuffer() { return mPlanetFrameCBuffer; }
		Buffer& GetPlanetFrameBuffer() { return mPlanetFrameBuffer; }
		Ref<ConstantBuffer> GetPlanetRenderingSettingsCBuffer() { return mRenderingSettingsCBuffer; }
		ShaderLayout* GetShaderLayout() { return &mShaderInputLayout; }

		// Terrain Materials
		const std::vector<PlanetMaterial>& GetTerrainMaterials() const { return mMaterials; }
		size_t GetNumMaterials() { return mMaterials.size(); }
		Ref<StructuredBuffer> GetMaterialSB() { return mMaterialSB; }
		Ref<StructuredBuffer> GetMaterialNoiseSB() { return mMaterialNoiseSB; }
		Ref<StructuredBuffer> GetMaterialNoisePermSB() { return mMaterialNoisePermSB; }
		void UploadMaterialsToGPU();
		void BuildPermutationTable(uint32_t seed, int outPerm[256]);
		Ref<Texture2DArray> GetPBRAlbedoArray() { return mPBRAlbedoArray; }
		Ref<Texture2DArray> GetPBRNormalArray() { return mPBRNormalArray; }
		Ref<Texture2DArray> GetPBRRoughnessArray() { return mPBRRoughnessArray; }
		Ref<Texture2DArray> GetPBRAOArray() { return mPBRAOArray; }
		Ref<Texture2DArray> GetPBRDisplacementArray() { return mPBRDisplacementArray; }
		void RebuildPBRTextureArrays();

		uint32_t& GetUseAlbedoMap() { return mUseAlbedoMap; }
		DirectX::XMFLOAT3& GetAlbedoColor() { return mAlbedoColor; }
		float& GetMetalness() { return mMetalness; }
		float& GetRoughness() { return mRoughness; }
		AssetHandle GetBaseHeightMapHandle() { return mBaseHeightMapHandle; }
		Ref<TextureCube> GetHeightMapCubeTexture() { return mBaseHeightMapTextureCube; }
		Ref<TextureCube> GetNormalMapCubeTexture() { return mNormalMapTextureCube; }
		Ref<TextureCube> GetAlbedoCubeTexture() { return mAlbedoMapTextureCube; }
		void MapRenderingSettings();

		Ref<TextureCube> GetStarFieldTextureCube() { return mStarFieldTextureCube; }

		TerrainData& GetTerrainData() { return mTerrainData; }
		const CubeData<float>& GetTerrainCubeData() const { return mTerrainCubeData; }
		const CubeData<uint32_t>& GetAlbedoCubeData() const { return mAlbedoCubeData; }

		AtmosphericData& GetAtmosphere() { return mAtmosphere; }
		Ref<Texture2D>& GetTransmittanceLUT() { return mTransmittanceLUT; }
		Ref<Texture2D>& GetMultiScatteringLUT() { return mMultiScatteringLUT; }
		Ref<Texture2D>& GetSkyViewLUT() { return mSkyViewLUT; }
		Ref<Texture3D>& GetAerialPerspectiveLUT() { return mAerielPerspectiveLUT; }
		Ref<Texture2D>& GetAPFar() { return mAPFar; }
		Ref<Texture2D>& GetAPNear() { return mAPNear; }

		float GetSpaceFactor(Vector3 cameraPosition, const Vector3& worldTranslation);

		double ComputeCurvatureBias(double desiredSwitchHeight, double radius, double patchWidth, double focalLenPx, double screenErrorPx);

		void GenerateFaceDotLevelLUT(std::vector<double>& faceLevelDotLUT, float planetRadius, float maxHeight);
		void GenerateHeightMultLUT(std::vector<double>& heightMultLUT, double planetRadius, double maxHeight);

		float GetGravityConstant() { return mGravityConstant; }

		uint32_t GetLODForWorldPos(const Vector3& worldPosWS);

		//size_t GetNumHeightDetails() { return mHeightDetails.size(); }
		//const std::vector<HeightDetail>& GetHeightDetails() { return mHeightDetails; }
		//Ref<StructuredBuffer> GetHeightDetailSettingsSB() { return mHeightDetailSettingsSB; }
		//Ref<StructuredBuffer> GetHeightDetailPermSB() { return mHeightDetailPermSB; }
		////void UploadHeightDetailsToGPU();

		const std::vector<TerrainObject>& GetTerrainObjects() { return mTerrainObjects; }
		Ref<ConstantBuffer> GetTerrainObjectCBuffer() { return mTerrainObjectCBuffer; }
		Buffer& GetTerrainObjectBuffer() { return mTerrainObjectBuffer; }

		uint32_t ObjectInstancesForLevelFromDensity(const TerrainObject& o, uint32_t cellSize, uint32_t gridSize);

		void SetMeshMode(PlanetMeshMode mode) { mMeshMode = mode; }
		PlanetMeshMode GetMeshMode() const { return mMeshMode; }

		Ref<PlanetMeshIcosphere>& GetIcosphereMesh() { return mIcosphereMesh; }
		Ref<PlanetMeshGeoClipmap>& GetGeoClipmapMesh() { return mGeoClipmapMesh; }

		bool IsTerrainReady() const;

		float GetPhysicsAtmosphereCeiling() { return mAtmosphereCeiling; }
		float GetSurfaceAirDensity() { return mSurfaceAirDensity; }
		float GetPhysicsScaleHeight() { return mPhysicsScaleHeight; }

		float GetWallEnhancementEnabled() const { return mWallEnhancementEnabled; }
		float GetWallStrength() const { return mWallStrength; }
		float GetWallStepMeters() const { return mWallStepMeters; }
		float GetWallSlopeStart() const { return mWallSlopeStart; }
		float GetWallSlopeEnd() const { return mWallSlopeEnd; }
		float GetWallSharpStart() const { return mWallSharpStart; }
		float GetWallSharpEnd() const { return mWallSharpEnd; }
		float GetWallMaxDelta() const { return mWallMaxDelta; }

		// --- Erosion ---
		float GetErosionEnabled() const { return mErosionEnabled; }
		float GetErosionStrength() const { return mErosionStrength; }
		float GetErosionStepMeters() const { return mErosionStepMeters; }
		float GetErosionTilingMeters() const { return mErosionTilingMeters; }
		float GetErosionSlopeStart() const { return mErosionSlopeStart; }
		float GetErosionSlopeFull() const { return mErosionSlopeFull; }
		float GetErosionSlopeEnd() const { return mErosionSlopeEnd; }
		float GetErosionSlopeFadeOut() const { return mErosionSlopeFadeOut; }
		int GetErosionOctaves() const { return mErosionOctaves; }
		float GetErosionLacunarity() const { return mErosionLacunarity; }
		float GetErosionPersistence() const { return mErosionPersistence; }
		float GetErosionGullyWeight() const { return mErosionGullyWeight; }
		float GetErosionDetail() const { return mErosionDetail; }
		float GetErosionCellScale() const { return mErosionCellScale; }
		float GetErosionNormalization() const { return mErosionNormalization; }
		float GetErosionAssumedSlope() const { return mErosionAssumedSlope; }
		float GetErosionAssumedSlopeBlend() const { return mErosionAssumedSlopeBlend; }
		float GetErosionMaxDistance() const { return mErosionMaxDistance; }
		float GetErosionFadeStart() const { return mErosionFadeStart; }

		float GetTurbulenceScale() const { return mTurbulenceScale; }
		float GetTurbulenceEpsilon() const { return mTurbulenceEpsilon; }
		const DirectX::XMFLOAT3& GetWindVelocity() const { return mWindVelocity; }

		template<typename T>
		static CubeData<T> LoadCubeData(const Ref<TextureCube>& source);
	};

}