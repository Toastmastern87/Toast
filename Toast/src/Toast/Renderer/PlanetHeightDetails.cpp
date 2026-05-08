#include "tpch.h"
#include "PlanetHeightDetails.h"
#include "TerrainSampler.h"
#include "Toast/Utils/PerlinNoise.h"

#include <DirectXMath.h>

namespace Toast {

	constexpr int MAX_MATERIALS_CPU = 8;

	static float ComputeMaterialWeightCPU(const PlanetMaterial::GPUData& mat, float slope, float colorAvg)
	{
		auto saturate = [](float x) { return std::clamp(x, 0.0f, 1.0f); };

		float slopeScore = 1.0f;
		if (slope < mat.SlopeMin)
			slopeScore = saturate(1.0f - (mat.SlopeMin - slope) * mat.BlendSharpness);
		else if (slope > mat.SlopeMax)
			slopeScore = saturate(1.0f - (slope - mat.SlopeMax) * mat.BlendSharpness);

		float colorScore = 1.0f;
		if (mat.UseAlbedo > 0.5f)
		{
			if (colorAvg < mat.ColorAvgMin)
				colorScore = saturate(1.0f - (mat.ColorAvgMin - colorAvg) * mat.BlendSharpness);
			else if (colorAvg > mat.ColorAvgMax)
				colorScore = saturate(1.0f - (colorAvg - mat.ColorAvgMax) * mat.BlendSharpness);
		}

		return slopeScore * colorScore;
	}

	static void ComputeAllMaterialWeightsCPU(const std::vector<PlanetMaterial>& materials, float slope, float colorAvg, float* outWeights)
	{
		float totalWeight = 0.0f;
		int count = std::min((int)materials.size(), MAX_MATERIALS_CPU);

		for (int m = 0; m < count; ++m)
		{
			outWeights[m] = ComputeMaterialWeightCPU(materials[m].GPU, slope, colorAvg);
			totalWeight += outWeights[m];
		}
		for (int m = count; m < MAX_MATERIALS_CPU; ++m)
			outWeights[m] = 0.0f;

		float invTotal = (totalWeight > 0.001f) ? (1.0f / totalWeight) : 0.0f;
		for (int m = 0; m < count; ++m)
			outWeights[m] *= invTotal;
	}

	// Mirror of HLSL BuildErosionFramePS — picks two world-aligned axes
	// based on the dominant component of dir.
	static void BuildErosionFrame(const DirectX::XMVECTOR& dir, DirectX::XMVECTOR& axisA, DirectX::XMVECTOR& axisB)
	{
		DirectX::XMFLOAT3 d;
		DirectX::XMStoreFloat3(&d, dir);

		float ax = fabsf(d.x);
		float ay = fabsf(d.y);
		float az = fabsf(d.z);

		if (ax >= ay && ax >= az)
		{
			axisA = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
			axisB = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		}
		else if (ay >= ax && ay >= az)
		{
			axisA = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
			axisB = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
		}
		else
		{
			axisA = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
			axisB = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		}
	}

	// Convert XMVECTOR (planet-space dir) to Vector3 for the existing sampler
	static Vector3 XMVectorToVec3(const DirectX::XMVECTOR& v)
	{
		DirectX::XMFLOAT3 f;
		DirectX::XMStoreFloat3(&f, v);
		return Vector3((double)f.x, (double)f.y, (double)f.z);
	}

	// ---------------------------------------------------------
	// Helpers (CPU mirror of HLSL helpers)
	// ---------------------------------------------------------
	static void BuildSphereTangents(const DirectX::XMVECTOR& dir, DirectX::XMVECTOR& tanU, DirectX::XMVECTOR& tanV)
	{
		// Pick stable up vector
		float dirY = DirectX::XMVectorGetY(dir);
		DirectX::XMVECTOR up = (fabsf(dirY) < 0.999f) ? DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f) : DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

