#pragma once

#include "Toast/Core/Base.h"

#include <DirectXMath.h>

#define PLANET_BAKE_THREADGROUP_SIZE 64

namespace Toast {

	struct PlanetBakedVertexGPU
	{
		DirectX::XMFLOAT3 NormalPS;
		float Height;
		uint32_t PackedDebug[3]; // 5 half floats: (wall, wallMask), (erosionMask, erosionPattern), (erosionDelta, unused)
		uint32_t Pad;
	};
	TOAST_STATIC_ASSERT(sizeof(PlanetBakedVertexGPU) == 32, "PlanetBakedVertexGPU must match the HLSL PlanetBakedVertex (16 bytes)")

}