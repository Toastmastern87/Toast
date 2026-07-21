#include "ScriptEditorPanel.h"

#include "imgui/imgui.h"

namespace Toast {

	ScriptEditorPanel::ScriptEditorPanel() 
	{
		mEditor.SetLanguage(TextEditor::Language::Cs());
	
		mEditor.SetPalette(TextEditor::GetDarkPalette());

		// Just a small test script to see if the colors are correct
		const char* sample =
			"// Toaster Script Editor\n"
			"public class Mover\n"
			"{\n"
			"    void OnUpdate(float ts)\n"
			"    {\n"
			"        // TODO: load a real .cs asset here in a later step\n"
			"    }\n"
			"}\n";
		mEditor.SetText(sample);
	}

	void ScriptEditorPanel::OnImGuiRender() 
	{
		if (!mOpen)
			return;

		ImGui::Begin("Script Editor", &mOpen);

		mEditor.Render("##scriptEditor");

		ImGui::End();
	}

}