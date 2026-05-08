#pragma once
#include "Toast/Core/Math/Vector.h"
#include "Toast/Renderer/PlanetSystem.h"

namespace Toast {

	struct HeightGrad2DCPU
	{
		float h;
		float gradX;
		float gradY;
	};

	float ComputeBaseSlope(const Planet& planet, const Vector3& dirPlanet);

	// Wall steepening — mirrors GPU ComputeWallSteepenBoost.
	// Returns the height boost (in meters) to add to the base heightmap.
	// outMask receives the slope-mask used (0..1).
	float ComputeWallSteepenBoost(const Planet& planet, const Vector3& dir, float hC, float& outMask);

	// Wall-enhanced height (base height + wall steepening)
	float SampleWallHeightOnlyCPU(const Planet& planet, const DirectX::XMVECTOR& dir);

	// Computes -1 to +1 fade target based on local terrain extremes
	float ComputeErosionFadeTargetCPU(const Planet& planet, const DirectX::XMVECTOR& dir, float hC);

	// Wall-enhanced height + 2D gradient on the local erosion frame
	HeightGrad2DCPU ComputeWallHeightGrad2DCPU(const Planet& planet, const DirectX::XMVECTOR& dir, DirectX::XMVECTOR& outAxisA, DirectX::XMVECTOR& outAxisB);

	// Erosion main function — mirrors GPU ComputeRuneStyleErosion.
	// Returns the height delta to add (in meters).
	// outMask receives the slope mask used (0..1).
	// outPattern receives the normalized pattern value for debug.
	float ComputeRuneStyleErosionCPU(const Planet& planet, const DirectX::XMVECTOR& dir, const DirectX::XMVECTOR& camHi, float baseHeight, float& outMask, float& outPattern);

	float ComputeMaterialNoiseCPU(const Planet& planet, const Vector3& dirPlanet, float slope, float colorAvg, const DirectX::XMVECTOR& cameraPlanetSpace);

}