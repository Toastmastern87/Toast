#pragma once

#include "Toast/Core/UUID.h"

#include "Toast/Events/Event.h"

#include "Toast/Scene/Scene.h"

namespace Toast {

	class SceneManager
	{
	private:
		struct SceneManagerData 
		{
			std::unordered_map<UUID, Scope<Scene>> Scenes;
			Scene* ActiveScene = nullptr;
		};

	protected:
		static Scope<SceneManagerData> sSceneManagerData;
	public:
		static void Init();
		static void Shutdown();
		static bool IsShuttingDown() { return sIsShuttingDown; }

		static void OnEvent(Event& e);
		static Scene* AddScene();
		static Scene* AddScene(Scope<Scene> scene);
		static void RemoveScene(UUID sceneID);

		static Scene* GetActiveScene() { return sSceneManagerData->ActiveScene; }
		static void SetActiveScene(Scene* scene);
	private:
		inline static bool sIsShuttingDown = false;
	};

}