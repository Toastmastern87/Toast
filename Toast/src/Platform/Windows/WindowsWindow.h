#pragma once

#include "Toast/Window.h"

namespace Toast 
{
	class WindowsWindow : public Window 
	{
	public:
		WindowsWindow(const WindowProps& props);
		virtual ~WindowsWindow();

		void Start() override;
		void End() override;

		inline unsigned int GetWidth() const override { return mData.Width; }
		inline unsigned int GetHeight() const override { return mData.Height; }

		inline void SetEventCallback(const EventCallbackFn& callback) override { mData.EventCallback = callback; }
		void SetVSync(bool enabled) override;
		bool IsVSync() const override;

		void OnResize() const override;

		inline virtual HWND GetNativeWindow() const override { return mWin32Window; }
		inline virtual GraphicsContext* GetGraphicsContext() const override { return mContext; }

		static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	private:
		virtual void Init(const WindowProps& props);
		virtual void Shutdown();

	private:
		HWND mWin32Window;
		GraphicsContext* mContext;

		struct WindowData 
		{
			std::string Title;
			unsigned int Width = 0, Height = 0;
			bool VSync = false;

			EventCallbackFn EventCallback;
		};

		WindowData mData;
	};
}