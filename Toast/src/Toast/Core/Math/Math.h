#pragma once

#include "Toast/Core/Math/Vector.h"
#include "Toast/Core/Math/Quaternion.h"
#include "Toast/Core/Math/Matrix.h"

#include <DirectXMath.h>

namespace Toast {

	class Math
	{
	public:
		static double DegreesToRadians(double degrees) {
			return degrees * (3.14159265358979323846 / 180.0);
		}

		static float GetVectorLength(const DirectX::XMFLOAT3& vector) 
		{
			return sqrt(pow(vector.x, 2.0f) + pow(vector.y, 2.0f) + pow(vector.z, 2.0f));
		}

		static double BilinearInterpolation(DirectX::XMFLOAT2 P, double Q11, double Q12, double Q21, double Q22)
		{
			// Calculate the fractional parts
			double fracX = P.x - floor(P.x);
			double fracY = P.y - floor(P.y);

			// Interpolate the Q values
			double R1 = Q11 * (1.0 - fracX) + Q21 * fracX;
			double R2 = Q12 * (1.0 - fracX) + Q22 * fracX;
			double result = R1 * (1.0 - fracY) + R2 * fracY;
			return result;
		}
	};
}