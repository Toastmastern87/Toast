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

		static float BilinearInterpolation(DirectX::XMFLOAT2 P, float Q11, float Q12, float Q21, float Q22) 
		{
			float x1 = floor(P.x);
			float x2 = floor(P.x + 1.0f);
			float y1 = floor(P.y);
			float y2 = floor(P.y + 1.0f);

			//R1(x, y) = Q11 ·(x2 – x) / (x2 – x1) + Q21 ·(x – x1) / (x2 – x1)
			float R1 = Q11 * (x2 - P.x) / (x2 - x1) + Q21 * (P.x - x1) / (x2 - x1);

			//R2(x, y) = Q12 · (x2 – x) / (x2 – x1) + Q22 · (x – x1) / (x2 – x1)
			float R2 = Q12 * (x2 - P.x) / (x2 - x1) + Q22 * (P.x - x1) / (x2 - x1);

			//P(x, y) = R1 ·(y2 – y) / (y2 – y1) + R2 ·(y – y1) / (y2 – y1)
			return R1 * (y2 - P.y) / (y2 - y1) + R2 * (P.y - y1) / (y2 - y1);
		}
	};
}