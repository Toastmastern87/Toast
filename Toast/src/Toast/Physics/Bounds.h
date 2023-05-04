#pragma once

#include <DirectXMath.h>

namespace Toast {

	namespace Bounds {

		static bool Intersects(const DirectX::XMFLOAT3& boundA, const DirectX::XMFLOAT3& boundB)
		{
			if (boundA.x < boundB.x || boundA.y < boundB.y || boundA.z < boundB.z)
				return false;

			if (boundA.x > boundB.x || boundA.y > boundB.y || boundA.z > boundB.z)
				return false;

			return true;
		}

		static std::tuple<DirectX::XMFLOAT3, DirectX::XMFLOAT3> GetBounds(const DirectX::XMFLOAT3& pos, DirectX::XMFLOAT4& orient, const float radius)
		{
			std::tuple<DirectX::XMFLOAT3, DirectX::XMFLOAT3> bounds;

			std::get<0>(bounds) = DirectX::XMFLOAT3(pos.x - radius, pos.y - radius, pos.z - radius);
			std::get<1>(bounds) = DirectX::XMFLOAT3(pos.x + radius, pos.y + radius, pos.z + radius);

			return bounds;
		}
	}

}