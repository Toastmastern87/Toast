#include "tpch.h"
#include "Toast/Core/Input.h"

#include "Toast/Core/Application.h"

namespace Toast {

	bool Input::IsKeyPressed(KeyCode keycode)
	{
		auto state = GetAsyncKeyState(static_cast<int>(keycode));

		return (state & 0x8000);
	}
	bool Input::IsMouseButtonPressed(MouseCode button)
	{
		auto state = GetAsyncKeyState(static_cast<int>(button));

		return (state & 0x8000);
	}

	std::pair<float, float> Input::GetMousePosition()
	{
		POINT p;

		GetCursorPos(&p);

		return { (float)p.x, (float)p.y };
	}

	float Input::GetMouseX()
	{
		auto [x, y] = GetMousePosition();

		return x;
	}
	float Input::GetMouseY()
	{
		auto [x, y] = GetMousePosition();

		return y;
	}
}