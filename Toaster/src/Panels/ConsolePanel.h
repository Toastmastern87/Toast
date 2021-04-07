#pragma once

#include "Toast/Core/Base.h"
#include "Toast/Scene/Scene.h"

namespace Toast {

	class ConsolePanel
	{
	public:
		ConsolePanel() = default;
		~ConsolePanel() = default;

		void OnImGuiRender();
	private:
		std::vector<std::string> mLogMessages;
	};
}
