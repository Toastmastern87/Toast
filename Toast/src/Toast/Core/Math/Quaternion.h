#pragma once

namespace Toast {

	class Vector3;

	class Quaternion {
	public:
		Quaternion() : x(0), y(0), z(0), w(1) {}  // Default constructor for identity quaternion
		Quaternion(double x, double y, double z, double w) : x(x), y(y), z(z), w(w) {}
		Quaternion(Vector3 vec, double w);
		Quaternion(DirectX::XMFLOAT4 quat) : x(quat.x), y(quat.y), z(quat.z), w(quat.w) {}
		Quaternion(DirectX::XMVECTOR quat);

		Quaternion operator+(const Quaternion& rhs) const {
			return Quaternion(x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w);
		}

		// Multiplication of two quaternions
		Quaternion operator*(const Quaternion& rhs) const {
			Quaternion q;
			q.w = w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z;
			q.x = w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y;
			q.y = w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x;
			q.z = w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w;
			return q;
		}

		Quaternion Conjugate() const;

		static double Magnitude(Quaternion& quat);
		static Quaternion Normalize(Quaternion& quat);
		static Quaternion FromAxisAngle(Vector3& axis, double theta);
		// Function to create a quaternion from roll, pitch, yaw
		static Quaternion FromRollPitchYaw(double roll, double pitch, double yaw);

		void ToString(const std::string& label);
	public:
		double x, y, z, w;
	};

}