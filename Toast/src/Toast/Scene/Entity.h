#pragma once

#include "Scene.h"

#include "Components.h"

#include "entt.hpp"

namespace Toast {

	class Entity 
	{
	public:
		Entity() = default;
		Entity(entt::entity handle, Scene* scene)
			: mEntityHandle(handle), mScene(scene) {}
		Entity(const Entity& other) = default;

		template<typename T, typename... Args>
		T& AddComponent(Args&&... args) 
		{
			TOAST_CORE_ASSERT(!HasComponent<T>(), "Entity already has component!");
			T& component = mScene->mRegistry.emplace<T>(mEntityHandle, std::forward<Args>(args)...);
			mScene->OnComponentAdded<T>(*this, component);
			return component;
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

		operator bool() const { return (mEntityHandle != entt::null) && mScene; }
		operator entt::entity() const { return mEntityHandle; }
		operator uint32_t() const { return (uint32_t)mEntityHandle; }
		
		bool operator==(const Entity& other) const 
		{
			return mEntityHandle == other.mEntityHandle && mScene == other.mScene; 
		}

		bool operator!=(const Entity& other) const
		{
			return !(*this == other);
		}

		bool RemoveChild(Entity child)
		{
			UUID childID = child.GetUUID();
			std::vector<UUID>& children = Children();
			auto it = std::find(children.begin(), children.end(), childID);
			if (it != children.end())
			{
				children.erase(it);

				return true;
			}

			return false;
		}

		void SetParentUUID(UUID parent) { GetComponent<RelationshipComponent>().ParentHandle = parent; }
		UUID GetParentUUID() { return GetComponent<RelationshipComponent>().ParentHandle; }
		std::vector<UUID>& Children() { return GetComponent<RelationshipComponent>().Children; }

		bool HasParent() { return GetParentUUID(); }

		UUID GetUUID() { return GetComponent<IDComponent>().ID; }

		UUID GetSceneUUID() { return mScene->GetUUID(); }
	private:
		entt::entity mEntityHandle{ entt::null };
		Scene* mScene = nullptr;

		friend class Scene;
		friend class SceneHierarchyPanel;
		friend class PropertiesPanel;
		friend class ScriptEngine;
	};
}