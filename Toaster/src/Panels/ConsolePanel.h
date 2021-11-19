#pragma once

#include "Toast/Core/Base.h"

#include "Toast/Scene/Scene.h"

#include "Toast/Core/Log.h"

#include "imgui/imgui.h"

namespace Toast {

	class ConsolePanel
	{
	public:
		ConsolePanel() = default;
		~ConsolePanel() = default;
		static Ref<ConsolePanel>& Get() { return sConsole; }

		void OnImGuiRender();
		void Submit(const std::string& message, Severity level = Severity::Info);

		//void SetAutoScroll(bool autoScroll) { mAutoScroll = autoScroll; }
		//bool* GetAutoScroll() { return &mAutoScroll; }
	private:
		static Ref<ConsolePanel> sConsole;
		
		bool mScrollLock = true;

		//Colors
		ImVec4 mTraceColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		ImVec4 mInfoColor = { 0.1f, 0.9f, 0.1f, 1.0f };
		ImVec4 mWarnColor = { 1.0f, 0.9f, 0.0f, 1.0f };
		ImVec4 mErrorColor = { 1.0f, 0.2f, 0.1f, 1.0f };
		ImVec4 mCriticalColor = { 0.5f, 0.0f, 0.7f, 1.0f };
	};
}
