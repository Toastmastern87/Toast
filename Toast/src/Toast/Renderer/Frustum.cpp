#include "tpch.h"
#include "Frustum.h"

namespace Toast {

	void Frustum::Invalidate(float aspectRatio, float FOV, float nearClip, float farClip)
	{
		Vector3 right = { 1.0, 0.0, 0.0 };
		Vector3 up = { 0.0, 1.0, 0.0 };
		Vector3 forward = { 0.0, 0.0, 1.0 };

		double heightNear = 2.0 * tan(Math::DegreesToRadians((double)FOV) / 2.0) * (double)nearClip;
		double widthNear = heightNear * aspectRatio;

		double heightFar = 2.0 * tan(Math::DegreesToRadians(FOV) / 2.0) * (double)farClip;
		double widthFar = heightFar * aspectRatio;

		mCenterNear = Vector3::Normalize(forward) * nearClip;
		mCenterFar = Vector3::Normalize(forward) * farClip;

		mNearTopLeft = mCenterNear + (up * (heightNear / 2.0)) - (right * (widthNear / 2.0));
		mNearTopRight = mCenterNear + (up * (heightNear / 2.0)) + (right * (widthNear / 2.0));
		mNearBottomLeft = mCenterNear - (up * (heightNear / 2.0)) - (right * (widthNear / 2.0));
		mNearBottomRight = mCenterNear - (up * (heightNear / 2.0)) + (right * (widthNear / 2.0));

		mFarTopLeft = mCenterFar + (up * (heightFar / 2.0)) - (right * (widthFar / 2.0));
		mFarTopRight = mCenterFar + (up * (heightFar / 2.0)) + (right * (widthFar / 2.0));
		mFarBottomLeft = mCenterFar - (up * (heightFar / 2.0)) - (right * (widthFar / 2.0));
		mFarBottomRight = mCenterFar - (up * (heightFar / 2.0)) + (right * (widthFar / 2.0));
	}

	void Frustum::Update(Matrix& transform, Matrix& planetTransform, float bias)
	{
		mNearTopLeft = Matrix::TransformPointRowVector(mNearTopLeft, transform);
		mNearTopRight = Matrix::TransformPointRowVector(mNearTopRight, transform);
		mNearBottomLeft = Matrix::TransformPointRowVector(mNearBottomLeft, transform);
		mNearBottomRight = Matrix::TransformPointRowVector(mNearBottomRight, transform);
		mFarTopLeft = Matrix::TransformPointRowVector(mFarTopLeft, transform);
		mFarTopRight = Matrix::TransformPointRowVector(mFarTopRight, transform);
		mFarBottomLeft = Matrix::TransformPointRowVector(mFarBottomLeft, transform);
		mFarBottomRight = Matrix::TransformPointRowVector(mFarBottomRight, transform);

		mPlanes.clear();
		//winding in an outside perspective so the cross product creates normals pointing inward
		mPlanes.emplace_back(Plane(mNearTopLeft, mNearBottomLeft, mNearTopRight));//Near
		mPlanes.emplace_back(Plane(mFarTopRight, mFarBottomRight, mFarTopLeft));//Far 
		mPlanes.emplace_back(Plane(mFarTopLeft, mFarBottomLeft, mNearTopLeft));//Left
		mPlanes.emplace_back(Plane(mNearTopRight, mNearBottomRight, mFarTopRight));//Right
		mPlanes.emplace_back(Plane(mFarTopLeft, mNearTopLeft, mFarTopRight));//Top
		mPlanes.emplace_back(Plane(mNearBottomLeft, mFarBottomLeft, mNearBottomRight));//Bottom
	}

	void Frustum::Update(Matrix& transform)
	{
		mNearTopLeft = Matrix::TransformPointRowVector(mNearTopLeft, transform);
		mNearTopRight = Matrix::TransformPointRowVector(mNearTopRight, transform);
		mNearBottomLeft = Matrix::TransformPointRowVector(mNearBottomLeft, transform);
		mNearBottomRight = Matrix::TransformPointRowVector(mNearBottomRight, transform);
		mFarTopLeft = Matrix::TransformPointRowVector(mFarTopLeft, transform);
		mFarTopRight = Matrix::TransformPointRowVector(mFarTopRight, transform);
		mFarBottomLeft = Matrix::TransformPointRowVector(mFarBottomLeft, transform);
		mFarBottomRight = Matrix::TransformPointRowVector(mFarBottomRight, transform);

		mPlanes.clear();
		//winding in an outside perspective so the cross product creates normals pointing inward
		mPlanes.emplace_back(Plane(mNearTopLeft, mNearBottomLeft, mNearTopRight));//Near
		mPlanes.emplace_back(Plane(mFarTopRight, mFarBottomRight, mFarTopLeft));//Far 
		mPlanes.emplace_back(Plane(mFarTopLeft, mFarBottomLeft, mNearTopLeft));//Left
		mPlanes.emplace_back(Plane(mNearTopRight, mNearBottomRight, mFarTopRight));//Right
		mPlanes.emplace_back(Plane(mFarTopLeft, mNearTopLeft, mFarTopRight));//Top
		mPlanes.emplace_back(Plane(mNearBottomLeft, mFarBottomLeft, mNearBottomRight));//Bottom
	}

