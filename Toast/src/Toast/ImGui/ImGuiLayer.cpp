#include "tpch.h"
#include "Toast/ImGui/ImGuiLayer.h"

#pragma warning(push, 0)
#include <imgui.h>
#pragma warning(pop)

#include "Toast/Core/Application.h"

#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/RendererAPI.h"

#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>

#include "ImGuizmo.h"

namespace Toast 
{

#define RGB8(u) ImVec4((( (u)>>16)&0xFF)/255.0f,(((u)>>8)&0xFF)/255.0f,((u)&0xFF)/255.0f,1.0f)

	ImGuiLayer::ImGuiLayer(WindowsWindow* window)
		: Layer("ImGuiLayer")
	{
	}

	void ImGuiLayer::OnAttach()
	{
		TOAST_PROFILE_FUNCTION();

		ImGui_ImplWin32_EnableDpiAwareness();

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

		io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleFonts;
		io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleViewports;

		io.Fonts->AddFontFromFileTTF("assets/fonts/1_Roboto Mono/RobotoMono-Bold.ttf", 16.0f);
		io.Fonts->AddFontFromFileTTF("assets/fonts/1_Roboto Mono/RobotoMono-Bold.ttf", 48.0f);
		io.FontDefault = io.Fonts->AddFontFromFileTTF("assets/fonts/1_Roboto Mono/RobotoMono-Regular.ttf", 16.0f);

		// Add the icons
		ImFontConfig config;
		config.MergeMode = true;
		static const ImWchar iconRanges[] = { 0xf000, 0xf307, 0 };
		io.Fonts->AddFontFromFileTTF("assets/fonts/FontAwesome/fontawesome-webfont.ttf", 13.0f, &config, iconRanges);

		io.Fonts->AddFontFromFileTTF("assets/fonts/1_Roboto Mono/RobotoMono-Bold.ttf", 24.0f);
		config.MergeMode = true;
		config.GlyphMinAdvanceX = 24.0f;
		io.Fonts->AddFontFromFileTTF("assets/fonts/FontAwesome/fontawesome-webfont.ttf", 24.0f,&config, iconRanges);

		io.Fonts->AddFontFromFileTTF("assets/fonts/1_Roboto Mono/RobotoMono-Bold.ttf", 18.0f);
		config.MergeMode = true;
		config.GlyphMinAdvanceX = 18.0f;
		io.Fonts->AddFontFromFileTTF("assets/fonts/FontAwesome/fontawesome-webfont.ttf", 18.0f, &config, iconRanges);

		ImGui::StyleColorsDark();

		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		SetDarkThemeColors();

		Application& app = Application::Get();
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		ImGui_ImplWin32_Init(app.GetWindow().GetNativeWindow());
		ImGui_ImplDX11_Init(device, deviceContext);
	}

	void ImGuiLayer::OnDetach()
	{
		TOAST_PROFILE_FUNCTION();
 
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	void ImGuiLayer::OnEvent(Event& e)
	{
		if (mBlockEvents) 
		{
			ImGuiIO& io = ImGui::GetIO();
			e.Handled |= e.IsInCategory(EventCategoryMouse) & io.WantCaptureMouse;
			e.Handled |= e.IsInCategory(EventCategoryKeyboard) & io.WantCaptureKeyboard;
		}
	}

	void ImGuiLayer::Begin()
	{
		TOAST_PROFILE_FUNCTION();

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();
	}

	void ImGuiLayer::End()
	{
		TOAST_PROFILE_FUNCTION();

		ImGuiIO& io = ImGui::GetIO();
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
	}

	void ImGuiLayer::SetDarkThemeColors()
	{
		auto& colors = ImGui::GetStyle().Colors;

		colors[ImGuiCol_WindowBg]			= RGB8(0x0C0C0C);

		// Text
		colors[ImGuiCol_Text]				= RGB8(0xFFFFFF); // bright white
		colors[ImGuiCol_TextDisabled]		= RGB8(0xB0B0B0); // softer grey

		// Headers
		colors[ImGuiCol_Header]				= RGB8(0x151616); // ~0.083,0.085,0.088
		colors[ImGuiCol_HeaderHovered]		= RGB8(0x202021); // ~0.126,0.127,0.129
		colors[ImGuiCol_HeaderActive]		= RGB8(0x101010); // ~0.063

		// Buttons
		colors[ImGuiCol_Button]				= RGB8(0x151616);
		colors[ImGuiCol_ButtonHovered]		= RGB8(0x202021);
		colors[ImGuiCol_ButtonActive]		= RGB8(0x101010);

		// Frame BG
		colors[ImGuiCol_FrameBg]			= RGB8(0x151616);
		colors[ImGuiCol_FrameBgHovered]		= RGB8(0x202021);
		colors[ImGuiCol_FrameBgActive]		= RGB8(0x101010);

		// Tabs
		colors[ImGuiCol_Tab]				= RGB8(0x101010);
		colors[ImGuiCol_TabHovered]			= RGB8(0x282828); // ~0.158
		colors[ImGuiCol_TabActive]			= RGB8(0x1E1E1E); // ~0.117
		colors[ImGuiCol_TabUnfocused]		= RGB8(0x101010);
		colors[ImGuiCol_TabUnfocusedActive] = RGB8(0x151616);

		// Title bars
		colors[ImGuiCol_TitleBg]			= RGB8(0x101010);
		colors[ImGuiCol_TitleBgActive]		= RGB8(0x101010);
		colors[ImGuiCol_TitleBgCollapsed]	= RGB8(0x101010);

		colors[ImGuiCol_MenuBarBg] = colors[ImGuiCol_TitleBgActive];
	}

}