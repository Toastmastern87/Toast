#include <Toast.h>
#include <Toast/Core/EntryPoint.h>

#include "EditorLayer.h"

namespace Toast {

	class Toaster : public Toast::Application
	{
	public:
		Toaster(const Toast::ApplicationSpecification& specification)
			: Application(specification)
		{
			PushLayer(new EditorLayer());
		}

		~Toaster()
		{
		}
	};

	Application* CreateApplication()
	{
		ApplicationSpecification spec;
		spec.Name = "Toaster";

		return new Toaster(spec);
	}
}