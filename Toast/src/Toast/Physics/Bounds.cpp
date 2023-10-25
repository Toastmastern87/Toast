#include "tpch.h"
#include "Bounds.h"

namespace Toast {

	const Bounds& Bounds::operator= (const Bounds& rhs)
	{
		mins = rhs.mins;
		maxs = rhs.maxs;
		return *this;
	}

	bool Bounds::Intersects(const Bounds& rhs) const
	{
		if (maxs.x < rhs.mins.x || maxs.y < rhs.mins.y || maxs.z < rhs.mins.z)
			return false;

		if (mins.x > rhs.maxs.x || mins.y > rhs.maxs.y || mins.z > rhs.maxs.z)
			return false;

		return true;
	}

	void Bounds::Expand(const Vector3* pts, const int num)
	{
		for (int i = 0; i < num; i++)
			Expand(pts[i]);
	}

	void Bounds::Expand(const Vector3& rhs)
	{
		if (rhs.x < mins.x)
			mins.x = rhs.x;
		if (rhs.y < mins.y)
			mins.y = rhs.y;
		if (rhs.z < mins.z)
			mins.z = rhs.z;

		if (rhs.x > maxs.x)
			maxs.x = rhs.x;
		if (rhs.y > maxs.y)
			maxs.y = rhs.y;
		if (rhs.z > maxs.z)
			maxs.z = rhs.z;
	}

	void Bounds::Expand(const Bounds& rhs)
	{
		Expand(rhs.mins);
		Expand(rhs.maxs);
	}

}