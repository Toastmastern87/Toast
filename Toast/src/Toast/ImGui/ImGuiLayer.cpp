#include "tpch.h"
#include "ImGuiLayer.h"

#include "imgui.h"
#include "Toast/Platform/DirectX/imgui_impl_dx11.h"

#include "Toast/Application.h"
#include "Toast/KeyCodes.h"

#include <sysinfoapi.h>
#include <d3d11.h>

//#pragma comment(lib, "d3d11.lib")

namespace Toast 
{
	static ID3D11Device* g_pd3dDevice = NULL;
	static ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
	static IDXGISwapChain* g_pSwapChain = NULL;
	static ID3D11RenderTargetView* g_mainRenderTargetView = NULL;

	static INT64 sTime = 0;
	static INT64 sTicksPerSecond = 0;

	void CreateRenderTarget()
	{
		ID3D11Texture2D* pBackBuffer;
		g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
		g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
		pBackBuffer->Release();
	}

	void CleanupRenderTarget()
	{
		if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
	}

	HRESULT CreateDeviceD3D(HWND hWnd)
	{
		// Setup swap chain
		DXGI_SWAP_CHAIN_DESC sd;
		ZeroMemory(&sd, sizeof(sd));
		sd.BufferCount = 2;
		sd.BufferDesc.Width = 0;
		sd.BufferDesc.Height = 0;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferDesc.RefreshRate.Numerator = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.OutputWindow = hWnd;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.Windowed = TRUE;
		sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

		UINT createDeviceFlags = 0;
		//createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
		D3D_FEATURE_LEVEL featureLevel;
		const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
		if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
			return E_FAIL;

		CreateRenderTarget();

		return S_OK;
	}

	ImGuiLayer::ImGuiLayer()
		: Layer("ImGuiLayer")
	{
	}

	ImGuiLayer::~ImGuiLayer()
	{
	}

	void ImGuiLayer::OnAttach()
	{
		::QueryPerformanceFrequency((LARGE_INTEGER*)&sTicksPerSecond);
		::QueryPerformanceCounter((LARGE_INTEGER*)&sTime);

		Application& app = Application::Get();	

		if (CreateDeviceD3D(app.GetWindow().GetNativeWindow()) < 0)
			TOAST_ERROR("Error creating the D3D object");

		ImGui::CreateContext();
		ImGui::StyleColorsDark();

		ImGuiIO& io = ImGui::GetIO();
		io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
		io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;

		io.KeyMap[ImGuiKey_Tab] = TOAST_TAB;
		io.KeyMap[ImGuiKey_LeftArrow] = TOAST_LEFT;
		io.KeyMap[ImGuiKey_RightArrow] = TOAST_RIGHT;
		io.KeyMap[ImGuiKey_UpArrow] = TOAST_UP;
		io.KeyMap[ImGuiKey_DownArrow] = TOAST_DOWN;
		io.KeyMap[ImGuiKey_PageUp] = TOAST_PRIOR;
		io.KeyMap[ImGuiKey_PageDown] = TOAST_NEXT;
		io.KeyMap[ImGuiKey_Home] = TOAST_HOME;
		io.KeyMap[ImGuiKey_End] = TOAST_END;
		io.KeyMap[ImGuiKey_Insert] = TOAST_INSERT;
		io.KeyMap[ImGuiKey_Delete] = TOAST_DELETE;
		io.KeyMap[ImGuiKey_Backspace] = TOAST_BACK;
		io.KeyMap[ImGuiKey_Space] = TOAST_SPACE;
		io.KeyMap[ImGuiKey_Enter] = TOAST_RETURN;
		io.KeyMap[ImGuiKey_Escape] = TOAST_ESCAPE;
		io.KeyMap[ImGuiKey_A] = TOAST_A;
		io.KeyMap[ImGuiKey_C] = TOAST_C;
		io.KeyMap[ImGuiKey_V] = TOAST_V;
		io.KeyMap[ImGuiKey_X] = TOAST_X;
		io.KeyMap[ImGuiKey_Y] = TOAST_Y;
		io.KeyMap[ImGuiKey_Z] = TOAST_Z;

		ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
	}

	void ImGuiLayer::OnDetach()
	{
	}

	void ImGuiLayer::OnUpdate()
	{
		ImGuiIO& io = ImGui::GetIO();
		Application& app = Application::Get();
		io.DisplaySize = ImVec2(app.GetWindow().GetWidth(), app.GetWindow().GetHeight());

		INT64 currentTime;
		::QueryPerformanceCounter((LARGE_INTEGER*)&currentTime);
		io.DeltaTime = (float)(currentTime - sTime) / sTicksPerSecond;
		sTime = currentTime;

		ImGui_ImplDX11_NewFrame();
		ImGui::NewFrame();

		ImVec4 clear_color = ImVec4(0.45f, 0.0f, 0.0f, 1.0f);
		static bool show = true;
		ImGui::ShowDemoWindow(&show);

		ImGui::Render();
		g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
		g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, (float*)&clear_color);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		g_pSwapChain->Present(1, 0);

