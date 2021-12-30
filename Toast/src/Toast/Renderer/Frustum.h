#pragma once

#include <DirectXMath.h>

using namespace DirectX;

struct Plane
{
	DirectX::XMVECTOR Normal;
	DirectX::XMVECTOR Distance;

	Plane()
	{
		Normal = { 0.0f, 1.0f, 0.0f };
		Distance = { 0.0f, 0.0f, 0.0f };
	}

	Plane(DirectX::XMVECTOR a, DirectX::XMVECTOR b, DirectX::XMVECTOR c)
	{
		Distance = a;
		Normal = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(-b + a, c - a));
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

		void Update(DirectX::XMMATRIX& transform, float aspectRatio, float FOV, float nearClip, float farClip, DirectX::XMVECTOR& pos);

		bool Contains(DirectX::XMVECTOR p);
		VolumeTri ContainsTriangle(DirectX::XMVECTOR p1, DirectX::XMVECTOR p2, DirectX::XMVECTOR p3);
		VolumeTri ContainsTriangleVolume(DirectX::XMVECTOR p1, DirectX::XMVECTOR p2, DirectX::XMVECTOR p3, float height);
	private:
		std::vector<Plane> mPlanes;
	};
}