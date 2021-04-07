#include "ConsolePanel.h"

#include "Toast/Utils/PlatformUtils.h"

#include "Toast/Core/Log.h"

#include "imgui/imgui.h"

namespace Toast {

	void ConsolePanel::OnImGuiRender()
	{
		mLogMessages.clear();

		ImGui::Begin("Console");

		std::string temp;
		std::string logString = Log::GetLogString();

		std::istringstream logStream(logString);

		while (std::getline(logStream, temp)) 
		{
			mLogMessages.push_back(temp);
		}

		for (std::string msg : mLogMessages)
		{
			std::size_t blankspace = msg.find(" ");

			switch (msg.at(0)) 
			{
			case 'i':
			{
				ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), msg.substr(blankspace + 1).c_str());
				break;
			}
			case 't':
			{
				ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), msg.substr(blankspace + 1).c_str());
				break;
			}
			case 'c':
			{
				ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), msg.substr(blankspace + 1).c_str());
				break;
			}
			case 'w':
			{
				ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), msg.substr(blankspace + 1).c_str());
				break;
			}
			}

		}

		ImGui::SetScrollHere(1.0f);

		ImGui::End();
	}

}