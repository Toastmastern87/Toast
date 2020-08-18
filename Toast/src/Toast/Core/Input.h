#pragma once

#include "Toast/Core/Core.h"
#include "Toast/Core/KeyCodes.h"
#include "Toast/Core/MouseCodes.h"

namespace Toast
{
	class Input
	{
	protected:
		Input() = default;
	public:
		Input(const Input&) = delete;
		Input& operator=(const Input&) = delete;

		inline static bool IsKeyPressed(KeyCode key) { return sInstance->IsKeyPressedImpl(key); }

		inline static bool IsMouseButtonPressed(MouseCode button) { return sInstance->IsMouseButtonPressedImpl(button); }
		inline static std::pair<float, float> GetMousePosition() { return sInstance->GetMousePositionImpl(); }
		inline static float GetMouseX() { return sInstance->GetMouseXImpl(); }
		inline static float GetMouseY() { return sInstance->GetMouseYImpl(); }

		static Scope<Input> Create();
	protected:
		virtual bool IsKeyPressedImpl(KeyCode key) = 0;

		virtual bool IsMouseButtonPressedImpl(MouseCode button) = 0;
		virtual std::pair<float, float> GetMousePositionImpl() = 0;
		virtual float GetMouseXImpl() = 0;
		virtual float GetMouseYImpl() = 0;

	private:
		static Scope<Input> sInstance;
	};
}