		mOldTime = mCurrentTime;
	}

	void ImGuiLayer::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<MouseButtonPressedEvent>(TOAST_BIND_EVENT_FN(ImGuiLayer::OnMouseButtonPressedEvent));
		dispatcher.Dispatch<MouseButtonReleasedEvent>(TOAST_BIND_EVENT_FN(ImGuiLayer::OnMouseButtonReleasedEvent));
		dispatcher.Dispatch<MouseMovedEvent>(TOAST_BIND_EVENT_FN(ImGuiLayer::OnMouseMovedEvent));
		dispatcher.Dispatch<MouseScrolledEvent>(TOAST_BIND_EVENT_FN(ImGuiLayer::OnMouseScrolledEvent));
		dispatcher.Dispatch<KeyPressedEvent>(TOAST_BIND_EVENT_FN(ImGuiLayer::OnKeyPressedEvent));
		dispatcher.Dispatch<KeyReleasedEvent>(TOAST_BIND_EVENT_FN(ImGuiLayer::OnKeyReleasedEvent));
		dispatcher.Dispatch<KeyTypedEvent>(TOAST_BIND_EVENT_FN(ImGuiLayer::OnKeyTypedEvent));
		dispatcher.Dispatch<WindowResizeEvent>(TOAST_BIND_EVENT_FN(ImGuiLayer::OnWindowResizeEvent));
	}

	bool ImGuiLayer::OnMouseButtonPressedEvent(MouseButtonPressedEvent& e)
	{
		int button = 0;

		// Due to Win32 we need to remap the button for ImGui
		if(e.GetMouseButton() == 1) { button = 0; }
		if(e.GetMouseButton() == 2) { button = 1; }
		if(e.GetMouseButton() == 16) { button = 2; }

		ImGuiIO& io = ImGui::GetIO();
		io.MouseDown[button] = true;

		return false;
	}

	bool ImGuiLayer::OnMouseButtonReleasedEvent(MouseButtonReleasedEvent& e)
	{
		int button = 0;

		// Due to Win32 we need to remap the button for ImGui
		if (e.GetMouseButton() == 1) { button = 0; }
		if (e.GetMouseButton() == 2) { button = 1; }
		if (e.GetMouseButton() == 4) { button = 2; }

		ImGuiIO& io = ImGui::GetIO();
		io.MouseDown[button] = false;

		return false;
	}

	bool ImGuiLayer::OnMouseMovedEvent(MouseMovedEvent& e)
	{
		ImGuiIO& io = ImGui::GetIO();
		io.MousePos = ImVec2(e.GetX(), e.GetY());

		return false;
	}

	bool ImGuiLayer::OnMouseScrolledEvent(MouseScrolledEvent& e)
	{
		ImGuiIO& io = ImGui::GetIO();
		io.MouseWheel += e.GetDelta() / (float)WHEEL_DELTA;

		return false;
	}

	bool ImGuiLayer::OnKeyPressedEvent(KeyPressedEvent& e)
	{
		ImGuiIO& io = ImGui::GetIO();
		io.KeysDown[e.GetKeyCode()] = true;

		io.KeyCtrl = (::GetKeyState(TOAST_CONTROL) & 0x8000) != 0;
		io.KeyShift = (::GetKeyState(TOAST_SHIFT) & 0x8000) != 0;
		io.KeyAlt = (::GetKeyState(TOAST_MENU) & 0x8000) != 0;
		io.KeySuper = false;

		return false;
	}

	bool ImGuiLayer::OnKeyReleasedEvent(KeyReleasedEvent& e)
	{
		ImGuiIO& io = ImGui::GetIO();
		io.KeysDown[e.GetKeyCode()] = false;

		return false;
	}

	bool ImGuiLayer::OnKeyTypedEvent(KeyTypedEvent& e)
	{
		ImGuiIO& io = ImGui::GetIO();

		int keycode = e.GetKeyCode();

		if (keycode > 0 && keycode < 0x10000)
			io.AddInputCharacter((unsigned short)keycode);

		return false;
	}

	bool ImGuiLayer::OnWindowResizeEvent(WindowResizeEvent& e)
	{
		CleanupRenderTarget();
		g_pSwapChain->ResizeBuffers(0, (UINT)(e.GetWidth()), (UINT)(e.GetHeight()), DXGI_FORMAT_UNKNOWN, 0);
		CreateRenderTarget();	

		ImGuiIO& io = ImGui::GetIO();
		io.DisplaySize = ImVec2(e.GetWidth(), e.GetHeight());
		io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

		return false;
	}
}