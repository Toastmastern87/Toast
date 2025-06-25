#pragma once

#include "Toast/Renderer/PlanetSystem.h"
#include "Toast/Scene/Scene.h"

namespace Toast {

	class PlanetPanel
	{
	public:
		PlanetPanel() = default;
		~PlanetPanel() = default;

		void OnImGuiRender();

		void SetContext(Scene* context);
	private:
		Scene* mContext;
	};

}