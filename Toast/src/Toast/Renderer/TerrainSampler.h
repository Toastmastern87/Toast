#pragma once

#include "Toast/Core/Math/Vector.h"
#include "Toast/Renderer/PlanetSystem.h"

namespace Toast {

	struct CubeSampleCPU
	{
		uint32_t face;
		double u;
		double v;
	};

	CubeSampleCPU DirectionToCube(const Vector3& vIn);
	Vector3 CubeFaceUVToDir(uint32_t face, double u, double v);
	CubeSampleCPU RemapFaceUV(uint32_t face, double u, double v);

	// Templated samplers — work with any CubeData<T> type
	template<typename T>
	T SampleCubeNearest(const CubeData<T>& td, const Vector3& dirIn);

	// Heightmap-specific (bilinear interpolation)
	float SampleCubeBilinear(const CubeData<float>& td, const Vector3& dirIn);
	float SampleHeightFromDir(const CubeData<float>& td, const Vector3& dirPlanet);
	float SampleHeightNearest(const CubeData<float>& td, const Vector3& dirIn);

	// Albedo-specific (point sample, SRGB→linear conversion, returns colorAvg)
	float SampleColorAvgFromDir(const CubeData<uint32_t>& td, const Vector3& dirPlanet);

} // namespace Toast