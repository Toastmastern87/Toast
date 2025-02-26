#pragma once

#include "Entity.h"

namespace Toast {

	class Prefab
	{
	public:
		Prefab();
		~Prefab() = default;

		void Create(Entity entity, std::string& name);
	private:
		Entity CreatePrefabFromEntity(Entity entity);
	private:
		Ref<Scene> mScene;
		Entity mEntity;
	};

	class PrefabLibrary
	{
	public:
		static Prefab* Load(Entity entity, std::string& name);
		static bool Exists(std::string& name);
	private:
		static std::unordered_map<std::string, Scope<Prefab>> mPrefabs;
	};

}