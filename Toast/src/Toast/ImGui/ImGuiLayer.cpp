#include "tpch.h"
#include "Toast/ImGui/ImGuiLayer.h"

#include <imgui.h>

#include "Toast/Core/Application.h"

#include "Toast/Renderer/Renderer.h"

#include "Platform/DirectX/DirectXRendererAPI.h"

#include <examples/imgui_impl_dx11.h>
#include <examples/imgui_impl_win32.h>

namespace Toast 
{
	ImGuiLayer::ImGuiLayer()
		: Layer("ImGuiLayer")
	{
	}

	void ImGuiLayer::OnAttach()
	{
		ImGui_ImplWin32_EnableDpiAwareness();

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

		io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleFonts;
		io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleViewports;

		ImGui::StyleColorsDark();

		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		Application& app = Application::Get();
		DirectXRendererAPI API = static_cast<DirectXRendererAPI&>(*RenderCommand::sRendererAPI);
		ID3D11Device* device = API.GetDevice();
		ID3D11DeviceContext* deviceContext = API.GetDeviceContext();

		ImGui_ImplWin32_Init(app.GetWindow().GetNativeWindow());
		ImGui_ImplDX11_Init(device, deviceContext);
	}

	void ImGuiLayer::OnDetach()
	{
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	void ImGuiLayer::Begin()
	{
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
	}

	void ImGuiLayer::End()
	{
		ImGuiIO& io = ImGui::GetIO();
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
	}
}