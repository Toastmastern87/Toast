#pragma once

#include <DirectXMath.h>

namespace Toast {

	class Math
	{
	public:
		static float GetVectorLength(const DirectX::XMFLOAT3& vector) 
		{
			return sqrt(pow(vector.x, 2.0f) + pow(vector.y, 2.0f) + pow(vector.z, 2.0f));
		}
	};
}