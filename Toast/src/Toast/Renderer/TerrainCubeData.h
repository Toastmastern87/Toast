#pragma once

#include <array>
#include <vector>

namespace Toast {

	inline size_t Index2D(uint32_t x, uint32_t y, uint32_t width)
	{
		return static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
	}

	struct TerrainCubeData
	{
		uint32_t Width = 0;
		uint32_t Height = 0;

		std::array<std::vector<float>, 6> FaceHeight;
	};
}