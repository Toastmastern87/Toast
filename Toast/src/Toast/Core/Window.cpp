#include "tpch.h"
#include "Toast/Core/Window.h"

#ifdef TOAST_PLATFORM_WINDOWS
	#include "Platform/Windows/WindowsWindow.h"
#endif

namespace Toast {

	Scope<Window> Window::Create(const WindowProps& props)
	{
		#ifdef TOAST_PLATFORM_WINDOWS
			return CreateScope<WindowsWindow>(props);
		#else
			TOAST_CORE_ASSERT(false, "Unkown platform!");
			return nullptr;
		#endif
	}
}