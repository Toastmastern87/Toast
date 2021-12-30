#include "tpch.h"
#include "Frustum.h"

#include <DirectXMath.h>

using namespace DirectX;

namespace Toast {

	void Frustum::Update(DirectX::XMMATRIX& transform, float aspectRatio, float FOV, float nearClip, float farClip, DirectX::XMVECTOR& pos)
	{
		DirectX::XMVECTOR forward, up, right;

		right = transform.r[0];
		up = transform.r[1];
		forward = transform.r[2];

		float heightNear = 2.0f * tan(DirectX::XMConvertToRadians(FOV) / 2.0f) * nearClip;
		float widthNear = heightNear * aspectRatio;

		float heightFar = 2.0f * tan(DirectX::XMConvertToRadians(FOV) / 2.0f) * farClip;
		float widthFar = heightFar * aspectRatio;

		DirectX::XMVECTOR centerNear = pos + DirectX::XMVector3Normalize(forward) * nearClip;
		DirectX::XMVECTOR centerFar = pos + DirectX::XMVector3Normalize(forward) * farClip;

		DirectX::XMVECTOR nearTopLeft = centerNear + (up * (heightNear / 2.0f)) - (right * (widthNear / 2.0f));
		DirectX::XMVECTOR nearTopRight = centerNear + (up * (heightNear / 2.0f)) + (right * (widthNear / 2.0f));
		DirectX::XMVECTOR nearBottomLeft = centerNear - (up * (heightNear / 2.0f)) - (right * (widthNear / 2.0f));
		DirectX::XMVECTOR nearBottomRight = centerNear - (up * (heightNear / 2.0f)) + (right * (widthNear / 2.0f));

		DirectX::XMVECTOR farTopLeft = centerFar + (up * (heightFar / 2.0f)) - (right * (widthFar / 2.0f));
		DirectX::XMVECTOR farTopRight = centerFar + (up * (heightFar / 2.0f)) + (right * (widthFar / 2.0f));
		DirectX::XMVECTOR farBottomLeft = centerFar - (up * (heightFar / 2.0f)) - (right * (widthFar / 2.0f));
		DirectX::XMVECTOR farBottomRight = centerFar - (up * (heightFar / 2.0f)) + (right * (widthFar / 2.0f));
	 
		mPlanes.clear();
		//winding in an outside perspective so the cross product creates normals pointing inward
		mPlanes.emplace_back(Plane(nearTopLeft, nearTopRight, nearBottomLeft));//Near
		mPlanes.emplace_back(Plane(farTopRight, farTopLeft, farBottomRight));//Far 
		mPlanes.emplace_back(Plane(farTopLeft, nearTopLeft, farBottomLeft));//Left
		mPlanes.emplace_back(Plane(nearTopRight, farTopRight, nearBottomRight));//Right
		mPlanes.emplace_back(Plane(farTopLeft, farTopRight, nearTopLeft));//Top
		mPlanes.emplace_back(Plane(nearBottomLeft, nearBottomRight, farBottomLeft));//Bottom
	}

	bool Frustum::Contains(DirectX::XMVECTOR p)
	{
		for (auto plane : mPlanes)
		{
			if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(plane.Normal, p - plane.Distance)) < 0.0f)
				return false;
		}
		return true;
	}

	VolumeTri Frustum::ContainsTriangle(DirectX::XMVECTOR p1, DirectX::XMVECTOR p2, DirectX::XMVECTOR p3)
	{
		VolumeTri ret = VolumeTri::CONTAINS;
		int i = 0;
		for (auto plane : mPlanes)
		{
			uint8_t rejects = 0;

			if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(plane.Normal, p1 - plane.Distance)) < 0.0f)
				rejects++;
			if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(plane.Normal, p2 - plane.Distance)) < 0.0f)
				rejects++;
			if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(plane.Normal, p3 - plane.Distance)) < 0.0f)
				rejects++;

			// if all three are outside a plane the triangle is outside the frustrum
			if (rejects >= 3) 
				return VolumeTri::OUTSIDE;

			// if at least one is outside the triangle intersects at least one plane
			else if (rejects > 0) 
				ret = VolumeTri::INTERSECT;

			i++;
		}

		return ret;
	}

	VolumeTri Frustum::ContainsTriangleVolume(DirectX::XMVECTOR p1, DirectX::XMVECTOR p2, DirectX::XMVECTOR p3, float height)
	{
		VolumeTri ret = VolumeTri::CONTAINS;

		for (auto plane : mPlanes)
		{
			uint8_t rejects = 0;

			if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(plane.Normal, p1 - plane.Distance)) < 0.0f)
				rejects++;
			if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(plane.Normal, p2 - plane.Distance)) < 0.0f)
				rejects++;
			if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(plane.Normal, p3 - plane.Distance)) < 0.0f)
				rejects++;
			// if all three are outside a plane the triangle is outside the frustrum
			if (rejects >= 3)
			{
				if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(plane.Normal, (p1 * height) - plane.Distance)) < 0.0f)
					rejects++;
				if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(plane.Normal, (p2 * height) - plane.Distance)) < 0.0f)
					rejects++;
				if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(plane.Normal, (p3 * height) - plane.Distance)) < 0.0f)
					rejects++;
				if (rejects >= 6) 
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

}