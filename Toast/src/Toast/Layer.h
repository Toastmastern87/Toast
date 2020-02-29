#pragma once

#include "Toast/Core.h"
#include "Toast/Events/Event.h"

namespace Toast
{
	class TOAST_API Layer
	{
	public:
		Layer(const std::string& name = "Layer");
		virtual ~Layer();

		virtual void OnAttach() {}
		virtual void OnDetach() {}
		virtual void OnUpdate() {}
		virtual void OnImGuiRender() {}
		virtual void OnEvent(Event& e) {}

		inline const std::string& GetName() { return mDebugName; }
	protected:
		std::string mDebugName;
	};
}