		tanU = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(up, dir));
		tanV = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(dir, tanU));
	}

	// Mirror of HLSL helpers
	static float SafeNormalize2X(float x, float y, float& outX, float& outY)
	{
		float len = sqrtf(x * x + y * y);
		if (len > 1e-10f)
		{
			outX = x / len;
			outY = y / len;
		}
		else
		{
			outX = 0.0f;
			outY = 0.0f;
		}
		return len;
	}

	static float PowInv(float t, float power)
	{
		return 1.0f - powf(1.0f - std::clamp(t, 0.0f, 1.0f), power);
	}

	static float SmoothStart(float t, float smoothing)
	{
		if (t >= smoothing)
			return t - 0.5f * smoothing;
		return 0.5f * t * t / std::max(smoothing, 0.0001f);
	}

	static float EaseOutSlope(float t)
	{
		float v = 1.0f - std::clamp(t, 0.0f, 1.0f);
		return 1.0f - v * v;
	}

	static float SlopeToErosionMask(float slopeTan, float slopeStart, float slopeFull, float slopeEnd, float slopeFadeOut)
	{
		auto smoothstep = [](float edge0, float edge1, float x)
			{
				float t = std::clamp((x - edge0) / std::max(edge1 - edge0, 0.00001f), 0.0f, 1.0f);
				return t * t * (3.0f - 2.0f * t);
			};

		float t = smoothstep(slopeStart, slopeFull, slopeTan);
		float fadeOut = 1.0f - smoothstep(slopeEnd, slopeFadeOut, slopeTan);
		return EaseOutSlope(t) * fadeOut;
	}

	// ---------------------------------------------------------
	// SampleWallHeightOnly — base height + wall boost combined
	// ---------------------------------------------------------
	float SampleWallHeightOnlyCPU(const Planet& planet, const DirectX::XMVECTOR& dir)
	{
		Vector3 dirVec = XMVectorToVec3(dir);
		float h = SampleHeightFromDir(planet.GetTerrainCubeData(), dirVec);

		float wallMask;
		float wallBoost = ComputeWallSteepenBoost(planet, dirVec, h, wallMask);

		return h + wallBoost;
	}

	// ---------------------------------------------------------
	// ComputeErosionFadeTarget
	// ---------------------------------------------------------
	float ComputeErosionFadeTargetCPU(const Planet& planet, const DirectX::XMVECTOR& dir, float hC)
	{
		const float planetRadius = (float)planet.GetRadius();
		const float erosionStrength = planet.GetErosionStrength();
		const float erosionStepMeters = std::max(planet.GetErosionStepMeters(), 1.0f);

		DirectX::XMVECTOR tanU, tanV;
		BuildSphereTangents(dir, tanU, tanV);  // your existing XMVECTOR helper

		const float angularStep = erosionStepMeters / std::max(planetRadius + hC, 1.0f);

		DirectX::XMVECTOR dirU0 = DirectX::XMVector3Normalize(DirectX::XMVectorAdd(dir, DirectX::XMVectorScale(tanU, angularStep)));
		DirectX::XMVECTOR dirU1 = DirectX::XMVector3Normalize(DirectX::XMVectorAdd(dir, DirectX::XMVectorScale(tanU, -angularStep)));
		DirectX::XMVECTOR dirV0 = DirectX::XMVector3Normalize(DirectX::XMVectorAdd(dir, DirectX::XMVectorScale(tanV, angularStep)));
		DirectX::XMVECTOR dirV1 = DirectX::XMVector3Normalize(DirectX::XMVectorAdd(dir, DirectX::XMVectorScale(tanV, -angularStep)));

		float hU0 = SampleWallHeightOnlyCPU(planet, dirU0);
		float hU1 = SampleWallHeightOnlyCPU(planet, dirU1);
		float hV0 = SampleWallHeightOnlyCPU(planet, dirV0);
		float hV1 = SampleWallHeightOnlyCPU(planet, dirV1);

		float hMin = std::min(std::min(hU0, hU1), std::min(hV0, hV1));
		float hMax = std::max(std::max(hU0, hU1), std::max(hV0, hV1));

		float hMid = 0.5f * (hMin + hMax);
		float hRange = std::max(0.5f * (hMax - hMin), std::max(erosionStrength * 0.6f, 1.0f));

		float result = std::clamp((hC - hMid) / hRange, -1.0f, 1.0f);

		return result;
	}

	// ---------------------------------------------------------
