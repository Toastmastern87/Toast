#pragma once

#include "Toast/Core/Window.h"

namespace Toast 
{
	class WindowsWindow : public Window 
	{
	public:
		WindowsWindow(const WindowProps& props);
		virtual ~WindowsWindow();

		void OnUpdate() override;

		unsigned int GetWidth() const override { return mData.Width; }
		unsigned int GetHeight() const override { return mData.Height; }

		void SetEventCallback(const EventCallbackFn& callback) override { mData.EventCallback = callback; }
		void SetVSync(bool enabled) override;
		bool IsVSync() const override;

		virtual void SetTitle(const std::string& title) override;

		virtual void* GetNativeWindow() const override { return mWin32Window; }

		static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	private:
		virtual void Init(const WindowProps& props);
		virtual void Shutdown();

	private:
		HWND mWin32Window;

		struct WindowData 
		{
			std::string Title;
			unsigned int Width = 0, Height = 0;
			bool VSync = true;

			EventCallbackFn EventCallback;
		};

		WindowData mData;
	};
}