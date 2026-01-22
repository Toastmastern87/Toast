#pragma once

namespace Toast {
	class Scene;

	struct ISceneProvider
	{
	public:
		virtual ~ISceneProvider() = default;
		virtual Scene* GetActiveScene() = 0;

		virtual void RequestSceneChange(const std::string& sceneName) = 0;
	};
}