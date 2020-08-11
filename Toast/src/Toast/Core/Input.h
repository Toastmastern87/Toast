#pragma once

#include "Toast/Core/Core.h"

namespace Toast
{
	class TOAST_API Input
	{
	protected:
		Input() = default;
	public:
		Input(const Input&) = delete;
		Input& operator=(const Input&) = delete;

		inline static bool IsKeyPressed(int keycode) { return sInstance->IsKeyPressedImpl(keycode); }

		inline static bool IsMouseButtonPressed(int button) { return sInstance->IsMouseButtonPressedImpl(button); }
		inline static std::pair<float, float> GetMousePosition() { return sInstance->GetMousePositionImpl(); }
		inline static float GetMouseX() { return sInstance->GetMouseXImpl(); }
		inline static float GetMouseY() { return sInstance->GetMouseYImpl(); }
	protected:
		virtual bool IsKeyPressedImpl(int keycode) = 0;

		virtual bool IsMouseButtonPressedImpl(int button) = 0;
		virtual std::pair<float, float> GetMousePositionImpl() = 0;
		virtual float GetMouseXImpl() = 0;
		virtual float GetMouseYImpl() = 0;

	private:
		static Scope<Input> sInstance;
	};
}
