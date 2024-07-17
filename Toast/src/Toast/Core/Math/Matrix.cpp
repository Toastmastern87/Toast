#include "tpch.h"

#include "Matrix.h"

namespace Toast {

	Matrix::Matrix(DirectX::XMMATRIX matrix) {
		DirectX::XMVECTOR row = matrix.r[0];
		DirectX::XMFLOAT4 elements;

		DirectX::XMStoreFloat4(&elements, row);
		m_00 = (double)elements.x;
		m_01 = (double)elements.y;
		m_02 = (double)elements.z;
		m_03 = (double)elements.w;

		row = matrix.r[1];
		DirectX::XMStoreFloat4(&elements, row);
		m_10 = (double)elements.x;
		m_11 = (double)elements.y;
		m_12 = (double)elements.z;
		m_13 = (double)elements.w;

		row = matrix.r[2];
		DirectX::XMStoreFloat4(&elements, row);
		m_20 = (double)elements.x;
		m_21 = (double)elements.y;
		m_22 = (double)elements.z;
		m_23 = (double)elements.w;

		row = matrix.r[3];
		DirectX::XMStoreFloat4(&elements, row);
		m_30 = (double)elements.x;
		m_31 = (double)elements.y;
		m_32 = (double)elements.z;
		m_33 = (double)elements.w;
	}

	Matrix::Matrix(double m00, double m01, double m02, double m03, double m10, double m11, double m12, double m13, double m20, double m21, double m22, double m23, double m30, double m31, double m32, double m33) {
		m_00 = m00;
		m_01 = m01;
		m_02 = m02;
		m_03 = m03;
		m_10 = m10;
		m_11 = m11;
		m_12 = m12;
		m_13 = m13;
		m_20 = m20;
		m_21 = m21;
		m_22 = m22;
		m_23 = m23;
		m_30 = m30;
		m_31 = m31;
		m_32 = m32;
		m_33 = m33;
	}

	Matrix Matrix::Identity()
	{
		Matrix identityMatrix;

		identityMatrix.m_00 = 1.0; identityMatrix.m_01 = 0.0; identityMatrix.m_02 = 0.0; identityMatrix.m_03 = 0.0;
		identityMatrix.m_10 = 0.0; identityMatrix.m_11 = 1.0; identityMatrix.m_12 = 0.0; identityMatrix.m_13 = 0.0;
		identityMatrix.m_20 = 0.0; identityMatrix.m_21 = 0.0; identityMatrix.m_22 = 1.0; identityMatrix.m_23 = 0.0;
		identityMatrix.m_30 = 0.0; identityMatrix.m_31 = 0.0; identityMatrix.m_32 = 0.0; identityMatrix.m_33 = 1.0;

		return identityMatrix;
	}

	Matrix Matrix::Zero()
	{
		Matrix zeroMatrix;

		zeroMatrix.m_00 = 0.0; zeroMatrix.m_01 = 0.0; zeroMatrix.m_02 = 0.0; zeroMatrix.m_03 = 0.0;
		zeroMatrix.m_10 = 0.0; zeroMatrix.m_11 = 0.0; zeroMatrix.m_12 = 0.0; zeroMatrix.m_13 = 0.0;
		zeroMatrix.m_20 = 0.0; zeroMatrix.m_21 = 0.0; zeroMatrix.m_22 = 0.0; zeroMatrix.m_23 = 0.0;
		zeroMatrix.m_30 = 0.0; zeroMatrix.m_31 = 0.0; zeroMatrix.m_32 = 0.0; zeroMatrix.m_33 = 0.0;

		return zeroMatrix;
	}

	Matrix Matrix::Transpose() const {
		return Matrix(m_00, m_10, m_20, m_30,
			m_01, m_11, m_21, m_31,
			m_02, m_12, m_22, m_32,
			m_03, m_13, m_23, m_33);
	}

	double Matrix::Determinant() const
	{
		// Helper function to calculate the determinant of a 3x3 matrix
		auto det3x3 = [](double a, double b, double c, double d, double e, double f, double g, double h, double i) -> double
		{
			return a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
		};

		double detA1 = det3x3(m_11, m_12, m_13, m_21, m_22, m_23, m_31, m_32, m_33);
		double detA2 = det3x3(m_10, m_12, m_13, m_20, m_22, m_23, m_30, m_32, m_33);
		double detA3 = det3x3(m_10, m_11, m_13, m_20, m_21, m_23, m_30, m_31, m_33);
		double detA4 = det3x3(m_10, m_11, m_12, m_20, m_21, m_22, m_30, m_31, m_32);

		return m_00 * detA1 - m_01 * detA2 + m_02 * detA3 - m_03 * detA4;
	}

