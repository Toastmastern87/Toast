#pragma once

#include "entt.hpp"
#include "Toast/Core/Timestep.h"

namespace Toast {

	class Scene 
	{
	public:
		Scene();
		~Scene();

		entt::entity CreateEntity();

		entt::registry& Reg() { return mRegistry; }

		void OnUpdate(Timestep ts);
	private:
		entt::registry mRegistry;
	};
}
