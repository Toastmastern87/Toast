#pragma once

#include <DirectXMath.h>

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
		static DirectX::XMFLOAT2 GetMousePosition();
		static float GetMouseX();
		static float GetMouseY();
		static float GetMouseWheelDelta();
		static void SetMouseWheelDelta(float delta);

	public:
		static float sMouseWheelDelta;
	};
}
