#include <Toast.h>
#include <Toast/Core/EntryPoint.h>

#include "TheNextFrontier2D.h"
#include "ExampleLayer.h"

class Mars : public Toast::Application 
{
public:
	Mars()
	{
		//PushLayer(new ExampleLayer());
		PushLayer(new TheNextFrontier2D());
	}

	~Mars()
	{
	}
};

Toast::Application* Toast::CreateApplication() 
{
	return new Mars();
}