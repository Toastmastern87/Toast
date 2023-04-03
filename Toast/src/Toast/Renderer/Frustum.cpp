#include "tpch.h"
#include "Frustum.h"

#include <DirectXMath.h>

using namespace DirectX;

namespace Toast {

	void Frustum::Invalidate(float aspectRatio, float FOV, float nearClip, float farClip, DirectX::XMVECTOR& pos)
	{
		DirectX::XMVECTOR right = { 1.0f, 0.0f, 0.0f };
		DirectX::XMVECTOR up = { 0.0f, 1.0f, 0.0f };
		DirectX::XMVECTOR forward = { 0.0f, 0.0f, 1.0f };

		float heightNear = 2.0f * tan(DirectX::XMConvertToRadians(FOV) / 2.0f) * nearClip;
		float widthNear = heightNear * aspectRatio;

		float heightFar = 2.0f * tan(DirectX::XMConvertToRadians(FOV) / 2.0f) * farClip;
		float widthFar = heightFar * aspectRatio;

		mCenterNear = pos + DirectX::XMVector3Normalize(forward) * nearClip;
		mCenterFar = pos + DirectX::XMVector3Normalize(forward) * farClip;

		mNearTopLeft = mCenterNear + (up * (heightNear / 2.0f)) - (right * (widthNear / 2.0f));
		mNearTopRight = mCenterNear + (up * (heightNear / 2.0f)) + (right * (widthNear / 2.0f));
		mNearBottomLeft = mCenterNear - (up * (heightNear / 2.0f)) - (right * (widthNear / 2.0f));
		mNearBottomRight = mCenterNear - (up * (heightNear / 2.0f)) + (right * (widthNear / 2.0f));

		mFarTopLeft = mCenterFar + (up * (heightFar / 2.0f)) - (right * (widthFar / 2.0f));
		mFarTopRight = mCenterFar + (up * (heightFar / 2.0f)) + (right * (widthFar / 2.0f));
		mFarBottomLeft = mCenterFar - (up * (heightFar / 2.0f)) - (right * (widthFar / 2.0f));
		mFarBottomRight = mCenterFar - (up * (heightFar / 2.0f)) + (right * (widthFar / 2.0f));
	}

