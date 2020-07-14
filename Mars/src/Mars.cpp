#include <Toast.h>

#include "imgui/imgui.h"

class ExampleLayer : public Toast::Layer 
{
public:
	ExampleLayer()
		: Layer("Example") 
	{
	}

	void OnUpdate() override 
	{
		//TOAST_INFO("ExampleLayer::Update");

		if(Toast::Input::IsKeyPressed(TOAST_TAB))
			TOAST_INFO("Tab key is pressed (poll)");
	}

	virtual void OnImGuiRender() override
	{
		ImGui::Begin("Test");
		ImGui::Text("Hello World");
		ImGui::End();
	}

	void OnEvent(Toast::Event& event) override
	{
		if (event.GetEventType() == Toast::EventType::KeyPressed)
		{
			Toast::KeyPressedEvent& e = (Toast::KeyPressedEvent&)event;
			if (e.GetKeyCode() == TOAST_TAB)
				TOAST_TRACE("Tab key is pressed (event)!");
			TOAST_TRACE("{0}", e.GetKeyCode());
		}
	}
};

class Mars : public Toast::Application 
{
public:
	Mars()
	{
		PushLayer(new ExampleLayer());
	}

	~Mars()
	{
	}
};

Toast::Application* Toast::CreateApplication() 
{
	return new Mars();
}