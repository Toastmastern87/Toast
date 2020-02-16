#include "tpch.h"
#include "WindowsWindow.h"

namespace Toast 
{
	static bool sGLFWInitialized = false;

	Window* Window::Create(const WindowProps& props) 
	{
		return new WindowsWindow(props);
	}

	WindowsWindow::WindowsWindow(const WindowProps& props) 
	{
		Init(props);
	}

	WindowsWindow::~WindowsWindow()
	{
		Shutdown();
	}

	void WindowsWindow::Init(const WindowProps& props) 
	{
		mData.Title = props.Title;
		mData.Width = props.Width;
		mData.Height = props.Height;

		TOAST_CORE_INFO("Creating window {0} {1} {2}", props.Title, props.Width, props.Height);

		if (!sGLFWInitialized)
		{
			int success = glfwInit();
			TOAST_CORE_ASSERT(success, "Could not initialize Win32!");

			sGLFWInitialized = true;
		}

		mWindow = glfwCreateWindow((int)props.Width, (int)props.Height, mData.Title.c_str(), nullptr, nullptr);
		glfwMakeContextCurrent(mWindow);
		glfwSetWindowUserPointer(mWindow, &mData);
		SetVSync(true);
	}

	void WindowsWindow::Shutdown() 
	{
		glfwDestroyWindow(mWindow);
	}

	void WindowsWindow::OnUpdate() 
	{
		glfwPollEvents();
		glfwSwapBuffers(mWindow);
	}

	void WindowsWindow::SetVSync(bool enabled)
	{
		if (enabled) 
		{
			glfwSwapInterval(1);
		}
		else
		{
			glfwSwapInterval(0);
		}

		mData.VSync = enabled;
	}

	bool WindowsWindow::IsVSync() const 
	{
		return mData.VSync;
	}
}