	void Frustum::UpdatePlanetSpace(const Vector3& camPosPS, const Vector3& playerCamRightWS, const Vector3& playerCamUpWS, const Vector3& playerCamForwardWS, Quaternion planetInvRotationQuat, const float nearClip, const float farClip, const float fov, const float aspect, float bias)
	{
		planetInvRotationQuat = Quaternion::Normalize(planetInvRotationQuat);

		Vector3 playerCamForwardPS = Vector3::Normalize(Vector3::Rotate(playerCamForwardWS, planetInvRotationQuat));
		Vector3 playerCamUpPS = Vector3::Normalize(Vector3::Rotate(playerCamUpWS, planetInvRotationQuat));
		Vector3 playerCamRightPS = Vector3::Normalize(Vector3::Rotate(playerCamRightWS, planetInvRotationQuat));

		double heightNear = 2.0 * tan(Math::DegreesToRadians((double)fov) / 2.0) * (double)nearClip;
		double widthNear = heightNear * aspect;

		double heightFar = 2.0 * tan(Math::DegreesToRadians((double)fov) / 2.0) * (double)farClip;
		double widthFar = heightFar * aspect;

		Vector3 centerNear = camPosPS + Vector3::Normalize(playerCamForwardPS) * nearClip;
		Vector3 centerFar = camPosPS + Vector3::Normalize(playerCamForwardPS) * farClip;

		Vector3 nearTopLeft = centerNear + (playerCamUpPS * (heightNear / 2.0)) - (playerCamRightPS * (widthNear / 2.0));
		Vector3 nearTopRight = centerNear + (playerCamUpPS * (heightNear / 2.0)) + (playerCamRightPS * (widthNear / 2.0));
		Vector3 nearBottomLeft = centerNear - (playerCamUpPS * (heightNear / 2.0)) - (playerCamRightPS * (widthNear / 2.0));
		Vector3 nearBottomRight = centerNear - (playerCamUpPS * (heightNear / 2.0)) + (playerCamRightPS * (widthNear / 2.0));

		Vector3 farTopLeft = centerFar + (playerCamUpPS * (heightFar / 2.0)) - (playerCamRightPS * (widthFar / 2.0));
		Vector3 farTopRight = centerFar + (playerCamUpPS * (heightFar / 2.0)) + (playerCamRightPS * (widthFar / 2.0));
		Vector3 farBottomLeft = centerFar - (playerCamUpPS * (heightFar / 2.0)) - (playerCamRightPS * (widthFar / 2.0));
		Vector3 farBottomRight = centerFar - (playerCamUpPS * (heightFar / 2.0)) + (playerCamRightPS * (widthFar / 2.0));

		mPlanetCheckPlanes.clear();
		//winding in an outside perspective so the cross product creates normals pointing inward
		mPlanetCheckPlanes.emplace_back(Plane(nearTopLeft, nearBottomLeft, nearTopRight, bias));//Near
		mPlanetCheckPlanes.emplace_back(Plane(farTopRight, farBottomRight, farTopLeft, bias));//Far 
		mPlanetCheckPlanes.emplace_back(Plane(farTopLeft, farBottomLeft, nearTopLeft, bias));//Left
		mPlanetCheckPlanes.emplace_back(Plane(nearTopRight, nearBottomRight, farTopRight, bias));//Right
		mPlanetCheckPlanes.emplace_back(Plane(farTopLeft, nearTopLeft, farTopRight, bias));//Top
		mPlanetCheckPlanes.emplace_back(Plane(nearBottomLeft, farBottomLeft, nearBottomRight, bias));//Bottom
	}

	bool Frustum::Contains(Vector3 p)
	{
		for (auto plane : mPlanes)
		{
			if (Vector3::Dot(plane.Normal, p) - plane.D < 0.0)
				return false;
		}
		return true;
	}