	Matrix Matrix::Adjugate() const
	{
		// Helper function to calculate the determinant of a 3x3 matrix
		auto det3x3 = [](double a, double b, double c, double d, double e, double f, double g, double h, double i) -> double
		{
			return a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
		};

		Matrix adj;

		adj.m_00 = det3x3(m_11, m_12, m_13, m_21, m_22, m_23, m_31, m_32, m_33);
		adj.m_01 = -det3x3(m_10, m_12, m_13, m_20, m_22, m_23, m_30, m_32, m_33);
		adj.m_02 = det3x3(m_10, m_11, m_13, m_20, m_21, m_23, m_30, m_31, m_33);
		adj.m_03 = -det3x3(m_10, m_11, m_12, m_20, m_21, m_22, m_30, m_31, m_32);

		adj.m_10 = -det3x3(m_01, m_02, m_03, m_21, m_22, m_23, m_31, m_32, m_33);
		adj.m_11 = det3x3(m_00, m_02, m_03, m_20, m_22, m_23, m_30, m_32, m_33);
		adj.m_12 = -det3x3(m_00, m_01, m_03, m_20, m_21, m_23, m_30, m_31, m_33);
		adj.m_13 = det3x3(m_00, m_01, m_02, m_20, m_21, m_22, m_30, m_31, m_32);

		adj.m_20 = det3x3(m_01, m_02, m_03, m_11, m_12, m_13, m_31, m_32, m_33);
		adj.m_21 = -det3x3(m_00, m_02, m_03, m_10, m_12, m_13, m_30, m_32, m_33);
		adj.m_22 = det3x3(m_00, m_01, m_03, m_10, m_11, m_13, m_30, m_31, m_33);
		adj.m_23 = -det3x3(m_00, m_01, m_02, m_10, m_11, m_12, m_30, m_31, m_32);

		adj.m_30 = -det3x3(m_01, m_02, m_03, m_11, m_12, m_13, m_21, m_22, m_23);
		adj.m_31 = det3x3(m_00, m_02, m_03, m_10, m_12, m_13, m_20, m_22, m_23);
		adj.m_32 = -det3x3(m_00, m_01, m_03, m_10, m_11, m_13, m_20, m_21, m_23);
		adj.m_33 = det3x3(m_00, m_01, m_02, m_10, m_11, m_12, m_20, m_21, m_22);

		//TOAST_CORE_INFO("adj of matrix before transpose:");
		//adj.ToString();

		// Transpose (simply rearrange the cofactors)
		//Matrix adjTranspose = adj.Transpose();

		//adjTranspose.m_00 = adj.m_00;
		//adjTranspose.m_01 = adj.m_10;
		//adjTranspose.m_02 = adj.m_20;
		//adjTranspose.m_03 = adj.m_30;

		//adjTranspose.m_10 = adj.m_01;
		//adjTranspose.m_11 = adj.m_11;
		//adjTranspose.m_12 = adj.m_21;
		//adjTranspose.m_13 = adj.m_31;

		//adjTranspose.m_20 = adj.m_02;
		//adjTranspose.m_21 = adj.m_12;
		//adjTranspose.m_22 = adj.m_22;
		//adjTranspose.m_23 = adj.m_32;

		//adjTranspose.m_30 = adj.m_03;
		//adjTranspose.m_31 = adj.m_13;
		//adjTranspose.m_32 = adj.m_23;
		//adjTranspose.m_33 = adj.m_33;

		//adjTranspose.ToString();
		return adj.Transpose();

	}

	Matrix Matrix::Inverse(const Matrix& mat) {
		double det = mat.Determinant();
		if (std::abs(det) < 1e-6) {  // considering very small values as zero
			// Matrix is singular, cannot compute the inverse.
			// You might want to handle this better, perhaps by returning an identity matrix 
			// or throwing an exception.
			TOAST_CORE_ERROR("Determinant to small!");
			return Matrix();  // Return identity or another default value for now.
		}

		Matrix adj = mat.Adjugate();
		Matrix inv;

		//TOAST_CORE_INFO("adj of matrix after transpose:");
		//adj.ToString();

		inv.m_00 = adj.m_00 / det;
		inv.m_01 = adj.m_01 / det;
		inv.m_02 = adj.m_02 / det;
		inv.m_03 = adj.m_03 / det;

		inv.m_10 = adj.m_10 / det;
		inv.m_11 = adj.m_11 / det;
		inv.m_12 = adj.m_12 / det;
		inv.m_13 = adj.m_13 / det;

		inv.m_20 = adj.m_20 / det;
		inv.m_21 = adj.m_21 / det;
		inv.m_22 = adj.m_22 / det;
		inv.m_23 = adj.m_23 / det;

		inv.m_30 = adj.m_30 / det;
		inv.m_31 = adj.m_31 / det;
		inv.m_32 = adj.m_32 / det;
		inv.m_33 = adj.m_33 / det;

		return inv;
	}