	void Frustum::Update(DirectX::XMMATRIX& transform, DirectX::XMMATRIX* planetTransform)
	{
	//	TOAST_CORE_INFO("Updating camera frustum");

		mNearTopLeft = DirectX::XMVector3Transform(mNearTopLeft, transform);
		mNearTopRight = DirectX::XMVector3Transform(mNearTopRight, transform);
		mNearBottomLeft = DirectX::XMVector3Transform(mNearBottomLeft, transform);
		mNearBottomRight = DirectX::XMVector3Transform(mNearBottomRight, transform);
		mFarTopLeft = DirectX::XMVector3Transform(mFarTopLeft, transform);
		mFarTopRight = DirectX::XMVector3Transform(mFarTopRight, transform);
		mFarBottomLeft = DirectX::XMVector3Transform(mFarBottomLeft, transform);
		mFarBottomRight = DirectX::XMVector3Transform(mFarBottomRight, transform);

		mPlanes.clear();
		//winding in an outside perspective so the cross product creates normals pointing inward
		mPlanes.emplace_back(Plane(mNearTopLeft, mNearBottomLeft, mNearTopRight));//Near
		mPlanes.emplace_back(Plane(mFarTopRight, mFarBottomRight, mFarTopLeft));//Far 
		mPlanes.emplace_back(Plane(mFarTopLeft, mFarBottomLeft, mNearTopLeft));//Left
		mPlanes.emplace_back(Plane(mNearTopRight, mNearBottomRight, mFarTopRight));//Right
		mPlanes.emplace_back(Plane(mFarTopLeft, mNearTopLeft, mFarTopRight));//Top
		mPlanes.emplace_back(Plane(mNearBottomLeft, mFarBottomLeft, mNearBottomRight));//Bottom

		if (planetTransform)
		{
			mPlanetCheckPlanes.clear();

			mPlanetSpaceNearTopLeft = DirectX::XMVector3Transform(mNearTopLeft, DirectX::XMMatrixInverse(nullptr, *planetTransform));
			mPlanetSpaceNearTopRight = DirectX::XMVector3Transform(mNearTopRight, DirectX::XMMatrixInverse(nullptr, *planetTransform));
			mPlanetSpaceNearBottomLeft = DirectX::XMVector3Transform(mNearBottomLeft, DirectX::XMMatrixInverse(nullptr, *planetTransform));
			mPlanetSpaceNearBottomRight = DirectX::XMVector3Transform(mNearBottomRight, DirectX::XMMatrixInverse(nullptr, *planetTransform));
			mPlanetSpaceFarTopLeft = DirectX::XMVector3Transform(mFarTopLeft, DirectX::XMMatrixInverse(nullptr, *planetTransform));
			mPlanetSpaceFarTopRight = DirectX::XMVector3Transform(mFarTopRight, DirectX::XMMatrixInverse(nullptr, *planetTransform));
			mPlanetSpaceFarBottomLeft = DirectX::XMVector3Transform(mFarBottomLeft, DirectX::XMMatrixInverse(nullptr, *planetTransform));
			mPlanetSpaceFarBottomRight = DirectX::XMVector3Transform(mFarBottomRight, DirectX::XMMatrixInverse(nullptr, *planetTransform));

			//winding in an outside perspective so the cross product creates normals pointing inward
			mPlanetCheckPlanes.emplace_back(Plane(mPlanetSpaceNearTopLeft, mPlanetSpaceNearBottomLeft, mPlanetSpaceNearTopRight));//Near
			mPlanetCheckPlanes.emplace_back(Plane(mPlanetSpaceFarTopRight, mPlanetSpaceFarBottomRight, mPlanetSpaceFarTopLeft));//Far 
			mPlanetCheckPlanes.emplace_back(Plane(mPlanetSpaceFarTopLeft, mPlanetSpaceFarBottomLeft, mPlanetSpaceNearTopLeft));//Left
			mPlanetCheckPlanes.emplace_back(Plane(mPlanetSpaceNearTopRight, mPlanetSpaceNearBottomRight, mPlanetSpaceFarTopRight));//Right
			mPlanetCheckPlanes.emplace_back(Plane(mPlanetSpaceFarTopLeft, mPlanetSpaceNearTopLeft, mPlanetSpaceFarTopRight));//Top
			mPlanetCheckPlanes.emplace_back(Plane(mPlanetSpaceNearBottomLeft, mPlanetSpaceFarBottomLeft, mPlanetSpaceNearBottomRight));//Bottom
		}
	}

	bool Frustum::Contains(DirectX::XMVECTOR p)
	{
		for (auto plane : mPlanes)
		{
			if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(plane.Normal, p)) - plane.D < 0.0f)
				return false;
		}
		return true;
	}

	VolumeTri Frustum::ContainsTriangle(DirectX::XMVECTOR p1, DirectX::XMVECTOR p2, DirectX::XMVECTOR p3)
	{
		VolumeTri ret = VolumeTri::CONTAINS;
		for (auto plane : mPlanes)
		{
			uint8_t rejects = 0;

			if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(plane.Normal, p1)) - plane.D < 0.0f)
				rejects++;
			if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(plane.Normal, p2)) - plane.D < 0.0f)
				rejects++;
			if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(plane.Normal, p3)) - plane.D < 0.0f)
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

	VolumeTri Frustum::ContainsTriangleVolume(DirectX::XMVECTOR p1, DirectX::XMVECTOR p2, DirectX::XMVECTOR p3, float height)
	{
		VolumeTri ret = VolumeTri::CONTAINS;

		DirectX::XMVECTOR firstC = p1;
		DirectX::XMVECTOR secondC = p2 - p1;
		DirectX::XMVECTOR thirdC = p3 - p1;

		int i = 0;

		for (auto plane : mPlanetCheckPlanes)
		{
			uint8_t rejects = 0;

			// TODO move the Furstum Culling tolerance to a scene setting!
			// -0.1f is to make the Frustum Culling more forgiving, this should be a scene setting in the future!
			if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(plane.Normal, p1)) - plane.D < -0.01f)
				rejects++;
			if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(plane.Normal, p2)) - plane.D < -0.01f)
				rejects++;
			if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(plane.Normal, p3)) - plane.D < -0.01f)
				rejects++;
			// if all three are outside a plane the triangle is outside the frustrum
			if (rejects >= 3)
			{
				if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(plane.Normal, p1 * height)) - plane.D < -0.01f)
					rejects++;
				if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(plane.Normal, p2 * height)) - plane.D < -0.01f)
					rejects++;
				if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(plane.Normal, p3 * height)) - plane.D < -0.01f)
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