#include "tpch.h"
#include "WindowsWindow.h"

#include "Toast/Events/ApplicationEvent.h"
#include "Toast/Events/KeyEvent.h"
#include "Toast/Events/MouseEvent.h"

#include "imgui.h"
IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Toast 
{
	static bool sWin32Initialized = false;

	HINSTANCE hInstance;

	static void Win32ErrorCallback(int error, const char* description) 
	{
		TOAST_CORE_ERROR("Win32 Error ({0}): {1}", error, description);
	}

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

		hInstance = GetModuleHandle(0);

		WNDCLASSEX wc = {};
		wc.lpfnWndProc = WindowProc;
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

		if (!RegisterClassEx(&wc)) 
		{
			TOAST_CORE_ERROR("Could not initialize the window class!");
		}

		mWin32Window = CreateWindowEx(0, wc.lpszClassName, mData.Title.c_str(), WS_OVERLAPPEDWINDOW, 0, 0, mData.Width, mData.Height, NULL, NULL, hInstance, NULL);

		if (!sWin32Initialized)
		{
			TOAST_CORE_ASSERT(mWin32Window, "Could not initialize Win32!");

			sWin32Initialized = true;
		}

		SetWindowLongPtr(mWin32Window, 0, (LONG_PTR)&mData);
		ShowWindow(mWin32Window, SW_SHOW);
		SetFocus(mWin32Window);
	}

	void WindowsWindow::Shutdown() 
	{
		DestroyWindow(mWin32Window);
	}

	void WindowsWindow::OnUpdate() 
	{
		MSG message;

		while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE) > 0)
		{
			TranslateMessage(&message);
			DispatchMessage(&message);
		}
	}

	void WindowsWindow::SetVSync(bool enabled)
	{
		mData.VSync = enabled;
	}

	bool WindowsWindow::IsVSync() const 
	{
		return mData.VSync;
	}

	LRESULT CALLBACK WindowsWindow::WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		LRESULT result = NULL;

		if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
			return true;

		switch (msg)
		{
			case WM_SIZING:
			{
				RECT rect = *((PRECT)lParam);

				WindowData* data = (WindowData*)GetWindowLongPtr(hWnd, 0);
				data->Width = (unsigned int)(rect.right - rect.left);
				data->Height = (unsigned int)(rect.bottom - rect.top);

				WindowResizeEvent event((unsigned int)(rect.right - rect.left), (unsigned int)(rect.bottom - rect.top));
				data->EventCallback(event);
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

				KeyReleasedEvent event(static_cast<int>(wParam));
				data->EventCallback(event);
				break;
			}
			case WM_CHAR:
			{
				WindowData* data = (WindowData*)GetWindowLongPtr(hWnd, 0);

				KeyTypedEvent event(static_cast<int>(wParam));
				data->EventCallback(event);
				break;
			}
			case WM_KEYDOWN:
			{
				WindowData* data = (WindowData*)GetWindowLongPtr(hWnd, 0);
				int repeatCount = (lParam & 0xffff);

				KeyPressedEvent event(static_cast<int>(wParam), repeatCount);
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

				MouseScrolledEvent event(GET_WHEEL_DELTA_WPARAM(wParam));
				data->EventCallback(event);
				break;
			}
			case WM_LBUTTONDOWN:
			{
				WindowData* data = (WindowData*)GetWindowLongPtr(hWnd, 0);

				MouseButtonPressedEvent event(VK_LBUTTON);
				data->EventCallback(event);
				break;
			}
			case WM_LBUTTONUP:
			{
				WindowData* data = (WindowData*)GetWindowLongPtr(hWnd, 0);

				MouseButtonReleasedEvent event(VK_LBUTTON);
				data->EventCallback(event);
				break;
			}
			case WM_MBUTTONDOWN:
			{
				WindowData* data = (WindowData*)GetWindowLongPtr(hWnd, 0);

				MouseButtonPressedEvent event(VK_MBUTTON);
				data->EventCallback(event);
				break;
			}
			case WM_MBUTTONUP:
			{
				WindowData* data = (WindowData*)GetWindowLongPtr(hWnd, 0);

				MouseButtonReleasedEvent event(VK_MBUTTON);
				data->EventCallback(event);
				break;
			}
			case WM_RBUTTONDOWN:
			{
				WindowData* data = (WindowData*)GetWindowLongPtr(hWnd, 0);

				MouseButtonPressedEvent event(VK_RBUTTON);
				data->EventCallback(event);
				break;
			}
			case WM_RBUTTONUP:
			{
				WindowData* data = (WindowData*)GetWindowLongPtr(hWnd, 0);

				MouseButtonReleasedEvent event(VK_RBUTTON);
				data->EventCallback(event);
				break;
			}
			default:
				result = DefWindowProc(hWnd, msg, wParam, lParam);
		}

		return result; 
	}
}