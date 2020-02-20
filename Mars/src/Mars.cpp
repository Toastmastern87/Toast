#include <Toast.h>

class ExampleLayer : public Toast::Layer 
{
public:
	ExampleLayer()
		: Layer("Example") 
	{
	}

	void OnUpdate() override 
	{
		TOAST_INFO("ExampleLayer::Update");
	}

	void OnEvent(Toast::Event& e) override
	{
		TOAST_TRACE("{0}", e);
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