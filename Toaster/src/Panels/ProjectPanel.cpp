#include "ProjectPanel.h"

#include "../FontAwesome.h"
#include "imgui/imgui.h"

namespace Toast {

	static float CalcListBoxHeight(float visibleItems)
	{
		const float rowH = ImGui::GetFrameHeight();
		const float rowGap = 1.0f; // matches your Dummy spacing
		const float innerPadY = 8.0f * 2.0f; // similar to your box padding estimate
		return innerPadY + visibleItems * rowH + (visibleItems - 1.0f) * rowGap;
	}

	void ProjectPanel::OnImGuiRender()
	{
		if (!mContext)
			return;

		ImGui::Begin(ICON_TOASTER_FOLDER_OPEN " Project");

		// ---------- Project Header ----------
		{
			ImGui::TextUnformatted("Project");
			ImGui::SameLine();
			ImGui::TextDisabled("%s", mContext->GetName().c_str());

			ImGui::TextDisabled("%s", mContext->GetPath().string().c_str());

			ImGui::Separator();
			ImGui::Spacing();
		}

		// ---------- Scenes Box (header + list inside the child) ----------
		{
			const float fullW = ImGui::GetContentRegionAvail().x;
			const float boxH = CalcListBoxHeight(6.0f);

			ImGuiWindowFlags childFlags = ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NoMove;

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
			ImGui::BeginChild("##ProjectScenesBox", ImVec2(fullW, boxH), true, childFlags);

			// --- Header row INSIDE the box ---
			{
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted("Scenes");

				const float btnSize = ImGui::GetFrameHeight(); // square
				float rightX = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - btnSize;

				ImGui::SameLine();
				ImGui::SetCursorPosX(rightX);

				ImGui::PushID("ProjectScenesAdd_InsideBox");
				if (ImGui::Button("+", ImVec2(btnSize, btnSize)))
					ImGui::OpenPopup("##ScenesAddPopup");

				if (ImGui::BeginPopup("##ScenesAddPopup"))
				{
					if (ImGui::MenuItem("New Scene..."))
					{
						UUID newId = mContext->CreateNewScene("NewScene", false);

						// Select it in the list
						mSelectedScene = newId;

						// Optional: start renaming immediately (recommended UX)
						mRenamingScene = newId;
						mRenameWantsFocus = true;
						memset(mRenameBuffer, 0, sizeof(mRenameBuffer));
						strncpy(mRenameBuffer, "NewScene", sizeof(mRenameBuffer) - 1);

						ImGui::CloseCurrentPopup();
					}
					if (ImGui::MenuItem("Import Scene..."))
					{
						// TODO (later)
					}
					ImGui::EndPopup();
				}
				ImGui::PopID();

				ImGui::Separator();
			}

			// Small spacing below header
			ImGui::Dummy(ImVec2(0.0f, 2.0f));

			// --- List content ---
			const auto& scenes = mContext->GetScenes();
			UUID activeId = mContext->GetActiveSceneID();

			if (scenes.empty())
			{
				ImGui::TextDisabled("No scenes in this project yet.");
			}
			else
			{
				int i = 0;
				for (const auto& [id, entry] : scenes)
				{
					const std::string name = entry.Path.stem().string();

					ImGui::PushID(i++);

					const bool isActive = (id == activeId);
					const bool isSelected = (mSelectedScene == id);
					const bool isRenamingThis = (mRenamingScene == id);

					// Boxed button styling
					ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
					ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 4.0f));

					// Full row width
					const float w = ImGui::GetContentRegionAvail().x;

					// ASCII-safe label (no special glyphs)
					std::string itemLabel = isActive ? ("> " + name) : name;

					// ---- Colors: Active > Selected > Normal (BUTTON COLORS because we draw a Button) ----
					int pushedColors = 0;

					if (isActive)
					{
						// Use theme color, just increase opacity. Avoid RGB scaling (looks bad across themes).
						ImVec4 base = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
						base.w = 0.95f;

						ImGui::PushStyleColor(ImGuiCol_Button, base);        ++pushedColors;
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, base); ++pushedColors;
						ImGui::PushStyleColor(ImGuiCol_ButtonActive, base);  ++pushedColors;
					}
					else if (isSelected)
					{
						ImVec4 base = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
						base.w = 0.70f;

						ImGui::PushStyleColor(ImGuiCol_Button, base);                                     ++pushedColors;
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]); ++pushedColors;
						ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);   ++pushedColors;
					}

					// ---- Draw the item ----
					bool clicked = false;

					if (isRenamingThis)
					{
						// Inline rename uses full width
						ImGui::PushItemWidth(w);

						if (mRenameWantsFocus)
						{
							ImGui::SetKeyboardFocusHere();
							mRenameWantsFocus = false;
						}

						const bool commit = ImGui::InputText("##RenameSceneInline",	mRenameBuffer, sizeof(mRenameBuffer), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

						// Cancel on Escape
						if (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Escape))
						{
							mRenamingScene = UUID{};
						}
						else if (commit)
						{
							std::string newName(mRenameBuffer);

							// Basic trim
							while (!newName.empty() && newName.front() == ' ')
								newName.erase(newName.begin());
							while (!newName.empty() && newName.back() == ' ')
								newName.pop_back();

							if (!newName.empty())
								mContext->RenameScene(id, newName);

							mRenamingScene = UUID{};
						}
						else
						{
							// If user clicks elsewhere, cancel rename
							if (!ImGui::IsItemActive() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
								mRenamingScene = UUID{};
						}

						ImGui::PopItemWidth();
					}
					else
					{
						// Compute the button rect so the active accent bar aligns perfectly.
						const ImVec2 textSize = ImGui::CalcTextSize(itemLabel.c_str(), nullptr, false);
						const float  buttonH = textSize.y + ImGui::GetStyle().FramePadding.y * 2.0f;

						// Current button rect (screen-space)
						const ImVec2 buttonMin = ImGui::GetCursorScreenPos();
						const ImVec2 buttonMax = ImVec2(buttonMin.x + w, buttonMin.y + buttonH);

						// Draw accent bar INSIDE the button rect (looks clean with rounding/padding)
						if (isActive)
						{
							const float barW = 3.0f;
							const float insetY = 2.0f;
							ImU32 accent = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
						}

						// Add a bit of left padding so the text doesn't collide with the accent bar
						ImGui::SetCursorPosX(ImGui::GetCursorPosX());

						// Keep your behavior: active scene cannot be selected
						if (isActive)
						{
							ImGui::BeginDisabled(true);
							ImGui::Button(itemLabel.c_str(), ImVec2(w, 0.0f)); // subtract our cursor shift
							ImGui::EndDisabled();
						}
						else
						{
							clicked = ImGui::Button(itemLabel.c_str(), ImVec2(w, 0.0f));
							if (clicked)
								mSelectedScene = id;

							if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
							{
								// Optional: keep selection consistent
								mSelectedScene = id;

								// Avoid opening while some inline rename is active (paranoia)
								if (!isRenamingThis && OnOpenSceneRequested)
									OnOpenSceneRequested(id);
							}
						}
					}

					// Pop colors AFTER the widget is drawn
					if (pushedColors > 0)
						ImGui::PopStyleColor(pushedColors);

					// Context menu (disable while renaming)
					if (!isRenamingThis && ImGui::BeginPopupContextItem("##SceneContext", ImGuiPopupFlags_MouseButtonRight))
					{
						if (ImGui::MenuItem("Rename"))
						{
							mRenamingScene = id;
							mRenameWantsFocus = true;
							memset(mRenameBuffer, 0, sizeof(mRenameBuffer));

							const std::string currentName = entry.Path.stem().string();
							strncpy(mRenameBuffer, currentName.c_str(), sizeof(mRenameBuffer) - 1);

							ImGui::CloseCurrentPopup();
						}

						if (ImGui::MenuItem("Delete"))
						{
							// TODO (later)
						}

						ImGui::EndPopup();
					}

					ImGui::PopStyleVar(3);
					ImGui::PopID();
				}
			}


			ImGui::EndChild();
			ImGui::PopStyleVar();
		}

		ImGui::End();
	}

}
