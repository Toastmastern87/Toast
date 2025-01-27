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
			Window& windowRef = GetWindow();

			// Attempt to cast the Window reference to a WindowsWindow pointer
			WindowsWindow* windowsWindow = dynamic_cast<WindowsWindow*>(&windowRef);

			PushLayer(new EditorLayer(windowsWindow));
		}

		~Toaster()
		{
		}
	};

	Application* CreateApplication()
	{
		ApplicationSpecification spec;
		spec.Name = "Toaster";
		spec.IconStr = "..\\Toaster/Resources/Icons/ToasterIcon32x32.ico";

		return new Toaster(spec);
	}
}