// ComputeWallHeightGrad2D
// ---------------------------------------------------------
	HeightGrad2DCPU ComputeWallHeightGrad2DCPU(const Planet& planet, const DirectX::XMVECTOR& dir, DirectX::XMVECTOR& outAxisA, DirectX::XMVECTOR& outAxisB)
	{
		HeightGrad2DCPU result;

		const float planetRadius = (float)planet.GetRadius();
		const float erosionStepMeters = std::max(planet.GetErosionStepMeters(), 1.0f);

		float hC = SampleWallHeightOnlyCPU(planet, dir);

		BuildErosionFrame(dir, outAxisA, outAxisB);

		// Project frame axes onto the local tangent plane
		DirectX::XMVECTOR dotAdir = DirectX::XMVector3Dot(outAxisA, dir);
		DirectX::XMVECTOR dotBdir = DirectX::XMVector3Dot(outAxisB, dir);

		DirectX::XMVECTOR tanA = DirectX::XMVectorSubtract(outAxisA, DirectX::XMVectorMultiply(dir, dotAdir));
		DirectX::XMVECTOR tanB = DirectX::XMVectorSubtract(outAxisB, DirectX::XMVectorMultiply(dir, dotBdir));

		float lenA = DirectX::XMVectorGetX(DirectX::XMVector3Length(tanA));
		float lenB = DirectX::XMVectorGetX(DirectX::XMVector3Length(tanB));

		if (lenA < 0.001f || lenB < 0.001f)
		{
			BuildSphereTangents(dir, tanA, tanB);
		}
		else
		{
			tanA = DirectX::XMVector3Normalize(tanA);
			tanB = DirectX::XMVector3Normalize(tanB);
		}

		const float angularStep = erosionStepMeters / std::max(planetRadius + hC, 1.0f);

		DirectX::XMVECTOR dirA0 = DirectX::XMVector3Normalize(DirectX::XMVectorAdd(dir, DirectX::XMVectorScale(tanA, angularStep)));
		DirectX::XMVECTOR dirA1 = DirectX::XMVector3Normalize(DirectX::XMVectorAdd(dir, DirectX::XMVectorScale(tanA, -angularStep)));
		DirectX::XMVECTOR dirB0 = DirectX::XMVector3Normalize(DirectX::XMVectorAdd(dir, DirectX::XMVectorScale(tanB, angularStep)));
		DirectX::XMVECTOR dirB1 = DirectX::XMVector3Normalize(DirectX::XMVectorAdd(dir, DirectX::XMVectorScale(tanB, -angularStep)));

		float hA0 = SampleWallHeightOnlyCPU(planet, dirA0);
		float hA1 = SampleWallHeightOnlyCPU(planet, dirA1);
		float hB0 = SampleWallHeightOnlyCPU(planet, dirB0);
		float hB1 = SampleWallHeightOnlyCPU(planet, dirB1);

		result.h = hC;
		result.gradX = (hA0 - hA1) / std::max(2.0f * erosionStepMeters, 1.0f);
		result.gradY = (hB0 - hB1) / std::max(2.0f * erosionStepMeters, 1.0f);

		return result;
	}

	float ComputeRuneStyleErosionCPU(const Planet& planet, const DirectX::XMVECTOR& dir, const DirectX::XMVECTOR& camHi, float baseHeight, float& outMask, float& outPattern)
	{
		outMask = 0.0f;
		outPattern = 0.0f;

		const float erosionEnabled = planet.GetErosionEnabled();
		const float erosionStrength = planet.GetErosionStrength();

		if (erosionEnabled <= 0.5f || erosionStrength <= 0.0001f)
			return 0.0f;

		// Distance fade — early out for orbital views
		const float planetRadius = (float)planet.GetRadius();
		const float erosionMaxDist = planet.GetErosionMaxDistance();
		const float erosionFadeStart = planet.GetErosionFadeStart();

		DirectX::XMVECTOR worldPos = DirectX::XMVectorScale(dir, planetRadius + baseHeight);
		DirectX::XMVECTOR diff = DirectX::XMVectorSubtract(worldPos, camHi);
		float distToCam = DirectX::XMVectorGetX(DirectX::XMVector3Length(diff));

		float distFade = std::clamp((erosionMaxDist - distToCam) / std::max(erosionMaxDist - erosionFadeStart, 1.0f), 0.0f, 1.0f);

		if (distFade <= 0.001f)
			return 0.0f;

		// ---------------------------------------------------------------------
		// Build local 2D position and initial wall-enhanced slope.
		// ---------------------------------------------------------------------
		DirectX::XMVECTOR axisA, axisB;
		HeightGrad2DCPU hg = ComputeWallHeightGrad2DCPU(planet, dir, axisA, axisB);

		DirectX::XMVECTOR p3 = DirectX::XMVectorScale(dir, planetRadius + baseHeight);

		float pX = DirectX::XMVectorGetX(DirectX::XMVector3Dot(p3, axisA));
		float pY = DirectX::XMVectorGetX(DirectX::XMVector3Dot(p3, axisB));

		float gradX = hg.gradX;
		float gradY = hg.gradY;

		float slopeLength = std::max(sqrtf(gradX * gradX + gradY * gradY), 1e-10f);

		// ---------------------------------------------------------------------
		// Rune-style controls (hardcoded — match GPU values)
		// ---------------------------------------------------------------------
		const float ridgeRounding = 0.05f;
		const float creaseRounding = 0.00f;
		const float roundingInputScale = 0.05f;
		const float roundingOctaveMultiplier = planet.GetErosionLacunarity();

		const float onsetInitial = 1.25f;
		const float onsetOctave = 1.25f;

		// Slope settings from cbuffer
		const float slopeStart = planet.GetErosionSlopeStart();
		const float slopeFull = planet.GetErosionSlopeFull();
		const float slopeEnd = planet.GetErosionSlopeEnd();
		const float slopeFadeOut = planet.GetErosionSlopeFadeOut();

		float terrainSlopeMask = SlopeToErosionMask(slopeLength, slopeStart, slopeFull, slopeEnd, slopeFadeOut);
		outMask = terrainSlopeMask;

		if (terrainSlopeMask <= 0.001f)
		{
			outPattern = 0.5f;
			return 0.0f;
		}

		float fadeTarget = ComputeErosionFadeTargetCPU(planet, dir, hg.h);

		// gullySlope = lerp(grad, normalize(grad) * assumedSlope, assumedSlopeBlend)
		const float assumedSlope = planet.GetErosionAssumedSlope();
		const float assumedBlend = planet.GetErosionAssumedSlopeBlend();

		float gradNormX, gradNormY;
		SafeNormalize2X(gradX, gradY, gradNormX, gradNormY);

		float gullySlopeX = gradX + assumedBlend * (gradNormX * assumedSlope - gradX);
		float gullySlopeY = gradY + assumedBlend * (gradNormY * assumedSlope - gradY);

		// ---------------------------------------------------------------------
		// Octave loop
		// ---------------------------------------------------------------------
		const float erosionTiling = planet.GetErosionTilingMeters();
		const float erosionCellScale = planet.GetErosionCellScale();
		const float erosionNormalization = planet.GetErosionNormalization();
		const float erosionGullyWeight = planet.GetErosionGullyWeight();
		const float erosionDetail = planet.GetErosionDetail();
		const float erosionLacunarity = planet.GetErosionLacunarity();
		const float erosionPersistence = planet.GetErosionPersistence();
		const int erosionOctavesRuntime = planet.GetErosionOctaves();

		float totalHeightDelta = 0.0f;
		float freq = 1.0f / std::max(erosionTiling * erosionCellScale, 1.0f);
		float octaveStrength = erosionStrength;
		float roundingMult = 1.0f;

		// Initial combi mask, Rune-style
		float roundingForInput = (creaseRounding + std::clamp(fadeTarget + 0.5f, 0.0f, 1.0f) * (ridgeRounding - creaseRounding)) * roundingInputScale;

		float combiMask = EaseOutSlope(SmoothStart(slopeLength * onsetInitial, roundingForInput * onsetInitial));

		combiMask *= terrainSlopeMask;

		// Compile-time octave count cap, runtime check inside
		for (int octave = 0; octave < planet.GetErosionOctaves(); octave++)
		{
			// Equivalent to HLSL's step((float)octave + 0.5, (float)ErosionOctaves)
			float active = (octave < erosionOctavesRuntime) ? 1.0f : 0.0f;

			// Normalize gullySlope for stripe direction
			float gullyDirX, gullyDirY;
			SafeNormalize2X(gullySlopeX, gullySlopeY, gullyDirX, gullyDirY);

			// Phacelle Noise
			NoiseVec2 phP(pX * freq, pY * freq);
			NoiseVec2 phNorm(gullyDirX, gullyDirY);

			PhacelleSampleCPU phacelle = PhacelleNoise(phP, phNorm, erosionCellScale, 0.25f, erosionNormalization);

			// phacelle.sideDir already includes cellScale * TAU
			// Scale derivative back since we passed p * freq
			float phacelleDerivX = phacelle.sideDir.x * -freq;
			float phacelleDerivY = phacelle.sideDir.y * -freq;

			float sloping = std::abs(phacelle.s);

			// Update gullySlope (non-masked feedback for next octave)
			float signS = (phacelle.s >= 0.0f) ? 1.0f : -1.0f;
			gullySlopeX += signS * phacelleDerivX * octaveStrength * erosionGullyWeight * active;
			gullySlopeY += signS * phacelleDerivY * octaveStrength * erosionGullyWeight * active;

			// Gullies: height offset + slope deriv
			float gulliesH = phacelle.c;
			float gulliesDX = phacelle.s * phacelleDerivX;
			float gulliesDY = phacelle.s * phacelleDerivY;

			// Faded gullies = lerp(fadeTargetVec, gullies * gullyWeight, combiMask)
			float fadedH = fadeTarget + combiMask * (gulliesH * erosionGullyWeight - fadeTarget);
			float fadedDX = combiMask * (gulliesDX * erosionGullyWeight);
			float fadedDY = combiMask * (gulliesDY * erosionGullyWeight);

			totalHeightDelta += fadedH * octaveStrength * active;
			gradX += fadedDX * octaveStrength * active;
			gradY += fadedDY * octaveStrength * active;

			fadeTarget = fadedH;

			float roundingForOctave = (creaseRounding + std::clamp(phacelle.c + 0.5f, 0.0f, 1.0f) * (ridgeRounding - creaseRounding)) * roundingMult;

			float newMask = EaseOutSlope(SmoothStart(sloping * onsetOctave, roundingForOctave * onsetOctave));

			combiMask = PowInv(combiMask, erosionDetail) * newMask * active;

			octaveStrength *= std::clamp(erosionPersistence, 0.0f, 1.0f);
			freq *= std::max(erosionLacunarity, 1.001f);
			roundingMult *= roundingOctaveMultiplier;
		}

		DirectX::XMFLOAT3 dirF;
		DirectX::XMStoreFloat3(&dirF, dir);

		outPattern = totalHeightDelta / std::max(erosionStrength, 1.0f);

		// Apply terrain slope mask once at end
		return totalHeightDelta * terrainSlopeMask;
	}

	static float SampleHeightF(const CubeData<float>& td, const DirectX::XMVECTOR& dir)
	{
		// Convert XMVECTOR to your Vector3 for the existing sampler
		DirectX::XMFLOAT3 d;
		DirectX::XMStoreFloat3(&d, dir);
		return SampleHeightFromDir(td, Vector3((double)d.x, (double)d.y, (double)d.z));
	}

	float ComputeBaseSlope(const Planet& planet, const Vector3& dirPlanet)
	{

		// Build tangent frame on the unit sphere
		DirectX::XMVECTOR dir = DirectX::XMVectorSet((float)dirPlanet.x, (float)dirPlanet.y, (float)dirPlanet.z, 0.0f);
		dir = DirectX::XMVector3Normalize(dir);

		DirectX::XMVECTOR tanU, tanV;
		BuildSphereTangents(dir, tanU, tanV);

		// Angular step: one texel of the cubemap face (matching GPU)
		const auto& terrainCube = planet.GetTerrainCubeData();
		const float width = (float)terrainCube.Width;
		const float angularStep = (2.0f / width) * (3.14159265f * 0.5f);

		DirectX::XMVECTOR dirU = DirectX::XMVector3Normalize(DirectX::XMVectorAdd(dir, DirectX::XMVectorScale(tanU, angularStep)));
		DirectX::XMVECTOR dirV = DirectX::XMVector3Normalize(DirectX::XMVectorAdd(dir, DirectX::XMVectorScale(tanV, angularStep)));

		// Convert XMVECTOR back to Vector3 for sampling
		DirectX::XMFLOAT3 dirF, dirUF, dirVF;
		DirectX::XMStoreFloat3(&dirF, dir);
		DirectX::XMStoreFloat3(&dirUF, dirU);
		DirectX::XMStoreFloat3(&dirVF, dirV);

		Vector3 dirVec((double)dirF.x, (double)dirF.y, (double)dirF.z);
		Vector3 dirUVec((double)dirUF.x, (double)dirUF.y, (double)dirUF.z);
		Vector3 dirVVec((double)dirVF.x, (double)dirVF.y, (double)dirVF.z);

		float hC = SampleHeightFromDir(terrainCube, dirVec);
		float hU = SampleHeightFromDir(terrainCube, dirUVec);
		float hV = SampleHeightFromDir(terrainCube, dirVVec);

		const float planetRadius = (float)planet.GetRadius();

		// Displaced positions on the sphere surface
		DirectX::XMVECTOR pC = DirectX::XMVectorScale(dir, planetRadius + hC);
		DirectX::XMVECTOR pU = DirectX::XMVectorScale(dirU, planetRadius + hU);
		DirectX::XMVECTOR pV = DirectX::XMVectorScale(dirV, planetRadius + hV);

		// Compute normal via cross product
		DirectX::XMVECTOR du = DirectX::XMVectorSubtract(pU, pC);
		DirectX::XMVECTOR dv = DirectX::XMVectorSubtract(pV, pC);
		DirectX::XMVECTOR N = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(du, dv));

		// Ensure outward-facing
		DirectX::XMVECTOR dotND = DirectX::XMVector3Dot(N, dir);
		if (DirectX::XMVectorGetX(dotND) < 0.0f)
			N = DirectX::XMVectorNegate(N);

		// slope = 1 - saturate(dot(normalize(N), dir))
		DirectX::XMVECTOR dotNdir = DirectX::XMVector3Dot(N, dir);
		float slope = 1.0f - std::clamp(DirectX::XMVectorGetX(dotNdir), 0.0f, 1.0f);

		return slope;
	}

	// ---------------------------------------------------------
	// Wall steepening — direct port of HLSL ComputeWallSteepenBoost
	// ---------------------------------------------------------
	float ComputeWallSteepenBoost(const Planet& planet, const Vector3& dirIn, float hC, float& outMask)
	{
		outMask = 0.0f;

		const float wallEnabled = planet.GetWallEnhancementEnabled();
		const float wallStrength = planet.GetWallStrength();

		if (wallEnabled <= 0.5f || wallStrength <= 0.0001f)
			return 0.0f;

		const float planetRadius = (float)planet.GetRadius();
		const float wallStepMeters = std::max(planet.GetWallStepMeters(), 1.0f);
		const float wallSlopeStart = planet.GetWallSlopeStart();
		const float wallSlopeEnd = planet.GetWallSlopeEnd();
		const float wallSharpStart = planet.GetWallSharpStart();
		const float wallSharpEnd = planet.GetWallSharpEnd();
		const float wallMaxDelta = planet.GetWallMaxDelta();

		DirectX::XMVECTOR dir = DirectX::XMVectorSet((float)dirIn.x, (float)dirIn.y, (float)dirIn.z, 0.0f);
		dir = DirectX::XMVector3Normalize(dir);

		DirectX::XMVECTOR tanU, tanV;
		BuildSphereTangents(dir, tanU, tanV);

		const float angularStep = wallStepMeters / std::max(planetRadius + hC, 1.0f);

		DirectX::XMVECTOR dirU0 = DirectX::XMVector3Normalize(DirectX::XMVectorAdd(dir, DirectX::XMVectorScale(tanU, angularStep)));
		DirectX::XMVECTOR dirU1 = DirectX::XMVector3Normalize(DirectX::XMVectorAdd(dir, DirectX::XMVectorScale(tanU, -angularStep)));
		DirectX::XMVECTOR dirV0 = DirectX::XMVector3Normalize(DirectX::XMVectorAdd(dir, DirectX::XMVectorScale(tanV, angularStep)));
		DirectX::XMVECTOR dirV1 = DirectX::XMVector3Normalize(DirectX::XMVectorAdd(dir, DirectX::XMVectorScale(tanV, -angularStep)));

		const auto& heightCube = planet.GetTerrainCubeData();
		float hU0 = SampleHeightF(heightCube, dirU0);
		float hU1 = SampleHeightF(heightCube, dirU1);
		float hV0 = SampleHeightF(heightCube, dirV0);
		float hV1 = SampleHeightF(heightCube, dirV1);

		const float dhdu = (hU0 - hU1) / std::max(2.0f * wallStepMeters, 1.0f);
		const float dhdv = (hV0 - hV1) / std::max(2.0f * wallStepMeters, 1.0f);

		const float slopeTan = sqrtf(dhdu * dhdu + dhdv * dhdv);

		// smoothstep
		auto smoothstep = [](float edge0, float edge1, float x) -> float
			{
				float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
				return t * t * (3.0f - 2.0f * t);
			};

		const float slopeMask = smoothstep(wallSlopeStart, wallSlopeEnd, slopeTan);
		outMask = slopeMask;

		if (slopeMask <= 0.001f || slopeTan <= 0.0001f)
			return 0.0f;

		const float gradDirU = dhdu / slopeTan;
		const float gradDirV = dhdv / slopeTan;

		DirectX::XMVECTOR tangentGradDir = DirectX::XMVector3Normalize(DirectX::XMVectorAdd(DirectX::XMVectorScale(tanU, gradDirU), DirectX::XMVectorScale(tanV, gradDirV)));

		DirectX::XMVECTOR dirHigh = DirectX::XMVector3Normalize(DirectX::XMVectorAdd(dir, DirectX::XMVectorScale(tangentGradDir, angularStep)));
		DirectX::XMVECTOR dirLow = DirectX::XMVector3Normalize(DirectX::XMVectorAdd(dir, DirectX::XMVectorScale(tangentGradDir, -angularStep)));

		float hHigh = SampleHeightF(heightCube, dirHigh);
		float hLow = SampleHeightF(heightCube, dirLow);

		const float hMin = std::min(hHigh, hLow);
		const float hMax = std::max(hHigh, hLow);

		const float hMid = 0.5f * (hMin + hMax);
		const float halfRange = std::max(0.5f * (hMax - hMin), 1.0f);

		// 0..1 position through the local height transition
		float x = std::clamp((hC - hMid) / halfRange * 0.5f + 0.5f, 0.0f, 1.0f);

		const float sharpStart = std::min(wallSharpStart, wallSharpEnd - 0.001f);
		const float sharpEnd = std::max(wallSharpEnd, sharpStart + 0.001f);

		const float xSharp = smoothstep(sharpStart, sharpEnd, x);

		// lerp(hMin, hMax, xSharp)
		const float hSharp = hMin + (hMax - hMin) * xSharp;

		const float boost = (hSharp - hC) * wallStrength * slopeMask;

		return std::clamp(boost, -wallMaxDelta, wallMaxDelta);
	}

	float ComputeMaterialNoiseCPU(const Planet& planet, const Vector3& dirPlanet, float slope, float colorAvg, const DirectX::XMVECTOR& cameraPlanetSpace)
	{
		const auto& materials = planet.GetTerrainMaterials();  // need this getter
		if (materials.empty())
			return 0.0f;

		const float planetRadius = (float)planet.GetRadius();

		// worldPos in planet space — same as GPU: dir * planetRadius
		DirectX::XMVECTOR dir = DirectX::XMVectorSet((float)dirPlanet.x, (float)dirPlanet.y, (float)dirPlanet.z, 0.0f);
		dir = DirectX::XMVector3Normalize(dir);
		DirectX::XMVECTOR worldPos = DirectX::XMVectorScale(dir, planetRadius);

		DirectX::XMFLOAT3 worldPosF;
		DirectX::XMStoreFloat3(&worldPosF, worldPos);
		DirectX::XMFLOAT3 dirF;
		DirectX::XMStoreFloat3(&dirF, dir);

		// Compute material weights
		float weights[MAX_MATERIALS_CPU];
		ComputeAllMaterialWeightsCPU(materials, slope, colorAvg, weights);

		// Distance to camera (for LOD activation distance fade)
		DirectX::XMVECTOR diff = DirectX::XMVectorSubtract(worldPos, cameraPlanetSpace);
		float vertexDist = DirectX::XMVectorGetX(DirectX::XMVector3Length(diff));

		float totalNoise = 0.0f;
		int matCount = std::min((int)materials.size(), MAX_MATERIALS_CPU);

		for (int m = 0; m < matCount; ++m)
		{
			if (weights[m] < 0.001f)
				continue;

			const PlanetMaterial& mat = materials[m];
			float matNoise = 0.0f;

			for (const NoiseLayer& layer : mat.NoiseLayers)
			{
				// LOD activation fade
				float activationDist = planetRadius / (float)(1 << layer.GPU.LODActivation);
				float fadeFactor = std::clamp(
					1.0f - (vertexDist - activationDist * 0.8f) / (activationDist * 0.2f),
					0.0f, 1.0f);

				// Apply radial frequency scale (same as GPU)
				float noisePosX = worldPosF.x;
				float noisePosY = worldPosF.y;
				float noisePosZ = worldPosF.z;

				if (std::abs(layer.GPU.RadialFreqScale - 1.0f) > 0.001f)
				{
					float radialComponent = worldPosF.x * dirF.x + worldPosF.y * dirF.y + worldPosF.z * dirF.z;
					float scale = 1.0f - layer.GPU.RadialFreqScale;
					noisePosX -= dirF.x * radialComponent * scale;
					noisePosY -= dirF.y * radialComponent * scale;
					noisePosZ -= dirF.z * radialComponent * scale;
				}

				// Evaluate noise based on type
				float h = 0.0f;
				switch (layer.GPU.Type)
				{
				case 0: // Fractal
					h = FractalPerlin3D(layer.Perm, noisePosX, noisePosY, noisePosZ, layer.GPU.Octaves, layer.GPU.Frequency, layer.GPU.Amplitude, layer.GPU.Lacunarity, layer.GPU.Persistence);
					break;
				case 1: // Ridged
					h = RidgedPerlin3D(layer.Perm, noisePosX, noisePosY, noisePosZ, layer.GPU.Octaves, layer.GPU.Frequency, layer.GPU.Amplitude, layer.GPU.Lacunarity, layer.GPU.Persistence, layer.GPU.RidgeSharpness);
					break;
				case 2: // Turbulence
					h = TurbulencePerlin3D(layer.Perm, noisePosX, noisePosY, noisePosZ, layer.GPU.Octaves, layer.GPU.Frequency, layer.GPU.Amplitude, layer.GPU.Lacunarity, layer.GPU.Persistence);
					break;
				case 3: // Voronoi
					h = Voronoi3D(layer.Perm, noisePosX, noisePosY, noisePosZ, layer.GPU.Octaves, layer.GPU.Frequency, layer.GPU.Amplitude, layer.GPU.Lacunarity, layer.GPU.Persistence);
					break;
				}

				matNoise += h * layer.GPU.BlendWeight * fadeFactor;
			}

			totalNoise += matNoise * weights[m];
		}

		return totalNoise;
	}

}