#include "tpch.h"

#include "Vector.h"

namespace Toast {

	Vector3::Vector3(float xIn, float yIn, float zIn, float wIn) {
		x = (double)xIn;
		y = (double)yIn;
		z = (double)zIn;
		w = (double)wIn;
	}

	Vector3::Vector3(DirectX::XMVECTOR vec) {
		x = DirectX::XMVectorGetX(vec);
		y = DirectX::XMVectorGetY(vec);
		z = DirectX::XMVectorGetZ(vec);
		w = 1.0;
	}

	Vector3::Vector3(DirectX::XMFLOAT3 vec) {
		x = vec.x;
		y = vec.y;
		z = vec.z;
		w = 1.0;
	}

	Vector3::Vector3(std::initializer_list<double> list)
	{
		auto it = list.begin();

		x = (it != list.end()) ? static_cast<double>(*it++) : 0.0;
		y = (it != list.end()) ? static_cast<double>(*it++) : 0.0;
		z = (it != list.end()) ? static_cast<double>(*it++) : 0.0;
		w = 1.0;
	}

	double Vector3::Magnitude() const {
		return sqrt(x * x + y * y + z * z);
	}

	Vector3 Vector3::Normalize(Vector3& vec) {
		double magnitude = vec.Magnitude();
		if (magnitude > 0.0)
		{
			vec.x /= magnitude;
			vec.y /= magnitude;
			vec.z /= magnitude;
		}

		return vec;
	}

	Vector3 Vector3::Normalize(std::initializer_list<double> list)
	{
		Vector3 vec(list);
		return Normalize(vec);
	}

	Vector3 Vector3::HomogeneousNormalize() const {
		if (w == 0.0) return *this;  // Avoid division by zero
		return Vector3(x / w, y / w, z / w, 1.0);
	}

	Vector3 Vector3::Cross(const Vector3& a, const Vector3& b) {
		return Vector3(
			a.y * b.z - a.z * b.y,
			a.z * b.x - a.x * b.z,
			a.x * b.y - a.y * b.x
		);
	}

	double Vector3::Dot(const Vector3& a, const Vector3& b) {
		return a.x * b.x + a.y * b.y + a.z * b.z;
	}

	Vector3 Vector3::Rotate(const Vector3 vec, const Quaternion& q) {
		Quaternion vecQuat(vec.x, vec.y, vec.z, 0.0);
		Quaternion qInv = q.Conjugate();
		Quaternion resultQuat = q * vecQuat * qInv;

		return Vector3(resultQuat.x, resultQuat.y, resultQuat.z);
	}

}