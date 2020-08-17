#include "tpch.h"
#include "Platform/Windows/WindowsInput.h"

#include "Toast/Core/Application.h"

namespace Toast 
{
	Scope<Input> Input::sInstance = CreateScope<WindowsInput>();

	bool WindowsInput::IsKeyPressedImpl(int keycode)
	{
		auto state = GetAsyncKeyState(keycode);

		return (state & 0x8000);
	}
	bool WindowsInput::IsMouseButtonPressedImpl(int button)
	{
		auto state = GetAsyncKeyState(button);

		return (state & 0x8000);
	}

	std::pair<float, float> WindowsInput::GetMousePositionImpl()
	{
		POINT p;

		GetCursorPos(&p);

		return { (float)p.x, (float)p.y };
	}

	float WindowsInput::GetMouseXImpl()
	{
		auto [x, y] = GetMousePositionImpl();

		return x;
	}
	float WindowsInput::GetMouseYImpl()
	{
		auto [x, y] = GetMousePositionImpl();

		return y;
	}
}