	Matrix Matrix::TranslationFromVector(const Vector3& translationVector)
	{
		Matrix translationMatrix;

		translationMatrix.m_00 = 1.0; translationMatrix.m_01 = 0.0; translationMatrix.m_02 = 0.0; translationMatrix.m_03 = 0.0;
		translationMatrix.m_10 = 0.0; translationMatrix.m_11 = 1.0; translationMatrix.m_12 = 0.0; translationMatrix.m_13 = 0.0;
		translationMatrix.m_20 = 0.0; translationMatrix.m_21 = 0.0; translationMatrix.m_22 = 1.0; translationMatrix.m_23 = 0.0;
		translationMatrix.m_30 = translationVector.x; translationMatrix.m_31 = translationVector.y; translationMatrix.m_32 = translationVector.z; translationMatrix.m_33 = 1.0;

		return translationMatrix;
	}

	Matrix Matrix::ScalingFromVector(const Vector3& scaleVector)
	{
		Matrix scalingMatrix;

		scalingMatrix.m_00 = scaleVector.x;	  scalingMatrix.m_01 = 0.0;             scalingMatrix.m_02 = 0.0;             scalingMatrix.m_03 = 0.0;
		scalingMatrix.m_10 = 0.0;             scalingMatrix.m_11 = scaleVector.y; scalingMatrix.m_12 = 0.0;             scalingMatrix.m_13 = 0.0;
		scalingMatrix.m_20 = 0.0;             scalingMatrix.m_21 = 0.0;             scalingMatrix.m_22 = scaleVector.z; scalingMatrix.m_23 = 0.0;
		scalingMatrix.m_30 = 0.0;             scalingMatrix.m_31 = 0.0;             scalingMatrix.m_32 = 0.0;             scalingMatrix.m_33 = 1.0;

		return scalingMatrix;
	}

	Matrix Matrix::RotationFromEauler(const Vector3& rot) 
	{
		Matrix m;

		double cosX = std::cos(rot.x);
		double sinX = std::sin(rot.x);
		double cosY = std::cos(rot.y);
		double sinY = std::sin(rot.y);
		double cosZ = std::cos(rot.z);
		double sinZ = std::sin(rot.z);

		// Rotation matrix around X-axis (pitch)
		Matrix Rx = Matrix::Identity();
		Rx.m_11 = cosX;
		Rx.m_12 = -sinX;
		Rx.m_21 = sinX;
		Rx.m_22 = cosX;

		// Rotation matrix around Y-axis (yaw)
		Matrix Ry = Matrix::Identity();
		Ry.m_00 = cosY;
		Ry.m_02= sinY;
		Ry.m_20 = -sinY;
		Ry.m_22 = cosY;

		// Rotation matrix around Z-axis (roll)
		Matrix Rz = Matrix::Identity();
		Rz.m_00 = cosZ;
		Rz.m_01 = -sinZ;
		Rz.m_10 = sinZ;
		Rz.m_11 = cosZ;

		// Combined rotation matrix: R = Rz * Ry * Rx
		m = Rz * Ry * Rx;

		return m;
	}

	Matrix Matrix::FromQuaternion(const Quaternion& q) {
		Matrix m;
		Quaternion qNorm = q;
		qNorm = Quaternion::Normalize(qNorm);

		m.m_00 = 1.0 - 2.0 * qNorm.y * qNorm.y - 2.0 * qNorm.z * qNorm.z;
		m.m_01 = 2.0 * qNorm.x * qNorm.y - 2.0 * qNorm.z * qNorm.w;
		m.m_02 = 2.0 * qNorm.x * qNorm.z + 2.0 * qNorm.y * qNorm.w;
		m.m_03 = 0.0;

		m.m_10 = 2.0 * qNorm.x * qNorm.y + 2.0 * qNorm.z * qNorm.w;
		m.m_11 = 1.0 - 2.0 * qNorm.x * qNorm.x - 2.0 * qNorm.z * qNorm.z;
		m.m_12 = 2.0 * qNorm.y * qNorm.z - 2.0 * qNorm.x * qNorm.w;
		m.m_13 = 0.0;

		m.m_20 = 2.0 * qNorm.x * qNorm.z - 2.0 * qNorm.y * qNorm.w;
		m.m_21 = 2.0 * qNorm.y * qNorm.z + 2.0 * qNorm.x * qNorm.w;
		m.m_22 = 1.0 - 2.0 * qNorm.x * qNorm.x - 2.0 * qNorm.y * qNorm.y;
		m.m_23 = 0.0;

		m.m_30 = 0.0;
		m.m_31 = 0.0;
		m.m_32 = 0.0;
		m.m_33 = 1.0;

		return m;
	}

	void Matrix::ToString()
	{
		TOAST_CORE_INFO("Matrix: %lf, %lf, %lf, %lf", m_00, m_01, m_02, m_03);
		TOAST_CORE_INFO("        %lf, %lf, %lf, %lf", m_10, m_11, m_12, m_13);
		TOAST_CORE_INFO("        %lf, %lf, %lf, %lf", m_20, m_21, m_22, m_23);
		TOAST_CORE_INFO("        %lf, %lf, %lf, %lf", m_30, m_31, m_32, m_33);
	}

}