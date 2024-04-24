#pragma once

#include "Toast/Core/Math/Quaternion.h"
#include "Toast/Core/Math/Vector.h"

namespace Toast {

	class Matrix
	{
	public:
		Matrix() = default;
		Matrix(DirectX::XMMATRIX matrix);
		Matrix(double m00, double m01, double m02, double m03, double m10, double m11, double m12, double m13, double m20, double m21, double m22, double m23, double m30, double m31, double m32, double m33);

		double Determinant() const;

		static Matrix Identity();
		static Matrix Zero();

		Matrix Transpose() const;

		Matrix Adjugate() const;

		static Matrix Inverse(const Matrix& mat);

		static Matrix TranslationFromVector(const Vector3& translationVector);

		static Matrix ScalingFromVector(const Vector3& scaleVector);

		static Matrix FromQuaternion(const Quaternion& q);

		void ToString();

		double& element(int i, int j) {
			return *(&m_00 + i * 4 + j);
		}

		const double& element(int i, int j) const {
			return *(&m_00 + i * 4 + j);
		}

		// Row major!
		Vector3 operator*(const Vector3& vec)
		{
			double xResult, yResult, zResult, wResult;

			if (vec.w == 0.0)
			{
				xResult = vec.x * m_00 + vec.y * m_01 + vec.z * m_02 + m_03;
				yResult = vec.x * m_10 + vec.y * m_11 + vec.z * m_12 + m_13;
				zResult = vec.x * m_20 + vec.y * m_21 + vec.z * m_22 + m_23;
				wResult = 0.0;
			}
			else
			{
				// For column-major multiplication, the vector appears on the left.
				xResult = vec.x * m_00 + vec.y * m_10 + vec.z * m_20 + vec.w * m_30;
				yResult = vec.x * m_01 + vec.y * m_11 + vec.z * m_21 + vec.w * m_31;
				zResult = vec.x * m_02 + vec.y * m_12 + vec.z * m_22 + vec.w * m_32;
				wResult = vec.x * m_03 + vec.y * m_13 + vec.z * m_23 + vec.w * m_33;

				//// Perform the homogeneous divide (divide by the w component)
				//if (wResult != 0.0) {
				//	xResult /= wResult;
				//	yResult /= wResult;
				//	zResult /= wResult;
				//}
			}

			return Vector3(xResult, yResult, zResult, wResult);
		}

		Matrix operator*(const Matrix& other) const {
			Matrix result;

			result.m_00 = m_00 * other.m_00 + m_01 * other.m_10 + m_02 * other.m_20 + m_03 * other.m_30;
			result.m_01 = m_00 * other.m_01 + m_01 * other.m_11 + m_02 * other.m_21 + m_03 * other.m_31;
			result.m_02 = m_00 * other.m_02 + m_01 * other.m_12 + m_02 * other.m_22 + m_03 * other.m_32;
			result.m_03 = m_00 * other.m_03 + m_01 * other.m_13 + m_02 * other.m_23 + m_03 * other.m_33;

			result.m_10 = m_10 * other.m_00 + m_11 * other.m_10 + m_12 * other.m_20 + m_13 * other.m_30;
			result.m_11 = m_10 * other.m_01 + m_11 * other.m_11 + m_12 * other.m_21 + m_13 * other.m_31;
			result.m_12 = m_10 * other.m_02 + m_11 * other.m_12 + m_12 * other.m_22 + m_13 * other.m_32;
			result.m_13 = m_10 * other.m_03 + m_11 * other.m_13 + m_12 * other.m_23 + m_13 * other.m_33;

			result.m_20 = m_20 * other.m_00 + m_21 * other.m_10 + m_22 * other.m_20 + m_23 * other.m_30;
			result.m_21 = m_20 * other.m_01 + m_21 * other.m_11 + m_22 * other.m_21 + m_23 * other.m_31;
			result.m_22 = m_20 * other.m_02 + m_21 * other.m_12 + m_22 * other.m_22 + m_23 * other.m_32;
			result.m_23 = m_20 * other.m_03 + m_21 * other.m_13 + m_22 * other.m_23 + m_23 * other.m_33;

			result.m_30 = m_30 * other.m_00 + m_31 * other.m_10 + m_32 * other.m_20 + m_33 * other.m_30;
			result.m_31 = m_30 * other.m_01 + m_31 * other.m_11 + m_32 * other.m_21 + m_33 * other.m_31;
			result.m_32 = m_30 * other.m_02 + m_31 * other.m_12 + m_32 * other.m_22 + m_33 * other.m_32;
			result.m_33 = m_30 * other.m_03 + m_31 * other.m_13 + m_32 * other.m_23 + m_33 * other.m_33;

			return result;
		}

		Matrix operator*(double scalar) const {
			Matrix result;

			result.m_00 = m_00 * scalar;
			result.m_01 = m_01 * scalar;
			result.m_02 = m_02 * scalar;
			result.m_03 = m_03 * scalar;

			result.m_10 = m_10 * scalar;
			result.m_11 = m_11 * scalar;
			result.m_12 = m_12 * scalar;
			result.m_13 = m_13 * scalar;

			result.m_20 = m_20 * scalar;
			result.m_21 = m_21 * scalar;
			result.m_22 = m_22 * scalar;
			result.m_23 = m_23 * scalar;

			result.m_30 = m_30 * scalar;
			result.m_31 = m_31 * scalar;
			result.m_32 = m_32 * scalar;
			result.m_33 = m_33 * scalar;

			return result;
		}

	public:


		double m_00, m_01, m_02, m_03;
		double m_10, m_11, m_12, m_13;
		double m_20, m_21, m_22, m_23;
		double m_30, m_31, m_32, m_33;
	};

}