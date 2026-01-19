#include "tpch.h"
#include "SceneManager.h"

namespace Toast {

	Scope<SceneManager::SceneManagerData> SceneManager::sSceneManagerData;

	void SceneManager::Init()
	{
		sSceneManagerData = CreateScope<SceneManagerData>();
	}

	void SceneManager::Shutdown()
	{
		sIsShuttingDown = true;

		sSceneManagerData.reset();
	}

	void SceneManager::OnEvent(Event& e)
	{
		if (sSceneManagerData->ActiveScene)
			sSceneManagerData->ActiveScene->OnEvent(e);
	}

	Scene* SceneManager::AddScene(Scope<Scene> scene)
	{
		if (!scene) 
			return nullptr;
		
		UUID id = scene->GetUUID();

		if (!sSceneManagerData->ActiveScene)
			sSceneManagerData->ActiveScene = scene.get();

		sSceneManagerData->Scenes[id] = std::move(scene);

		return sSceneManagerData->Scenes[id].get();
	}

	Scene* SceneManager::AddScene()
	{
		Scope<Scene> newScene = CreateScope<Scene>();
		
		return AddScene(std::move(newScene));
	}

	void SceneManager::RemoveScene(UUID sceneID)
	{
		sSceneManagerData->Scenes.erase(sceneID);
	}

	void SceneManager::SetActiveScene(Scene* scene)
	{
		if (scene && sSceneManagerData->Scenes.find(scene->GetUUID()) != sSceneManagerData->Scenes.end())
			sSceneManagerData->ActiveScene = scene;
	}

}