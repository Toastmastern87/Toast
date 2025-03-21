#pragma once

#include <sstream>
#include <windows.h>

#include "Toast/Core/Base.h"
#include "Toast/Events/Event.h"

namespace Toast
{
	struct WindowProps
	{
		std::string Title;
		std::string IconStr;
		uint32_t Width;
		uint32_t Height;

		WindowProps(const std::string& title = "Toaster", 
					const std::string& iconStr = "",
					uint32_t width = 2400,
					uint32_t height = 1300)
			: Title(title), IconStr(iconStr), Width(width), Height(height)
		{
		}
	};

	//Interface for a windows window
	class Window
	{
	public:
		using EventCallbackFn = std::function<void(Event&)>;

		virtual ~Window() = default;

		virtual void OnUpdate() = 0;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;

		// Window attributes
		virtual void SetEventCallback(const EventCallbackFn& callback) = 0;
		virtual void SetVSync(bool enabled) = 0;
		virtual bool IsVSync() const = 0;

		virtual void SetTitle(const std::string& title) = 0;
		virtual void SetIcon(const std::string& iconPath) = 0;

		virtual void SetDragOnGoing(bool drag) = 0;
		virtual POINT GetDeltaDrag() = 0;

		static Scope<Window> Create(const WindowProps& props = WindowProps());

		virtual void* GetNativeWindow() const = 0;
	};
}