#include "ConsolePanel.h"

#include "Toast/Utils/PlatformUtils.h"

#include "Toast/Core/Log.h"

#include "../FontAwesome.h"

#include "imgui/imgui.h"

namespace Toast {

	Ref<ConsolePanel> ConsolePanel::sConsole = CreateRef<ConsolePanel>();

	void ConsolePanel::OnImGuiRender()
	{
		ImGui::Begin(ICON_TOASTER_EXCLAMATION_CIRCLE" Console");

		//if (ImGui::Button(ICON_TOASTER_PLAY))
		//	mScrollLock = true;
		//ImGui::SameLine();
		//if (ImGui::Button(ICON_TOASTER_PAUSE))
		//	mScrollLock = false;

		ImGui::BeginChild("Console", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

		auto& messages = Log::sMessages;
		ImGuiListClipper clipper;
		clipper.Begin((int)messages.size());

		while (clipper.Step()) 
		{
			for (auto i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
			{
				auto& [severity, text] = messages[i];
				switch (severity)
				{
				case Severity::Trace:
					ImGui::TextColored(mTraceColor, text.c_str()); break;
				case Severity::Info:
					ImGui::TextColored(mInfoColor, text.c_str()); break;
				case Severity::Warning:
					ImGui::TextColored(mWarnColor, text.c_str()); break;
				case Severity::Error:
					ImGui::TextColored(mErrorColor, text.c_str()); break;
				case Severity::Critical:
					ImGui::TextColored(mCriticalColor, text.c_str()); break;
				}
			}
		}

		if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
			ImGui::SetScrollHereY(1.0f);

		ImGui::EndChild();
		ImGui::End();
	}

}