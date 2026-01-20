#pragma once

namespace Toast {
	class Scene;

	struct ISceneProvider
	{
		virtual ~ISceneProvider() = default;
		virtual Scene* GetActiveScene() = 0;
	};
}
