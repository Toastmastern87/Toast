#pragma once

#include "Entity.h"

namespace Toast {

	class Prefab
	{
	public:
		Prefab();
		~Prefab() = default;

		void Create(Entity entity, std::string& name);
		Entity LoadFromFile(std::string& name);
		void Update(Entity entity, std::string& name);

		std::vector<Entity> GetEntities() const;
	private:
		void GatherEntities(Entity entity, std::vector<Entity>& outEntities) const;

		Entity CreatePrefabFromEntity(Entity entity);
	private:
		Ref<Scene> mScene;
		Entity mEntity;
	};

	class PrefabLibrary
	{
	public:
		static std::vector<Entity> GetEntities(std::string& name);
		static std::vector<Entity> Load(Entity entity, std::string& name);
		static Prefab* Update(Entity entity, std::string& name);
		static bool Exists(std::string& name);
	private:
		static std::unordered_map<std::string, Scope<Prefab>> mPrefabs;
	};

}