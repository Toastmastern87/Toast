#include "tpch.h"
#include "Entity.h"

namespace Toast {

	Entity::Entity(entt::entity handle, Scene* scene)
		: mEntityHandle(handle), mScene(scene)
	{
	}
}