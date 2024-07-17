#include "tpch.h"
#include "Bounds.h"

namespace Toast {

	const Bounds& Bounds::operator=(const Bounds& rhs)
	{
		mins = rhs.mins;
		maxs = rhs.maxs;
		return *this;
	}

	const Toast::Bounds& Bounds::operator+(const Vector3& rhs)
	{
		mins = mins + rhs;
		maxs = maxs + rhs;
		return *this;
	}

	const Toast::Bounds& Bounds::operator-(const Vector3& rhs)
	{
		mins = mins - rhs;
		maxs = maxs - rhs;
		return *this;
	}

	const Bounds& Bounds::operator*(Matrix& transform)
	{
		mins = transform * mins;
		maxs = transform * maxs;
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

	void Bounds::ToString(const std::string& label)
	{
		TOAST_CORE_INFO("%s: Min: %lf, %lf, %lf", label.c_str(), mins.x, mins.y, mins.z);
		TOAST_CORE_INFO("    Max: %lf, %lf, %lf", maxs.x, maxs.y, maxs.z);
	}

}