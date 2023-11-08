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

	void Frustum::Update(Matrix& transform, Matrix& planetTransform)
	{
		//	TOAST_CORE_INFO("Updating camera frustum");
		//TOAST_CORE_CRITICAL("BEFORE mNearTopLeft: %lf, %lf, %lf", mNearTopLeft.x, mNearTopLeft.y, mNearTopLeft.z);
		mNearTopLeft = transform * mNearTopLeft; 
		mNearTopRight = transform * mNearTopRight; 
		mNearBottomLeft = transform * mNearBottomLeft; 
		mNearBottomRight = transform * mNearBottomRight; 
		mFarTopLeft = transform * mFarTopLeft; 
		mFarTopRight = transform * mFarTopRight;
		mFarBottomLeft = transform * mFarBottomLeft;
		mFarBottomRight = transform * mFarBottomRight;

		//TOAST_CORE_CRITICAL("AFTER mNearTopLeft: %lf, %lf, %lf", mNearTopLeft.x, mNearTopLeft.y, mNearTopLeft.z);

		mPlanes.clear();
		//winding in an outside perspective so the cross product creates normals pointing inward
		mPlanes.emplace_back(Plane(mNearTopLeft, mNearBottomLeft, mNearTopRight));//Near
		mPlanes.emplace_back(Plane(mFarTopRight, mFarBottomRight, mFarTopLeft));//Far 
		mPlanes.emplace_back(Plane(mFarTopLeft, mFarBottomLeft, mNearTopLeft));//Left
		mPlanes.emplace_back(Plane(mNearTopRight, mNearBottomRight, mFarTopRight));//Right
		mPlanes.emplace_back(Plane(mFarTopLeft, mNearTopLeft, mFarTopRight));//Top
		mPlanes.emplace_back(Plane(mNearBottomLeft, mFarBottomLeft, mNearBottomRight));//Bottom

		mPlanetCheckPlanes.clear();

		Matrix inversePlanetMatrix = Matrix::Inverse(planetTransform);

		mPlanetSpaceNearTopLeft = inversePlanetMatrix * mNearTopLeft;
		mPlanetSpaceNearTopRight = inversePlanetMatrix * mNearTopRight;
		mPlanetSpaceNearBottomLeft = inversePlanetMatrix * mNearBottomLeft;
		mPlanetSpaceNearBottomRight = inversePlanetMatrix * mNearBottomRight;
		mPlanetSpaceFarTopLeft = inversePlanetMatrix * mFarTopLeft;
		mPlanetSpaceFarTopRight = inversePlanetMatrix * mFarTopRight;
		mPlanetSpaceFarBottomLeft = inversePlanetMatrix * mFarBottomLeft;
		mPlanetSpaceFarBottomRight = inversePlanetMatrix * mFarBottomRight;

		//winding in an outside perspective so the cross product creates normals pointing inward
		mPlanetCheckPlanes.emplace_back(Plane(mPlanetSpaceNearTopLeft, mPlanetSpaceNearBottomLeft, mPlanetSpaceNearTopRight));//Near
		mPlanetCheckPlanes.emplace_back(Plane(mPlanetSpaceFarTopRight, mPlanetSpaceFarBottomRight, mPlanetSpaceFarTopLeft));//Far 
		mPlanetCheckPlanes.emplace_back(Plane(mPlanetSpaceFarTopLeft, mPlanetSpaceFarBottomLeft, mPlanetSpaceNearTopLeft));//Left
		mPlanetCheckPlanes.emplace_back(Plane(mPlanetSpaceNearTopRight, mPlanetSpaceNearBottomRight, mPlanetSpaceFarTopRight));//Right
		mPlanetCheckPlanes.emplace_back(Plane(mPlanetSpaceFarTopLeft, mPlanetSpaceNearTopLeft, mPlanetSpaceFarTopRight));//Top
		mPlanetCheckPlanes.emplace_back(Plane(mPlanetSpaceNearBottomLeft, mPlanetSpaceFarBottomLeft, mPlanetSpaceNearBottomRight));//Bottom
	}

	void Frustum::Update(Matrix& transform)
	{
		mNearTopLeft = transform * mNearTopLeft;
		mNearTopRight = transform * mNearTopRight;
		mNearBottomLeft = transform * mNearBottomLeft;
		mNearBottomRight = transform * mNearBottomRight;
		mFarTopLeft = transform * mFarTopLeft;
		mFarTopRight = transform * mFarTopRight;
		mFarBottomLeft = transform * mFarBottomLeft;
		mFarBottomRight = transform * mFarBottomRight;

		mPlanes.clear();
		//winding in an outside perspective so the cross product creates normals pointing inward
		mPlanes.emplace_back(Plane(mNearTopLeft, mNearBottomLeft, mNearTopRight));//Near
		mPlanes.emplace_back(Plane(mFarTopRight, mFarBottomRight, mFarTopLeft));//Far 
		mPlanes.emplace_back(Plane(mFarTopLeft, mFarBottomLeft, mNearTopLeft));//Left
		mPlanes.emplace_back(Plane(mNearTopRight, mNearBottomRight, mFarTopRight));//Right
		mPlanes.emplace_back(Plane(mFarTopLeft, mNearTopLeft, mFarTopRight));//Top
		mPlanes.emplace_back(Plane(mNearBottomLeft, mFarBottomLeft, mNearBottomRight));//Bottom
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

	VolumeTri Frustum::ContainsTriangle(Vector3 p1, Vector3 p2, Vector3 p3)
	{
		VolumeTri ret = VolumeTri::CONTAINS;
		for (auto plane : mPlanes)
		{
			uint8_t rejects = 0;

			if (Vector3::Dot(plane.Normal, p1) - plane.D < 0.0)
				rejects++;
			if (Vector3::Dot(plane.Normal, p2) - plane.D < 0.0)
				rejects++;
			if (Vector3::Dot(plane.Normal, p3) - plane.D < 0.0)
				rejects++;

			// if all three are outside a plane the triangle is outside the frustrum
			if (rejects >= 3)
			{
				//TOAST_CORE_INFO("CONTAINS TRIANGLE OUTSIDE");
				return VolumeTri::OUTSIDE;
			}

			// if at least one is outside the triangle intersects at least one plane
			else if (rejects > 0)
			{
				//TOAST_CORE_INFO("INTERSECTS");
				ret = VolumeTri::INTERSECT;
			}
		}

		//TOAST_CORE_INFO("RETURN VALUE CONTAINS TRIANGLE: %d", ret);
		return ret;
	}

	VolumeTri Frustum::ContainsTriangleVolume(Vector3 p1, Vector3 p2, Vector3 p3, float height)
	{
		VolumeTri ret = VolumeTri::CONTAINS;

		Vector3 firstC = p1;
		Vector3 secondC = p2 - p1;
		Vector3 thirdC = p3 - p1;

		int i = 0;

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
				if (Vector3::Dot(plane.Normal, p1 * height) - plane.D < -0.01)
					rejects++;
				if (Vector3::Dot(plane.Normal, p2 * height) - plane.D < -0.01)
					rejects++;
				if (Vector3::Dot(plane.Normal, p3 * height) - plane.D < -0.01)
					rejects++;
				if (rejects >= 6) 
					return VolumeTri::OUTSIDE;
				else
					ret = VolumeTri::INTERSECT;
			}
			// if at least one is outside the triangle intersects at least one plane
			else if (rejects > 0)
				ret = VolumeTri::INTERSECT;

			i++;
		}

		return ret;
	}

}