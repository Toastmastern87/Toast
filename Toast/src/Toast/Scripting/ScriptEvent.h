#pragma once

#include <cstdint>

#include "Toast/Events/Event.h"
#include "Toast/Events/KeyEvent.h"
#include "Toast/Events/MouseEvent.h"

namespace Toast {

	struct ScriptEvent 
	{
		EventType Type = EventType::None;

		uint32_t Key = 0;
		uint32_t RepeatCount = 0;
		uint32_t MouseButton = 0;

		float MouseX = 0.0f;
		float MouseY = 0.0f;
		float ScrollDelta = 0.0f;
	};

	inline ScriptEvent MakeScriptEvent(const KeyPressedEvent& e)
	{
		ScriptEvent se;
		se.Type = EventType::KeyPressed;
		se.Key = (uint32_t)e.GetKeyCode();
		se.RepeatCount = (uint32_t)e.GetRepeatCount();

		return se;
	}

	inline ScriptEvent MakeScriptEvent(const KeyReleasedEvent& e)
	{
		ScriptEvent se;
		se.Type = EventType::KeyReleased;
		se.Key = (uint32_t)e.GetKeyCode();

		return se;
	}

	inline ScriptEvent MakeScriptEvent(const MouseButtonPressedEvent& e)
	{
		ScriptEvent se;
		se.Type = EventType::MouseButtonPressed;
		se.MouseButton = (uint32_t)e.GetMouseButton();

		return se;
	}

	inline ScriptEvent MakeScriptEvent(const MouseButtonReleasedEvent& e)
	{
		ScriptEvent se;
		se.Type = EventType::MouseButtonReleased;
		se.MouseButton = (uint32_t)e.GetMouseButton();

		return se;
	}

}