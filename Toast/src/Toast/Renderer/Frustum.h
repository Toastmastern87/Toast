#pragma once

#include <DirectXMath.h>

using namespace DirectX;

struct Plane
{
	DirectX::XMVECTOR Normal;
	float D;

	Plane()
	{
		Normal = { 0.0f, 1.0f, 0.0f };
		D = 0.0f;
	}

	Plane(DirectX::XMVECTOR a, DirectX::XMVECTOR b, DirectX::XMVECTOR c)
	{
		Normal = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(b - a, c - a));
		D = DirectX::XMVectorGetX((DirectX::XMVector3Dot(Normal, a)));
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

		void Invalidate(float aspectRatio, float FOV, float nearClip, float farClip, DirectX::XMVECTOR& pos);
		void Update(DirectX::XMMATRIX& transform, DirectX::XMMATRIX* planetTransform = nullptr);

		bool Contains(DirectX::XMVECTOR p);
		VolumeTri ContainsTriangle(DirectX::XMVECTOR p1, DirectX::XMVECTOR p2, DirectX::XMVECTOR p3);
		VolumeTri ContainsTriangleVolume(DirectX::XMVECTOR p1, DirectX::XMVECTOR p2, DirectX::XMVECTOR p3, float height);
	public:
		DirectX::XMVECTOR mCenterNear;
		DirectX::XMVECTOR mCenterFar;

		DirectX::XMVECTOR mNearTopLeft;
		DirectX::XMVECTOR mNearTopRight;
		DirectX::XMVECTOR mNearBottomLeft;
		DirectX::XMVECTOR mNearBottomRight;

		DirectX::XMVECTOR mFarTopLeft;
		DirectX::XMVECTOR mFarTopRight;
		DirectX::XMVECTOR mFarBottomLeft;
		DirectX::XMVECTOR mFarBottomRight;

		DirectX::XMVECTOR mPlanetSpaceNearTopLeft;
		DirectX::XMVECTOR mPlanetSpaceNearTopRight;
		DirectX::XMVECTOR mPlanetSpaceNearBottomLeft;
		DirectX::XMVECTOR mPlanetSpaceNearBottomRight;

		DirectX::XMVECTOR mPlanetSpaceFarTopLeft;
		DirectX::XMVECTOR mPlanetSpaceFarTopRight;
		DirectX::XMVECTOR mPlanetSpaceFarBottomLeft;
		DirectX::XMVECTOR mPlanetSpaceFarBottomRight;
	private:
		std::vector<Plane> mPlanes;
		std::vector<Plane> mPlanetCheckPlanes;
	};
}