#pragma once

#include "Toast/Core/Math/Math.h"

namespace Toast {

	class Bounds
	{
	public:
		Bounds() { Clear(); }
		Bounds(const Bounds& rhs) : mins(rhs.mins), maxs(rhs.maxs) {}
		const Bounds& operator= (const Bounds& rhs);
		~Bounds() = default;

		void Clear() {
			mins = Vector3(1e6f, 1e6f, 1e6f), maxs = Vector3(-1e6f, -1e6f, -1e6f);
		}
		bool Intersects(const Bounds& rhs) const;
		void Expand(const Vector3* pts, const int num);
		void Expand(const Vector3& rhs);
		void Expand(const Bounds& rhs);

		float WidthX() const { return maxs.x - mins.x; }
		float WidthY() const { return maxs.y - mins.y; }
		float WidthZ() const { return maxs.z - mins.z; }

	public:
		Vector3 mins;
		Vector3 maxs;
	};

}