	VolumeTri Frustum::ContainsTriangle(const Vector3& p1, const Vector3& p2, const Vector3& p3) const
	{
		bool anyIntersect = false;

		for (const auto& plane : mPlanetCheckPlanes)
		{
			const double d1 = Vector3::Dot(plane.Normal, p1) - plane.D;
			const double d2 = Vector3::Dot(plane.Normal, p2) - plane.D;
			const double d3 = Vector3::Dot(plane.Normal, p3) - plane.D;

			const bool in1 = d1 >= -0.01;
			const bool in2 = d2 >= -0.01;
			const bool in3 = d3 >= -0.01;

			// All 3 outside this plane => outside frustum
			if (!in1 && !in2 && !in3)
				return VolumeTri::OUTSIDE;

			// Mixed => intersects at least one plane
			if (!(in1 && in2 && in3))
				anyIntersect = true;
		}

		return anyIntersect ? VolumeTri::INTERSECT : VolumeTri::CONTAINS;
	}

	VolumeTri Frustum::ContainsTriangleVolume(Vector3 p1, Vector3 p2, Vector3 p3, double height) const
	{
		TOAST_PROFILE_FUNCTION();
		
		VolumeTri ret = VolumeTri::CONTAINS;

		for (auto plane : mPlanetCheckPlanes)
		{
			uint8_t rejects = 0;

			// TODO move the Furstum Culling tolerance to a scene setting!
			// -0.1f is to make the Frustum Culling more forgiving, this should be a scene setting in the future!
			if (Vector3::Dot(plane.Normal, p1) - plane.D < -0.01)
				rejects++;
			if (Vector3::Dot(plane.Normal, p2) - plane.D < -0.01)
				rejects++;
			if (Vector3::Dot(plane.Normal, p3) - plane.D < -0.01)
				rejects++;
			// if all three are outside a plane the triangle is outside the frustrum
			if (rejects >= 3)
			{
				//TOAST_CORE_CRITICAL("height: %lf", height);
				//height = 0.0;// works, but its waaay to high I believe.
				if (Vector3::Dot(plane.Normal, p1 * (1.0 + height)) - plane.D < -0.01)
					rejects++;
				if (Vector3::Dot(plane.Normal, p2 * (1.0 + height)) - plane.D < -0.01)
					rejects++;
				if (Vector3::Dot(plane.Normal, p3 * (1.0 + height)) - plane.D < -0.01)
					rejects++;
				if (Vector3::Dot(plane.Normal, p1 * (1.0 - height)) - plane.D < -0.01)
					rejects++;
				if (Vector3::Dot(plane.Normal, p2 * (1.0 - height)) - plane.D < -0.01)
					rejects++;
				if (Vector3::Dot(plane.Normal, p3 * (1.0 - height)) - plane.D < -0.01)
					rejects++;
				if (rejects >= 9)
					return VolumeTri::OUTSIDE;
				else
					ret = VolumeTri::INTERSECT;
			}
			// if at least one is outside the triangle intersects at least one plane
			else if (rejects > 0)
				ret = VolumeTri::INTERSECT;
		}

		return ret;
	}

	VolumeTri Frustum::ContainsPatchSphere(const Vector3& a, const Vector3& b, const Vector3& c, const double radius, int16_t subdivision)
	{
		Vector3 center = (a + b + c) / 3.0;
		//Vector3 center = Vector3::Normalize(a + b + c) * radius;

		double rSq = std::max({
			Vector3::LengthSquared(a - center),
			Vector3::LengthSquared(b - center),
			Vector3::LengthSquared(c - center)
			});

		double multiplier;
		if (subdivision <= 2)
			multiplier = 16.0;  // huge patches, big bulge
		else if (subdivision <= 5)
			multiplier = 12.0;   // medium patches
		else
			multiplier = 4.0;   // small patches, minimal bulge

		double rSq4 = rSq * multiplier; // (r)^2 = (sqrt(rSq)*2)^2 = rSq*4

		bool anyIntersect = false;

		for (size_t i = 0; i < mPlanetCheckPlanes.size(); i++)
		{
			double dc = Vector3::Dot(mPlanetCheckPlanes[i].Normal, center) - mPlanetCheckPlanes[i].D;

			// dc < -r  →  dc negative AND dc² > rSq4
			if (dc < 0.0 && dc * dc > rSq4)
				return VolumeTri::OUTSIDE;

			// |dc| <= r  →  dc² <= rSq4
			if (dc * dc <= rSq4)
				anyIntersect = true;
		}

		return anyIntersect ? VolumeTri::INTERSECT : VolumeTri::CONTAINS;
	}

	void Frustum::ToString()
	{
		TOAST_CORE_INFO("Frustum planet planes!");
		for (auto plane : mPlanetCheckPlanes)
		{
			TOAST_CORE_INFO("Plane D: %lf", plane.D);
			TOAST_CORE_INFO("Plane Normal:");
			plane.Normal.ToString();
		}
	}

}