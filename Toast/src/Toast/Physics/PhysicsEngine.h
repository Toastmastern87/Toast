#pragma once

#include "Toast/Scene/Components.h"

#include <DirectXMath.h>

namespace Toast {

	namespace PhysicsEngine {
		
		static void ApplyImpulseLinear(RigidBodyComponent& rbc, DirectX::XMVECTOR impulse)
		{
			if (rbc.InvMass == 0.0f)
				return;

			DirectX::XMStoreFloat3(&rbc.LinearVelocity, (DirectX::XMLoadFloat3(&rbc.LinearVelocity) + impulse * rbc.InvMass));
		}
	}
}