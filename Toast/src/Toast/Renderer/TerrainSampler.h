#pragma once

#include "Toast/Core/Math/Vector.h"
#include "Toast/Renderer/TerrainCubeData.h"

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
	float SampleCubeBilinear(const TerrainCubeData& td, const Vector3& dirIn);
	float SampleHeightFromDir(const TerrainCubeData& td, const Vector3& dirPlanet);

	float SampleHeightNearest(const TerrainCubeData& td, const Vector3& dirIn);

} // namespace Toast