#pragma once

#include "Toast/Core/Math/Math.h"

namespace Toast {

	struct Plane
	{
		Vector3 Normal;
		double D;

		Plane()
		{
			Normal = { 0.0, 1.0, 0.0 };
			D = 0.0;
		}

		Plane(Vector3 a, Vector3 b, Vector3 c, double bias = 0.0)
		{
			Normal = Vector3::Normalize(Vector3::Cross(b - a, c - a));
			D = Vector3::Dot(Normal, a) - bias;
		}
	};

	struct Sphere
	{
		Vector3 center;
		double radius;
	};

	enum class VolumeTri
	{
		OUTSIDE = 0,
		INTERSECT = 1,
		CONTAINS = 2
	};

	class Frustum
	{
	public:
		Frustum() = default;
		~Frustum() = default;

		void Invalidate(float aspectRatio, float FOV, float nearClip, float farClip);
		void Update(Matrix& transform, Matrix& planetTransform, float bias);
		void Update(Matrix& transform);
		void UpdatePlanetSpace(const Vector3& camPosPS, const Vector3& playerCamRightWS, const Vector3& playerCamUpWS, const Vector3& playerCamForwardWS, Quaternion planetInvRotationQuat, const float nearClip, const float farClip, const float fov, const float aspect, float bias);

		bool Contains(Vector3 p);
		VolumeTri ContainsTriangle(const Vector3& p1, const Vector3& p2, const Vector3& p3) const;
		VolumeTri ContainsTriangleVolume(Vector3 p1, Vector3 p2, Vector3 p3, double heightRange) const;
		VolumeTri ContainsPatchSphere(const Vector3& a, const Vector3& b, const Vector3& c, const double radius);

		Sphere ComputePatchBoundingSphere(const Vector3& a, const Vector3& b, const Vector3& c, const double radius);

		void ToString();
	public:
		Vector3 mCenterNear;
		Vector3 mCenterFar;

		Vector3 mNearTopLeft;
		Vector3 mNearTopRight;
		Vector3 mNearBottomLeft;
		Vector3 mNearBottomRight;

		Vector3 mFarTopLeft;
		Vector3 mFarTopRight;
		Vector3 mFarBottomLeft;
		Vector3 mFarBottomRight;

		Vector3 mPlanetSpaceNearTopLeft;
		Vector3 mPlanetSpaceNearTopRight;
		Vector3 mPlanetSpaceNearBottomLeft;
		Vector3 mPlanetSpaceNearBottomRight;

		Vector3 mPlanetSpaceFarTopLeft;
		Vector3 mPlanetSpaceFarTopRight;
		Vector3 mPlanetSpaceFarBottomLeft;
		Vector3 mPlanetSpaceFarBottomRight;
	private:
		std::vector<Plane> mPlanes;
		std::vector<Plane> mPlanetCheckPlanes;

		friend class PlanetSystem;
	};
}