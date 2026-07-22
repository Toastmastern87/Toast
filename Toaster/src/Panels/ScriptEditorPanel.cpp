#include "ScriptEditorPanel.h"

#include "Toast/Core/Log.h"

#include "imgui/imgui.h"

#include <fstream>;
#include <sstream>;

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

	void ScriptEditorPanel::OpenFile(const std::filesystem::path& filepath)
	{
		std::ifstream stream(filepath, std::ios::in | std::ios::binary);
		if (!stream)
		{
			TOAST_CORE_ERROR("ScriptEditorPanel: failed to open '%s'", filepath.string().c_str());
			return;
		}

		std::stringstream ss;
		ss << stream.rdbuf();

		mEditor.SetText(ss.str());
		mCurrentFile = filepath;
	}

}