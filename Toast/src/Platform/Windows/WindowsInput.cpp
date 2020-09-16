#include "tpch.h"
#include "Toast/Core/Input.h"

#include "Toast/Core/Application.h"

namespace Toast {

	bool Input::IsKeyPressed(const KeyCode keycode)
	{
		auto state = GetAsyncKeyState(static_cast<int>(keycode));

		return (state & 0x8000);
	}
	bool Input::IsMouseButtonPressed(const MouseCode button)
	{
		auto state = GetAsyncKeyState(static_cast<int>(button));

		return (state & 0x8000);
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
}