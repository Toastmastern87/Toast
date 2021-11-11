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

		for (auto itr = Log::sMessages.begin(); itr != Log::sMessages.end(); ++itr)
		{
			switch (itr->first)
			{
			case Severity::Trace:
				ImGui::TextColored(mTraceColor, itr->second.c_str()); break;
			case Severity::Info:
				ImGui::TextColored(mInfoColor, itr->second.c_str()); break;
			case Severity::Warning:
				ImGui::TextColored(mWarnColor, itr->second.c_str()); break;
			case Severity::Error:
				ImGui::TextColored(mErrorColor, itr->second.c_str()); break;
			case Severity::Critical:
				ImGui::TextColored(mCriticalColor, itr->second.c_str()); break;
			}
		}

		//ImGui::SetScrollHere(1.0f);

		ImGui::End();
	}

}