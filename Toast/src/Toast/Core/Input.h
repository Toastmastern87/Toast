#pragma once

#include "Toast/Core/Base.h"
#include "Toast/Core/KeyCodes.h"
#include "Toast/Core/MouseCodes.h"

namespace Toast
{
	class Input
	{
	public:
		static bool IsKeyPressed(KeyCode key);

		static bool IsMouseButtonPressed(MouseCode button);
		static std::pair<float, float> GetMousePosition();
		static float GetMouseX();
		static float GetMouseY();
	};
}
