#include <Toast.h>
#include <Toast/Core/EntryPoint.h>

#include "EditorLayer.h"

namespace Toast {

	class Toaster : public Application
	{
	public:
		Toaster()
			: Application("Toaster")
		{
			PushLayer(new EditorLayer());
		}

		~Toaster()
		{
		}
	};

	Application* CreateApplication()
	{
		return new Toaster();
	}
}