#pragma once

#include <DirectXMath.h>

namespace Toast {

	// Maximum of different terrain objects the planet system can scatter
	#define MAX_TERRAIN_OBJECT_TYPES		8

	// Maximum number of surviving instances per type and per frame
	#define MAX_TERRAIN_INSTANCES_PER_TYPE		16384

	// Threads per group in the computer shader
	#define TERRAIN_SCATTER_THREADGROUP_SIZE	64

	#define TERRAIN_ARGS_STRIDE					32
	#define TERRAIN_ARGS_INSTANCE_COUNT_OFF		4

	struct TerrainInstanceGPU
	{
		DirectX::XMFLOAT3 PositionCR;
		float Scale;
		DirectX::XMFLOAT3 RotationQuat;
	};

}