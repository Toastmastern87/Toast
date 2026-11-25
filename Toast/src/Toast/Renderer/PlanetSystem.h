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

#include "Toast/Renderer/CubeData.h"
#include "Toast/Renderer/Frustum.h"
#include "Toast/Renderer/Mesh.h"
#include "Toast/Renderer/PlanetMaterial.h"
#include "Toast/Renderer/PlanetMesh.h"
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
		int RayleighExp10 = -10;
		DirectX::XMFLOAT3 RayleighScattering = { 0.0f, 0.0f, 0.0f };
		int MieScatteringExp10 = -5;
		DirectX::XMFLOAT3 MieScattering = { 0.0f, 0.0f, 0.0f };
		int MieAbsorptionExp10 = -6;
		DirectX::XMFLOAT3 MieAbsorption = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 MieAnisotropy = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 MieLobeWeight = { 1.0f, 1.0f, 1.0f };
		float MieAnisotropyBroad = 0.0f;
		DirectX::XMFLOAT3 GroundAlbedo = { 0.0f, 0.0f, 0.0f };
		float OzoneStrength = 0.0f;
		uint32_t StepsTransmittance = 192;
		uint32_t StepsMultiScattering = 128;
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
		Ref<PlanetMesh> mMesh;

		Ref<ConstantBuffer> mPlanetFrameCBuffer, mRenderingSettingsCBuffer;
		Buffer mPlanetFrameBuffer, mRenderingSettingsBuffer;

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
		Ref<Texture2D> mGroundIrradianceLUT;
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
		// Speed of sound in the surface air, in m/s. Drives audio doppler. Default is Earth settings!
		float mSpeedOfSound = 343.5f; 
		// Wind / turbulence (drives particle curl noise) 
		float mTurbulenceScale = 0.0f;
		// Finite-difference step for the curl
		float mTurbulenceEpsilon = 0.0f;
		// Wind vector in m/s. The particle system accumulates this into a scroll
		// offset so the noise field drifts over time instead of being a static
		// pattern particles slide through.
		DirectX::XMFLOAT3 mWindVelocity = { 0.0f, 0.0f, 0.0f };

		AssetHandle mHeightMapToCubeMapShaderHandle = 0;
		AssetHandle mHeightCubeToNormalCubeShaderHandle = 0;
		AssetHandle mHeightCubeMaxMipShaderHandle = 0;
		AssetHandle mAlbedoMapToCubeShaderHandle = 0;

		friend class SceneSerializer;
		friend class PlanetPanel;
		friend class PlanetMesh;
	public:
		Planet();

		void Initialize();

		void Shutdown();

		void OnUpdate(Camera* camera, const Quaternion& playerCamRot, const Vector3& playerCamPosWS, const Vector3& renderingCamPosWS, const Vector3& worldTranslation, DirectX::XMMATRIX viewMatrix, Frustum* frustum, const DirectX::XMFLOAT4X4& renderingCameraViewMatrix, float frustumBias);

		DirectX::XMMATRIX GetTransformRotation();
		DirectX::XMMATRIX GetTransformNoScale();
		DirectX::XMFLOAT3& GetTranslation() { return mTranslation; }
		Quaternion GetRotation() { return mRotationQuat; }
		Quaternion GetInvRotation() { return mInvRotationQuat; }

		Ref<TextureCube> CreateHeightMapCube(const Texture2D* heightMapTexture);
		void BuildHeightMaxMips(const Ref<TextureCube>& heightCube);
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
		Ref<Texture2D>& GetGroundIrradianceLUT() { return mGroundIrradianceLUT; }
		Ref<Texture2D>& GetSkyViewLUT() { return mSkyViewLUT; }
		Ref<Texture3D>& GetAerialPerspectiveLUT() { return mAerielPerspectiveLUT; }
		Ref<Texture2D>& GetAPFar() { return mAPFar; }
		Ref<Texture2D>& GetAPNear() { return mAPNear; }

		float GetSpaceFactor(Vector3 cameraPosition, const Vector3& worldTranslation);

		double ComputeCurvatureBias(double desiredSwitchHeight, double radius, double patchWidth, double focalLenPx, double screenErrorPx);

		void GenerateFaceDotLevelLUT(std::vector<double>& faceLevelDotLUT, float planetRadius, float maxHeight);
		void GenerateHeightMultLUT(std::vector<double>& heightMultLUT, double planetRadius, double maxHeight);

		float GetGravityConstant() { return mGravityConstant; }

		const std::vector<TerrainObject>& GetTerrainObjects() { return mTerrainObjects; }

		Ref<PlanetMesh>& GetMesh() { return mMesh; }

		bool IsTerrainReady() const;

		float GetPhysicsAtmosphereCeiling() { return mAtmosphereCeiling; }
		float GetSurfaceAirDensity() { return mSurfaceAirDensity; }
		float GetPhysicsScaleHeight() { return mPhysicsScaleHeight; }
		float GetSpeedOfSound() const { return mSpeedOfSound; }

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