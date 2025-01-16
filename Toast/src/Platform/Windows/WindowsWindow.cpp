#include "tpch.h"
#include "WindowsWindow.h"

#include "Toast/Core/Application.h"
#include "Toast/Core/Input.h"

#include "Toast/Renderer/Renderer.h"

#include "Toast/Events/ApplicationEvent.h"
#include "Toast/Events/KeyEvent.h"
#include "Toast/Events/MouseEvent.h"

#include "imgui.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Toast 
{
	static bool sWin32Initialized = false;

	HINSTANCE hInstance;

	static void Win32ErrorCallback(int error, const char* description) 
	{
		TOAST_CORE_ERROR("Win32 Error (%d): {%s}", error, description);
	}

	WindowsWindow::WindowsWindow(const WindowProps& props) 
	{
		TOAST_PROFILE_FUNCTION();

		Init(props);
	}

	WindowsWindow::~WindowsWindow()
	{
		TOAST_PROFILE_FUNCTION();

		Shutdown();
	}

	void WindowsWindow::Init(const WindowProps& props) 
	{
		TOAST_PROFILE_FUNCTION();

		mData.Title = props.Title;
		mData.Width = props.Width;
		mData.Height = props.Height;

		TOAST_CORE_INFO("Creating window %s %dx%d", props.Title.c_str(), props.Width, props.Height);

		hInstance = GetModuleHandle(0);

		WNDCLASSEX wc = {};
		wc.lpfnWndProc = WindowProc;
		wc.style = CS_CLASSDC;
		wc.hInstance = hInstance;
		wc.lpszClassName = "Toast Win32 Window";
		wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
		wc.hCursor = LoadCursor(NULL, IDC_ARROW);
		wc.hIcon = LoadIcon(NULL, IDI_WINLOGO);
		wc.hIconSm = wc.hIcon;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = sizeof(WindowData*);
		wc.lpszMenuName = NULL;
		wc.cbSize = sizeof(WNDCLASSEX);

		wc.hIcon = (HICON)LoadImage(
			nullptr,
			props.IconStr.c_str(),// File path to your .ico
			IMAGE_ICON,
			32,                   // Desired icon width
			32,                   // Desired icon height
			LR_LOADFROMFILE | LR_DEFAULTCOLOR
		);

		if (!wc.hIcon)
		{
			// Grab the error code
			DWORD error = GetLastError();

			// Format a readable error message
			LPVOID lpMsgBuf = nullptr;
			FormatMessage(
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				nullptr,
				error,
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				(LPTSTR)&lpMsgBuf,
				0,
				nullptr
			);

			// lpMsgBuf now points to a string with the error text.
			// You can log it, display it in a MessageBox, etc. For example:
			if (lpMsgBuf)
			{
				MessageBox(nullptr, (LPCTSTR)lpMsgBuf, TEXT("LoadImage Failed"), MB_OK | MB_ICONERROR);

				// Free the buffer allocated by FormatMessage
				LocalFree(lpMsgBuf);
			}
		}

		if (!wc.hIcon)
			TOAST_CORE_CRITICAL("Icon not loaded correctly");

		if (!RegisterClassEx(&wc)) 
		{
			TOAST_CORE_ERROR("Could not initialize the window class!");
		}

		mWin32Window = CreateWindow(wc.lpszClassName, mData.Title.c_str(), WS_OVERLAPPEDWINDOW, 0, 0, mData.Width, mData.Height, NULL, NULL, wc.hInstance, NULL);

		if (!sWin32Initialized)
		{
			TOAST_CORE_ASSERT(mWin32Window, "Could not initialize Win32!");

			sWin32Initialized = true;
		}

		SetWindowLongPtr(mWin32Window, 0, (LONG_PTR)&mData);
		ShowWindow(mWin32Window, SW_SHOWDEFAULT);
		UpdateWindow(mWin32Window);
		SetFocus(mWin32Window);

		mData.IsInitialized = true;
	}

	void WindowsWindow::Shutdown() 
	{
		TOAST_PROFILE_FUNCTION();

		DestroyWindow(mWin32Window);
	}

	void WindowsWindow::OnUpdate() 
	{
		TOAST_PROFILE_FUNCTION();

		MSG message;

		while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE) > 0)
		{
			TranslateMessage(&message);
			DispatchMessage(&message);
		}

		RenderCommand::SwapBuffers(mData.VSync);
	}

	void WindowsWindow::SetVSync(bool enabled)
	{
		mData.VSync = enabled;
	}

	bool WindowsWindow::IsVSync() const 
	{
		return mData.VSync;
	}

	void WindowsWindow::SetTitle(const std::string& title)
	{
		mData.Title = title;
		SetWindowText(mWin32Window, mData.Title.c_str());
	}

	void WindowsWindow::SetIcon(const std::string& iconPath)
	{
		HICON hIcon = (HICON)LoadImage(
			nullptr,               // Not loading from resources
			iconPath.c_str(),      // Path to the .ico file
			IMAGE_ICON,
			32,                    // Width
			32,                    // Height
			LR_LOADFROMFILE | LR_DEFAULTCOLOR
		);

		// If either of these fails, you may want to handle it (fallback to default, etc.)
		if (!hIcon)
		{
			// As a fallback, load a default icon or just return
			hIcon = LoadIcon(nullptr, IDI_WINLOGO);
		}

		// Set the large icon
		SendMessage(mWin32Window, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
	}

	LRESULT CALLBACK WindowsWindow::WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		LRESULT result = NULL;

		if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
			return true;

		switch (msg)
		{
			case WM_SIZE: 
			{
				WindowData* data = (WindowData*)GetWindowLongPtr(hWnd, 0);
				if (data->IsInitialized)
				{
					data->Width = LOWORD(lParam);
					data->Height = HIWORD(lParam);

					WindowResizeEvent event((UINT)LOWORD(lParam), (UINT)HIWORD(lParam));
					data->EventCallback(event);
				}

				break;
			}
			case WM_CLOSE:
			case WM_DESTROY:
			{
				WindowData* data = (WindowData*)GetWindowLongPtr(hWnd, 0);

				WindowCloseEvent event;
				data->EventCallback(event);
				break;
			}
			case WM_KEYUP:
			{
				WindowData* data = (WindowData*)GetWindowLongPtr(hWnd, 0);

				KeyReleasedEvent event(static_cast<KeyCode>(wParam));
				data->EventCallback(event);
				break;
			}
			case WM_CHAR:
			{
				WindowData* data = (WindowData*)GetWindowLongPtr(hWnd, 0);

				KeyTypedEvent event(static_cast<KeyCode>(wParam));
				data->EventCallback(event);
				break;
			}
			case WM_KEYDOWN:
			{
				WindowData* data = (WindowData*)GetWindowLongPtr(hWnd, 0);
				int repeatCount = (lParam & 0xffff);

				KeyPressedEvent event(static_cast<KeyCode>(wParam), repeatCount);
				data->EventCallback(event);
				break;
			}
			case WM_MOUSEMOVE:
			{
				WindowData* data = (WindowData*)GetWindowLongPtr(hWnd, 0);

				MouseMovedEvent event((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam));
				data->EventCallback(event) ;
				break;
			}
			case WM_MOUSEWHEEL:
			{
				WindowData* data = (WindowData*)GetWindowLongPtr(hWnd, 0);

				MouseScrolledEvent event((float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA);
				data->EventCallback(event);
				break;
			}
			case WM_LBUTTONDOWN:
			{
				WindowData* data = (WindowData*)GetWindowLongPtr(hWnd, 0);

				MouseButtonPressedEvent event(static_cast<MouseCode>(VK_LBUTTON));
				data->EventCallback(event);
				break;
			}
			case WM_LBUTTONUP:
			{
				WindowData* data = (WindowData*)GetWindowLongPtr(hWnd, 0);

				MouseButtonReleasedEvent event(static_cast<MouseCode>(VK_LBUTTON));
				data->EventCallback(event);
				break;
			}
			case WM_MBUTTONDOWN:
			{
				WindowData* data = (WindowData*)GetWindowLongPtr(hWnd, 0);

				MouseButtonPressedEvent event(static_cast<MouseCode>(VK_MBUTTON));
				data->EventCallback(event);
				break;
			}
			case WM_MBUTTONUP:
			{
				WindowData* data = (WindowData*)GetWindowLongPtr(hWnd, 0);

				MouseButtonReleasedEvent event(static_cast<MouseCode>(VK_MBUTTON));
				data->EventCallback(event);
				break;
			}
			case WM_RBUTTONDOWN:
			{
				WindowData* data = (WindowData*)GetWindowLongPtr(hWnd, 0);

				MouseButtonPressedEvent event(static_cast<MouseCode>(VK_RBUTTON));
				data->EventCallback(event);
				break;
			}
			case WM_RBUTTONUP:
			{
				WindowData* data = (WindowData*)GetWindowLongPtr(hWnd, 0);

				MouseButtonReleasedEvent event(static_cast<MouseCode>(VK_RBUTTON));
				data->EventCallback(event);
				break;
			}
			default:
				result = DefWindowProc(hWnd, msg, wParam, lParam);
		}

		return result; 
	}
}