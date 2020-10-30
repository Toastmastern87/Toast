#pragma once

#include <DirectXMath.h>

#define M_PI 3.14159265358979323846264338327950288f

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