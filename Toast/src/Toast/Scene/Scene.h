#pragma once

#include "entt.hpp"
#include "Toast/Core/Timestep.h"
#include "Toast/Renderer/PerspectiveCamera.h"

namespace Toast {

	class Entity;

	class Scene 
	{
	public:
		Scene();
		~Scene();

		Entity CreateEntity(const std::string& name = std::string());
		void DestroyEntity(Entity entity);

		void OnUpdateRuntime(Timestep ts);
		void OnUpdateEditor(Timestep ts, const Ref<PerspectiveCamera> perspectiveCamera);
		void OnViewportResize(uint32_t width, uint32_t height);

		void RenderGrid(bool renderGrid) { mSettings.GridActivated = renderGrid; }

		//Settings
		struct Settings
		{
			bool GridActivated = true;

			enum class Wireframe { NO = 0, YES = 1, ONTOP = 2 };
			Wireframe WireframeRendering = Wireframe::NO;
		};
	private:
		template<typename T>
		void OnComponentAdded(Entity entity, T& component);
	private:
		entt::registry mRegistry;
		uint32_t mViewportWidth = 0, mViewportHeight = 0;

		Settings mSettings;

		friend class Entity;
		friend class SceneSerializer;
		friend class SceneHierarchyPanel;
		friend class SceneSettingsPanel;
	};
}
	