#pragma once

#include <DirectXMath.h>

namespace Toast {

	namespace Bounds {

		static bool Intersects(const std::tuple<DirectX::XMFLOAT3, DirectX::XMFLOAT3>& boundA, const std::tuple<DirectX::XMFLOAT3, DirectX::XMFLOAT3>& boundB)
		{
			DirectX::XMFLOAT3 minsA = std::get<0>(boundA);
			DirectX::XMFLOAT3 maxsA = std::get<1>(boundA);

			DirectX::XMFLOAT3 minsB = std::get<0>(boundB);
			DirectX::XMFLOAT3 maxsB = std::get<1>(boundB);

			if (maxsA.x < minsB.x || maxsA.y < minsB.y || maxsA.z < minsB.z)
				return false;

			if (minsA.x > maxsB.x || minsA.y > maxsB.y || minsA.z > maxsB.z)
				return false;

			return true;
		}

		static std::tuple<DirectX::XMFLOAT3, DirectX::XMFLOAT3> Expand(std::tuple<DirectX::XMFLOAT3, DirectX::XMFLOAT3>& bounds, DirectX::XMFLOAT3& expandVec)
		{
			DirectX::XMFLOAT3 mins = std::get<0>(bounds);
			DirectX::XMFLOAT3 maxs = std::get<1>(bounds);

			if ((mins.x + expandVec.x) < mins.x)
				mins.x += expandVec.x;
			if ((mins.y + expandVec.y) < mins.y)
				mins.y += expandVec.y;
			if ((mins.z + expandVec.z) < mins.z)
				mins.z += expandVec.z;

			if((maxs.x + expandVec.x) > maxs.x)
				maxs.x += expandVec.x;
			if ((maxs.y + expandVec.y) > maxs.y)
				maxs.y += expandVec.y;
			if ((maxs.z + expandVec.z) > maxs.z)
				maxs.z += expandVec.z;

			return std::make_tuple(mins, maxs);
		}

		static std::tuple<DirectX::XMFLOAT3, DirectX::XMFLOAT3> GetBounds(const DirectX::XMFLOAT3& pos, DirectX::XMFLOAT4& orient, const float radius)
		{
			std::tuple<DirectX::XMFLOAT3, DirectX::XMFLOAT3> bounds;

			std::get<0>(bounds) = DirectX::XMFLOAT3(pos.x - radius, pos.y - radius, pos.z - radius);
			std::get<1>(bounds) = DirectX::XMFLOAT3(pos.x + radius, pos.y + radius, pos.z + radius);

			return bounds;
		}

		static std::tuple<DirectX::XMFLOAT3, DirectX::XMFLOAT3> GetPlanetBounds(const DirectX::XMFLOAT3& pos, const float maxAltitude, const float radius)
		{
			std::tuple<DirectX::XMFLOAT3, DirectX::XMFLOAT3> bounds;

			std::get<0>(bounds) = DirectX::XMFLOAT3(-(radius + maxAltitude) + pos.x, -(radius + maxAltitude) + pos.y, -(radius + maxAltitude) + pos.z);
			std::get<1>(bounds) = DirectX::XMFLOAT3(radius + maxAltitude + pos.x, radius + maxAltitude + pos.y, radius + maxAltitude + pos.z);

			return bounds;
		}
	}

}