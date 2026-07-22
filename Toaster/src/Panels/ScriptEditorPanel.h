#pragma once

#include "../vendor/ImGuiColorTextEdit/TextEditor.h"

#include <filesystem>

namespace Toast {

	class ScriptEditorPanel
	{
	public:
		ScriptEditorPanel();

		void OnImGuiRender();

		void OpenFile(const std::filesystem::path& filepath);

		bool IsOpen() const { return mOpen; }
		void SetOpen(bool open) { mOpen = open; }
	private:
		TextEditor mEditor;

		std::filesystem::path mCurrentFile;

		bool mOpen = false;
	};

}