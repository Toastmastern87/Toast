#pragma once

#include "Toast/Core/Base.h"
#include "Toast/Core/Timestep.h"
#include "Platform/Windows/WindowsWindow.h"

#include "Toast/Events/Event.h"

namespace Toast
{
	class Layer
	{
	public:
		Layer(const std::string& name = "Layer", WindowsWindow* window = nullptr);
		virtual ~Layer() = default;

		virtual void OnAttach() {}
		virtual void OnDetach() {}
		virtual void OnUpdate(Timestep ts) {}
		virtual void OnImGuiRender() {}
		virtual void OnEvent(Event& e) {}

		const std::string& GetName() { return mDebugName; }
	protected:
		std::string mDebugName;
	};
}