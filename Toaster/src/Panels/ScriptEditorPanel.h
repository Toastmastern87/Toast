#pragma once

#include "../vendor/ImGuiColorTextEdit/TextEditor.h"

namespace Toast {

	class ScriptEditorPanel
	{
	public:
		ScriptEditorPanel();

		void OnImGuiRender();

		bool IsOpen() const { return mOpen; }
		void SetOpen(bool open) { mOpen = open; }
	private:
		TextEditor mEditor;

		bool mOpen = false;
	};

}