#include "tpch.h"
#include "Toast/Core/Input.h"

#include "Toast/Core/Application.h"

namespace Toast {

	static std::unordered_map<MouseCode, bool> sPreviousMouseState;

	bool Input::IsKeyPressed(const KeyCode keycode)
	{
		auto state = GetAsyncKeyState(static_cast<int>(keycode));

		return (state & 0x8000);
	}

	bool Input::IsMouseButtonPressed(const MouseCode button)
	{
		auto state = GetAsyncKeyState(static_cast<int>(button));

		sPreviousMouseState[button] = state;

		return (state & 0x8000);
	}

	bool Input::IsMouseButtonReleased(const MouseCode button)
	{
		// Get current state (true if pressed)
		bool isPressed = (GetAsyncKeyState(static_cast<int>(button)) & 0x8000) != 0;

		// Check if it was pressed in the previous frame and now it's not
		bool wasReleased = sPreviousMouseState[button] && !isPressed;

		// Update the stored state for the next frame
		sPreviousMouseState[button] = isPressed;

		return wasReleased;
	}

	DirectX::XMFLOAT2 Input::GetMousePosition()
	{
		POINT p;

		GetCursorPos(&p);

		return { (float)p.x, (float)p.y };
	}

	float Input::GetMouseX()
	{
		return GetMousePosition().x;
	}

	float Input::GetMouseY()
	{
		return GetMousePosition().y;
	}

	float Input::sMouseWheelDelta;

	float Input::GetMouseWheelDelta() 
	{
		return sMouseWheelDelta;
	}

	void Input::SetMouseWheelDelta(float delta)
	{
		sMouseWheelDelta = delta;
	}
}