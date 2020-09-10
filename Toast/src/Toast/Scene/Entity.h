#pragma once

#include "Scene.h"

#include "entt.hpp"

namespace Toast {

	class Entity 
	{
	public:
		Entity() = default;
		Entity(entt::entity handle, Scene* scene);
		Entity(const Entity& other) = default;

		template<typename T, typename... Args>
		T& AddComponent(Args&&... args) 
		{
			TOAST_CORE_ASSERT(!HasComponent<T>(), "Entity already has component!");
			return mScene->mRegistry.emplace<T>(mEntityHandle, std::forward<Args>(args)...);
		}

		template<typename T>
		T& GetComponent() 
		{
			TOAST_CORE_ASSERT(HasComponent<T>(), "Entity does not has component!");
			return mScene->mRegistry.get<T>(mEntityHandle);
		}

		template<typename T>
		bool HasComponent() 
		{
			return mScene->mRegistry.has<T>(mEntityHandle);
		}

		template<typename T>
		void RemoveComponents()
		{
			TOAST_CORE_ASSERT(HasComponent<T>(), "Entity does not has component!");
			return mScene->mRegistry.remove<T>(mEntityHandle);
		}

		operator bool() const { return mEntityHandle != entt::null; }
	private:
		entt::entity mEntityHandle{ entt::null };
		Scene* mScene = nullptr;
	};
}