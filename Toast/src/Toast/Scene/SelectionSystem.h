#pragma once
#include "Entity.h"
#include <vector>

namespace Toast {

	class Scene;

	class SelectionSystem
	{
	public:
		SelectionSystem(Scene* scene) : mScene(scene) {}

		void Select(Entity entity);
		void SelectExclusive(Entity entity);
		void Deselect(Entity entity);
		void ClearSelection();
		bool IsSelected(Entity entity) const;
		std::vector<Entity> GetSelected() const;
	private:
		Scene* mScene;
	};

}