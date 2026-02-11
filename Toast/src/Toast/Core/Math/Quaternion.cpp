#include "tpch.h"

#include "Quaternion.h"

#include "Toast/Core/Math/Vector.h"

namespace Toast {

	Quaternion::Quaternion(DirectX::XMVECTOR quat)
	{
		x = DirectX::XMVectorGetX(quat);
		y = DirectX::XMVectorGetY(quat);
		z = DirectX::XMVectorGetZ(quat);
		w = DirectX::XMVectorGetW(quat);
	}

	Quaternion::Quaternion(Vector3 vec, double w)
	{
		x = vec.x;
		y = vec.y;
		z = vec.z;
		this->w = w;
	}

	Quaternion Quaternion::Conjugate() const {
		return Quaternion(-x, -y, -z, w);
	}

	double Quaternion::Magnitude(Quaternion& quat) {
		return sqrt(quat.w * quat.w + quat.x * quat.x + quat.y * quat.y + quat.z * quat.z);
	}

	Quaternion Quaternion::Normalize(Quaternion& quat) {
		double magnitude = Quaternion::Magnitude(quat);
		if (magnitude > 0.0)
		{
			quat.x /= magnitude;
			quat.y /= magnitude;
			quat.z /= magnitude;
			quat.w /= magnitude;
		}

		return quat;
	}

	Quaternion Quaternion::FromAxisAngle(Vector3& axis, double theta) {
		// Ensure the axis is normalized
		Vector3 normalizedAxis = Vector3::Normalize(axis);

		double sinThetaOverTwo = sin(theta / 2.0);
		double cosThetaOverTwo = cos(theta / 2.0);

		return Quaternion(
			normalizedAxis.x * sinThetaOverTwo,
			normalizedAxis.y * sinThetaOverTwo,
			normalizedAxis.z * sinThetaOverTwo,
			cosThetaOverTwo
		);
	}

	// Function to create a quaternion from roll, pitch, yaw
	Quaternion Quaternion::FromRollPitchYaw(double pitch, double yaw, double roll)
	{
		using namespace DirectX;

		XMVECTOR q = XMQuaternionRotationRollPitchYaw((float)pitch, (float)yaw, (float)roll);
		q = XMQuaternionNormalize(q);

		Quaternion out;
		out.x = (double)XMVectorGetX(q);
		out.y = (double)XMVectorGetY(q);
		out.z = (double)XMVectorGetZ(q);
		out.w = (double)XMVectorGetW(q);
		return out;
	}

	void Quaternion::ToString(const std::string& label)
	{
		TOAST_CORE_INFO("%s: %lf, %lf, %lf, %lf", label.c_str(), x, y, z, w);
	}
}