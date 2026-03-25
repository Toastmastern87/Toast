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
#include "Toast/Renderer/RenderCommand.h"
#include "Toast/Renderer/TerrainCubeData.h"

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

		// Spawning control
		float DensityPerKm2;  // main dial
		int   MaxPerPatch;       // hard cap 
		int   MaxTotal;   // safety cap layer-wide

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
			CULL, LEAF, LEAFPATCH, SPLIT, SPLITCULL
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

	public:
		PlanetMeshIcosphere() = default;

		void Init();
		void InitShaderLayout();
		void GeneratePatchGeometry();

		void OnUpdate(Frustum* frustum, DirectX::XMMATRIX viewMatrixPlanetRendering, Vector3& cameraPosPS, Vector3& renderingCameraPosPS, Vector3& planetCenterWS, double radius, double maxHeight, const TerrainCubeData& terrainData);
		void RecursiveFace(Frustum* frustum, uint32_t ia, uint32_t ib, uint32_t ic, int16_t subdivision, Vector3& cameraPosPS, bool splitCull);
		NextPlanetFace CheckFaceSplit(Frustum* frustum, const  Vector3& a, const  Vector3& b, const  Vector3& c, int16_t subdivision, Vector3& cameraPosPS, bool frustumCheckNeeded, double hA, double hB, double hC);

		Ref<ShaderLayout> GetShaderInputLayout() { return mShaderInputLayout; }

		uint32_t GetIndexCount() { return static_cast<uint32_t>(mIndices.size()); }
		uint16_t GetPatchLevels() { return mPatchLevels; }
		uint32_t GetPatchCount() { return static_cast<uint32_t>(mPatches.size()); }

		void BuildGPUData();
		void BindGPUData();

		uint32_t GetMidpoint(uint32_t i1, uint32_t i2);

		void GenerateDistanceLUT();
		void GenerateFaceDotLevelLUT();
		void GenerateHeightMultLUT();

		bool& GetBackfaceCulling() { return mBackfaceCulling; }
		bool& GetFrustumCulling() { return mFrustumCulling; }

		void SetDistanceLUTDirty() { mDistanceLUTIsDirty = true; }

		friend class SceneSerializer;
		friend class PlanetPanel;
	private:
		double GetCachedHeight(uint32_t idx, int16_t subdivision);
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

		const TerrainCubeData* mTerrainCubeData;
		
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

		// Settings
		bool mBackfaceCulling;
		bool mFrustumCulling;
	};

	class PlanetMeshGeoClipmap
	{
	public:
		PlanetMeshGeoClipmap() = default;

		void Init();
		void InitShaderLayout();

		void OnUpdate(PhysicsEngine* physicsEngine, double radius, double maxHeight, const Vector3& playerCamPosPS, TerrainCubeData* mTerrainCubeData, const Vector3& camTangent, const double& shiftEast, const double& shiftNorth);

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

		TerrainCubeData* mTerrainCubeData;
	};

	class Planet
	{
	private:
		// General Data
		//bool mValidPlanet = false;
		//uint32_t mGridSize = 0;
		//uint32_t mTempGridSize = 0;
		//int32_t mNumLevels = 0;
		//int32_t mTempNumLevels = 0;
		//std::vector<ClipLevel> mLevels;
		//LODDrawInfo mActiveLevels;
		//bool mRunOnce = false;

		DirectX::XMFLOAT3 mTranslation = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 mRotationEulerAngles = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT4 mRotationQuaternion = { 0.0f, 0.0f, 0.0f, 1.0f };
		Quaternion mRotationQuat;
		Quaternion mInvRotationQuat;
		Vector3 mPlanetCenterCR;
		Vector2 mCamSurfaceMeters;
		double mShiftEastM;
		double mShiftNorthM;

		// Mesh Data
		Ref<PlanetMeshIcosphere> mIcosphereMesh;
		Ref<PlanetMeshGeoClipmap> mGeoClipmapMesh;
		PlanetMeshMode mMeshMode = PlanetMeshMode::GeometryClipmapping;

		// GPU Data
		//Ref<VertexBuffer> mGridVertexBuffer;
		//Ref<VertexBuffer> mLODGridVertexBuffer;
		//Ref<IndexBuffer> mCenterGridIndexBuffer;
		//Ref<IndexBuffer> mRingGridIndexBuffer;
		//Ref<IndexBuffer> mLODGridIndexBuffer;
		//uint32_t mGridIndexCount = 0;
		//uint32_t mRingGridIndexCount = 0;
		//uint32_t mLODGridIndexCount = 0;

		Ref<ConstantBuffer> mPlanetFrameCBuffer, mRenderingSettingsCBuffer;
		Buffer mPlanetFrameBuffer, mRenderingSettingsBuffer;
		ShaderLayout mShaderInputLayout;

		// Terrain Data
		double mRadius = 0.0;
		double mMaxHeight = 0.0;
		double mMinHeight = 0.0;
		double mAltitude = 0.0;
		DirectX::XMFLOAT3 mBasisLonEast;
		DirectX::XMFLOAT3 mBasisLonNorth;
		DirectX::XMFLOAT3 mBasisSpinUp;
		DirectX::XMFLOAT3 mBasisRadUp;
		DirectX::XMFLOAT3 mBasisTanEast;
		DirectX::XMFLOAT3 mBasisTanNorth;
		//std::vector<double> mDistanceLUT;
		AssetHandle mBaseHeightMapHandle;
		Ref<TextureCube> mBaseHeightMapTextureCube;
		Ref<TextureCube> mNormalMapTextureCube;
		Ref<TextureCube> mAlbedoMapTextureCube;
		TerrainData mTerrainData;
		TerrainCubeData mTerrainCubeData;
		std::vector<HeightDetail> mHeightDetails;
		Ref<StructuredBuffer> mHeightDetailSettingsSB;
		Ref<StructuredBuffer> mHeightDetailPermSB;
		bool mHeightDetailsDirty = true;
		uint32_t mLastHeightDetailCount = 0;
		std::vector<TerrainObject> mTerrainObjects;
		Ref<ConstantBuffer> mTerrainObjectCBuffer;
		Buffer mTerrainObjectBuffer;

		// PBR Data
		uint32_t mUseAlbedoMap = 0;
		AssetHandle mAlbedoTextureHandle;
		DirectX::XMFLOAT3 mAlbedoColor = { 0.0f, 0.0f, 0.0f };
		float mRoughness = 0.0f;
		float mMetalness = 0.0f;
		Ref<ConstantBuffer> mPlanetMaterialCBuffer;
		Buffer mPlanetMaterialBuffer;
		float mSlopeSensitivity = 30.0f;
		float mSlopeThreshold = 0.3f;
		float mSlopeDarkening = 0.5f;

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
		AssetHandle mStarFieldTexture2DHandle;
		Ref<TextureCube> mStarFieldTextureCube;
		Texture2D* mSpecularBRDFLUT;

		// Physics
		float mGravityConstant = 0.0f;

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

		double GetRadius() { return mRadius; }
		double GetMaxHeight() { return mMaxHeight; }
		double GetMinHeight() { return mMinHeight; }
		double GetAltitude() const { return mAltitude; }
		DirectX::XMFLOAT3& GetBasisLonEast() { return mBasisLonEast; }
		DirectX::XMFLOAT3& GetBasisLonNorth() { return mBasisLonNorth; }
		DirectX::XMFLOAT3& GetBasisSpinUp() { return mBasisSpinUp; }
		DirectX::XMFLOAT3& GetBasisRadUp() { return mBasisRadUp; }
		DirectX::XMFLOAT3& GetBasisTanEast() { return mBasisTanEast; }
		DirectX::XMFLOAT3& GetBasisTanNorth() { return mBasisTanNorth; }

		bool AtmosphereActivated() { return mAtmosphereActivated; }

		Ref<ConstantBuffer> GetPlanetFrameCBuffer() { return mPlanetFrameCBuffer; }
		Buffer& GetPlanetFrameBuffer() { return mPlanetFrameBuffer; }
		Ref<ConstantBuffer> GetPlanetRenderingSettingsCBuffer() { return mRenderingSettingsCBuffer; }
		ShaderLayout* GetShaderLayout() { return &mShaderInputLayout; }

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
		TerrainCubeData& GetTerrainCubeData() { return mTerrainCubeData; }

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

		TerrainCubeData LoadTerrainDataFromTextureCube();
		float GetGravityConstant() { return mGravityConstant; }

		uint32_t GetLODForWorldPos(const Vector3& worldPosWS);

		size_t GetNumHeightDetails() { return mHeightDetails.size(); }
		const std::vector<HeightDetail>& GetHeightDetails() { return mHeightDetails; }
		Ref<StructuredBuffer> GetHeightDetailSettingsSB() { return mHeightDetailSettingsSB; }
		Ref<StructuredBuffer> GetHeightDetailPermSB() { return mHeightDetailPermSB; }
		void UploadHeightDetailsToGPU();
		void BuildPermutationTable(uint32_t seed, int outPerm[256]);

		const std::vector<TerrainObject>& GetTerrainObjects() { return mTerrainObjects; }
		Ref<ConstantBuffer> GetTerrainObjectCBuffer() { return mTerrainObjectCBuffer; }
		Buffer& GetTerrainObjectBuffer() { return mTerrainObjectBuffer; }

		uint32_t ObjectInstancesForLevelFromDensity(const TerrainObject& o, uint32_t cellSize, uint32_t gridSize);

		void SetMeshMode(PlanetMeshMode mode) { mMeshMode = mode; }
		PlanetMeshMode GetMeshMode() const { return mMeshMode; }

		Ref<PlanetMeshIcosphere>& GetIcosphereMesh() { return mIcosphereMesh; }
		Ref<PlanetMeshGeoClipmap>& GetGeoClipmapMesh() { return mGeoClipmapMesh; }

		bool IsTerrainReady() const;
	};

}