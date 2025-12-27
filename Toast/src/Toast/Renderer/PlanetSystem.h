#pragma once

#include <future>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include <../vendor/directxtex/include/DirectXTex.h>
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
#include <random>
#include <array>

#define MAX_INT_VALUE	65535.0
#define M_PI			3.14159265358979323846
#define M_PIDIV2		(3.14159265358979323846 / 2.0)

namespace Toast {

	struct Int4
	{
		int32_t x, y, z, w;
		Int4() = default;
		Int4(int32_t _x, int32_t _y, int32_t _z, int32_t _w) : x(_x), y(_y), z(_z), w(_w) {}
	};
	static_assert(sizeof(Int4) == 16, "Int4 must be 16 bytes to match HLSL int4.");

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

	//NEW
	struct TerrainCubeData
	{
		uint32_t Width = 0;  
		uint32_t Height = 0; 

		std::array<std::vector<float>, 6> FaceHeight;
	};

	inline size_t Index2D(uint32_t x, uint32_t y, uint32_t width)
	{
		return static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
	}

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

	class Planet
	{
	// NEW PLANET SYSTEM
	private:
		// General Data
		bool mValidPlanet = false;
		uint32_t mGridSize = 0;
		uint32_t mTempGridSize = 0;
		int32_t mNumLevels = 0;
		int32_t mTempNumLevels = 0;
		std::vector<ClipLevel> mLevels;
		LODDrawInfo mActiveLevels;
		bool mRunOnce = false;

		DirectX::XMFLOAT3 mTranslation = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 mRotationEulerAngles = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT4 mRotationQuaternion = { 0.0f, 0.0f, 0.0f, 1.0f };
		Quaternion mRotationQuat;
		Quaternion mInvRotationQuat;
		Vector3 mPlanetCenterCR;
		Vector2 mCamSurfaceMeters;

		// GPU Data
		Ref<VertexBuffer> mGridVertexBuffer;
		Ref<VertexBuffer> mLODGridVertexBuffer;
		Ref<IndexBuffer> mCenterGridIndexBuffer;
		Ref<IndexBuffer> mRingGridIndexBuffer;
		Ref<IndexBuffer> mLODGridIndexBuffer;
		uint32_t mGridIndexCount = 0;
		uint32_t mRingGridIndexCount = 0;
		uint32_t mLODGridIndexCount = 0;

		Ref<ConstantBuffer> mPlanetFrameCBuffer, mPlanetLevelCBuffer;
		Buffer mPlanetFrameBuffer, mPlanetLevelBuffer;
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
		std::vector<double> mDistanceLUT;
		Texture2D* mBaseHeightMapTexture;
		Ref<TextureCube> mBaseHeightMapTextureCube;
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
		//bool mTerrainObjectsDirty = false; // any parameter that requires rebuild of instances
		//bool mTerrainObjectBuffersDirty = false; // buffer allocation must be (re)done (MaxTotal change, new object, etc.)

		// PBR Data
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
		Texture2D* mStarFieldTexture2D;
		Ref<TextureCube> mStarFieldTextureCube;
		Ref<TextureCube> mRadianceMap;
		Ref<TextureCube> mIrradianceMap;
		Texture2D* mSpecularBRDFLUT;

		// Physics
		float mGravityConstant = 0.0f;

		friend class SceneSerializer;
		friend class PlanetPanel;
	public:
		// NEW PLANET SYSTEM
		Planet();

		void Initialize();
		void InitializeLevels();

		void Shutdown();

		void RebuildGrid();
		void RebuildRingGridIndices();
		void RebuildLODEdgeGrid();
		LODDrawInfo DetermineActiveLODLevels(const Vector3& camPosPlanet);
		void UpdateLevelOrigins(const Vector3& camPosPlanet);
		Buffer& BuildLevelCB(uint32_t L);
		uint32_t GetGridSize() { return mGridSize; }

		void OnUpdate(const Vector3& camPosWS, const Vector3& worldTranslation, DirectX::XMMATRIX viewMatrix);

		DirectX::XMFLOAT3& GetTranslation() { return mTranslation; }
		Quaternion GetRotation() { return mRotationQuat; }
		Quaternion GetInvRotation() { return mInvRotationQuat; }

		Ref<TextureCube> CreateHeightMapCube(const Texture2D* heightMapTexture);

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
		bool IsValid() { return mValidPlanet; }
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

		Ref<ConstantBuffer> GetPlanetFrameCBuffer() { return mPlanetFrameCBuffer; }
		Buffer& GetPlanetFrameBuffer() { return mPlanetFrameBuffer; }
		Ref<ConstantBuffer> GetPlanetLevelCBuffer() { return mPlanetLevelCBuffer; }
		ShaderLayout* GetShaderLayout() { return &mShaderInputLayout; }

		DirectX::XMFLOAT3& GetAlbedoColor() { return mAlbedoColor; }
		float& GetMetalness() { return mMetalness; }
		float& GetRoughness() { return mRoughness; }
		Texture2D* GetBaseHeightMapTexture() { return mBaseHeightMapTexture; }
		Ref<TextureCube> GetHeightMapCubeTexture() { return mBaseHeightMapTextureCube; }

		Texture2D* GetStarFieldTexture2D() { return mStarFieldTexture2D; }
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

		float GetSpaceFactor(Vector3 cameraPosition);

		double ComputeCurvatureBias(double desiredSwitchHeight, double radius, double patchWidth, double focalLenPx, double screenErrorPx);
		void GenerateDistanceLUT(uint32_t maxLevels, double planetRadius, float FoVY, uint32_t viewportWidth, double metersPerFirstCell = 1.0, float screenErrorPx = 2.0f, double spacingBias = 1.2);

		void GenerateFaceDotLevelLUT(std::vector<double>& faceLevelDotLUT, float planetRadius, float maxHeight);
		void GenerateHeightMultLUT(std::vector<double>& heightMultLUT, double planetRadius, double maxHeight);

		TerrainCubeData LoadTerrainDataFromTextureCube();
		float GetGravityConstant() { return mGravityConstant; }

		bool ProjectWorldPosToLevelGrid(const Vector3& worldPos, const Vector3& worldTranslation, PlanetProjectionResult& out);
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
	};

}