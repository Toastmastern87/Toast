#include "tpch.h"
#include "SelectionSystem.h"
#include "Scene.h"
#include "Components.h"

namespace Toast {

	void SelectionSystem::Select(Entity entity)
	{
		if (entity && !entity.HasComponent<SelectedComponent>())
			entity.AddComponent<SelectedComponent>();
	}

	void SelectionSystem::SelectExclusive(Entity entity)
	{
		ClearSelection();
		Select(entity);
	}

	void SelectionSystem::Deselect(Entity entity)
	{
		if (entity && entity.HasComponent<SelectedComponent>())
			entity.RemoveComponents<SelectedComponent>();
	}

	void SelectionSystem::ClearSelection()
	{
		mScene->mRegistry.clear<SelectedComponent>();
	}

	bool SelectionSystem::IsSelected(Entity entity) const
	{
		return entity && entity.HasComponent<SelectedComponent>();
	}

	std::vector<Toast::Entity> SelectionSystem::GetSelected() const
	{
		std::vector<Entity> result;
		auto view = mScene->mRegistry.view<SelectedComponent>();
		for (auto e : view)
			result.emplace_back(e, mScene);
		return result;
	}

}