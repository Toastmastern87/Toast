#pragma once

#include "Toast/Core/Math/Vector.h"

namespace Toast {

	struct Particle {
		Vector3 position;
		Vector3 velocity;
		Vector3 acceleration;
		float lifetime;
	};

}