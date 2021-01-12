#pragma once

#include "entt.hpp"
#include "Toast/Core/Timestep.h"
#include "Toast/Renderer/PerspectiveCamera.h"
#include "Toast/Renderer/SceneEnvironment.h"

namespace Toast {

	class Entity;

	class Scene 
	{
	public:
		Scene();
		~Scene();

		Entity CreateEntity(const std::string& name = std::string());
		void DestroyEntity(Entity entity);

		Entity CreateCube(const std::string& name = std::string());
		Entity CreateSphere(const std::string& name = std::string());

		void OnUpdateRuntime(Timestep ts);
		void OnUpdateEditor(Timestep ts, const Ref<PerspectiveCamera> perspectiveCamera);
		void OnViewportResize(uint32_t width, uint32_t height);

		//Settings
		struct Settings
		{
			enum class Wireframe { NO = 0, YES = 1, ONTOP = 2 };
			Wireframe WireframeRendering = Wireframe::NO;
		};
		struct Stats
		{
			float timesteps = 0.0f;
			float FPS;
			uint32_t VerticesCount = 0;
		};
	private:
		template<typename T>
		void OnComponentAdded(Entity entity, T& component);
	private:
		entt::registry mRegistry;
		uint32_t mViewportWidth = 0, mViewportHeight = 0;
		Ref<Environment> mSceneEnvironment;

		Settings mSettings;
		Stats mStats;

		DirectX::XMVECTOR mOldCameraPos = { 0.0f, 0.0f, 0.0f }, mOldCameraRot = { 0.0f, 0.0f, 0.0f }, mOldCameraScale = { 0.0f, 0.0f, 0.0f };

		friend class Entity;
		friend class SceneSerializer;
		friend class SceneHierarchyPanel;
		friend class SceneSettingsPanel;
	};
}
	