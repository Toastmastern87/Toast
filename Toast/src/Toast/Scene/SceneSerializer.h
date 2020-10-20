#pragma once

#include "Toast\Core\Base.h"

#include "Scene.h"

namespace Toast {

	class SceneSerializer 
	{
	public:
		SceneSerializer(const Ref<Scene>& scene);

		void SerializeScene(const std::string& filepath);

		bool DeserializeScene(const std::string& filepath);
	private:
		Ref<Scene> mScene;
	};
}