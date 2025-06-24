#pragma once

#include "Toast/Renderer/PlanetSystem.h"

namespace Toast {

	class PlanetPanel
	{
	public:
		PlanetPanel() = default;
		~PlanetPanel() = default;

		void OnImGuiRender();
	};

}