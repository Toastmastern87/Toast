#pragma once

#include "Toast/Core/Math/Math.h"

struct Plane
{
	Toast::Vector3 Normal;
	double D;

	Plane()
	{
		Normal = { 0.0, 1.0, 0.0 };
		D = 0.0;
	}

	Plane(Toast::Vector3 a, Toast::Vector3 b, Toast::Vector3 c)
	{
		Normal = Toast::Vector3::Normalize(Toast::Vector3::Cross(b - a, c - a));
		D = Toast::Vector3::Dot(Normal, a);
	}
};

enum class VolumeTri
{
	OUTSIDE = 0,
	INTERSECT = 1,
	CONTAINS = 2
};

namespace Toast {

	class Frustum
	{
	public:
		Frustum() = default;
		~Frustum() = default;

		void Invalidate(float aspectRatio, float FOV, float nearClip, float farClip);
		void Update(Matrix& transform, Matrix& planetTransform);
		void Update(Matrix& transform);

		bool Contains(Vector3 p);
		VolumeTri ContainsTriangle(Vector3 p1, Vector3 p2, Vector3 p3);
		VolumeTri ContainsTriangleVolume(Vector3 p1, Vector3 p2, Vector3 p3, double heightRange);

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
	};
}