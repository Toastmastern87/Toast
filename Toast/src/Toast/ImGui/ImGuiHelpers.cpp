#include "tpch.h"

#include "ImGuiHelpers.h"

namespace Toast
{
	namespace ImGuiHelpers
	{

		struct DragState
		{
			float startValue;
			ImVec2 lastDelta;

			bool  isEditing = false;
			char  inputBuf[64] = {};
		};

		static std::unordered_map<ImGuiID, DragState> g_DragStates;

		bool ManualDragFloat(const char* label, float& value, WindowsWindow* window, std::string& activeDragArea, float speed, ImVec2 dragAreaSize, const char* displayFormat, float minVal, float maxVal)
		{
			ImGuiID id = ImGui::GetID(label);
			DragState& st = g_DragStates[id];

			bool changed = false;

			static bool justBecameEditing = false;

			// 2) We create an invisible button to capture clicks
			//    The size can be adjusted, or you can do a small arrow etc.
			ImGui::PushID(label);

			if (!st.isEditing)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0x2F, 0x31, 0x33, 0xFF)); // #2F3133
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(0x4C, 0x4D, 0x4E, 0xFF)); 
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(0x37, 0x39, 0x3B, 0xFF));

				ImGuiButtonFlags btnFlags = ImGuiButtonFlags_PressedOnClick;
				bool clicked = ImGui::ButtonEx("##DragArea", dragAreaSize, btnFlags);

				bool hovered = ImGui::IsItemHovered();
				ImGuiIO& io = ImGui::GetIO();

				if (hovered && io.MouseClickedCount[ImGuiMouseButton_Left] == 2)
				{
					justBecameEditing = true; 

					st.isEditing = true;
					// Copy current float to inputBuf
					snprintf(st.inputBuf, sizeof(st.inputBuf), displayFormat, value);
				}
				else if (clicked)
				{
					// Handle single click (e.g., start dragging)
					if (!window->IsDragging())
					{
						window->SetDragOnGoing(true);
						activeDragArea = label; // Set the active drag area
						st.startValue = value;
						st.lastDelta = ImVec2(0, 0);
					}
				}

				ImGui::PopStyleColor(3);

				// Now we overlay the numeric text in the center of that same button
				// We'll get the item rect to find out where the button is
				ImDrawList* drawList = ImGui::GetWindowDrawList();
				ImVec2 rectMin = ImGui::GetItemRectMin(); // top-left of button
				ImVec2 rectMax = ImGui::GetItemRectMax(); // bottom-right of button
				ImVec2 center = ImVec2(
					(rectMin.x + rectMax.x) * 0.5f,
					(rectMin.y + rectMax.y) * 0.5f
				);

				// Format the numeric text
				char textBuf[64];
				snprintf(textBuf, sizeof(textBuf), displayFormat, value);

				// Compute text size so we can center it
				ImVec2 textSize = ImGui::CalcTextSize(textBuf);
				ImVec2 textPos = ImVec2(
					center.x - textSize.x * 0.5f,
					center.y - textSize.y * 0.5f
				);

				drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
					ImVec2(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f),
					IM_COL32(255, 255, 255, 255), // White color
					textBuf);

				// 3) If we are in "dragging" mode, accumulate deltas into 'value'
				if (window->IsDragging() && activeDragArea == label)
				{
					bool leftDown = ImGui::GetIO().MouseDown[0];
					if (!leftDown)
					{
						window->SetDragOnGoing(false);
					}
					else
					{
						POINT delta = window->GetDeltaDrag(); // Get delta from WindowsWindow
						float dx = static_cast<float>(delta.x) - st.lastDelta.x;
						value += dx * speed;

						if (minVal != FLT_MAX && value < minVal)
							value = minVal;

						if (maxVal != 0.0f && value > maxVal)
							value = maxVal;

						changed = (dx != 0.0f);

						st.lastDelta.x = static_cast<float>(delta.x);
					}
				}
				else
				{
					// if user clicked => start dragging
					if (clicked)
					{
						if (!window->IsDragging())
						{
							window->SetDragOnGoing(true);
							st.startValue = value;
							st.lastDelta = ImVec2(0, 0);
						}
					}
				}
			}
			else
			{
				ImGui::SetNextItemWidth(dragAreaSize.x);

				if (justBecameEditing)
				{
					ImGui::SetKeyboardFocusHere();
					justBecameEditing = false; // Reset
				}

				ImVec2 startPos = ImGui::GetCursorScreenPos();

				if (ImGui::InputText("##FloatInput", st.inputBuf, IM_ARRAYSIZE(st.inputBuf),
					ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
				{
					// user pressed Enter
					// parse
					float newVal = value;
					if (sscanf(st.inputBuf, "%f", &newVal) == 1)
					{
						value = newVal;
						changed = true;
					}
					st.isEditing = false; // done
				}

				// If user clicks away or otherwise defocuses, we finalize
				// We'll check ImGui::IsItemDeactivated() which is true
				// if user clicks outside or tab away
				if (ImGui::IsItemDeactivated() && st.isEditing)
				{
					float newVal = value;
					if (sscanf(st.inputBuf, "%f", &newVal) == 1)
					{
						value = newVal;
						changed = true;
					}
					st.isEditing = false;
				} 
			}

			ImGui::PopID();

			return changed;
		}

		bool ManualDragDouble(const char* label, double& value,	WindowsWindow* window, std::string& activeDragArea,	float speed, ImVec2 dragAreaSize, const char* displayFormat, double minVal, double maxVal)
		{
			ImGuiID id = ImGui::GetID(label);
			DragState& st = g_DragStates[id];

			bool changed = false;

			// We create a push/pop ID to differentiate this widget in ImGui
			ImGui::PushID(label);

			// If not in editing mode (text input), we handle the drag area
			if (!st.isEditing)
			{
				// Style for the "button" background
				ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0x2F, 0x31, 0x33, 0xFF));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(0x4C, 0x4D, 0x4E, 0xFF));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(0x37, 0x39, 0x3B, 0xFF));

				// Use ButtonEx with 'PressedOnClick' so the click is recognized on mouse-down
				ImGuiButtonFlags btnFlags = ImGuiButtonFlags_PressedOnClick;
				bool clicked = ImGui::ButtonEx("##DragArea", dragAreaSize, btnFlags);

				bool hovered = ImGui::IsItemHovered();
				ImGuiIO& io = ImGui::GetIO();

				// Check double-click to switch to editing mode
				if (hovered && io.MouseClickedCount[ImGuiMouseButton_Left] == 2)
				{
					st.isEditing = true;

					// Copy current double into the input buffer
					// e.g. 2 decimal places
					std::snprintf(st.inputBuf, sizeof(st.inputBuf), displayFormat, value);
				}
				else if (clicked)
				{
					// Single click => start dragging if not already dragging
					if (!window->IsDragging())
					{
						window->SetDragOnGoing(true);
						activeDragArea = label;     // mark this as the active drag area
						st.startValue = value;
						st.lastDelta = ImVec2(0.0f, 0.0f);
					}
				}

				ImGui::PopStyleColor(3);

				// Overlay numeric text in the center of that "button" area
				// We'll get the item rect, compute center
				ImDrawList* drawList = ImGui::GetWindowDrawList();
				ImVec2 rectMin = ImGui::GetItemRectMin();
				ImVec2 rectMax = ImGui::GetItemRectMax();
				ImVec2 center = ImVec2(
					(rectMin.x + rectMax.x) * 0.5f,
					(rectMin.y + rectMax.y) * 0.5f
				);

				// Format the numeric text (example: 1 decimal place)
				char textBuf[64];
				std::snprintf(textBuf, sizeof(textBuf), displayFormat, value);

				// Compute text size so we can center it
				ImVec2 textSize = ImGui::CalcTextSize(textBuf);

				// Draw text centered
				drawList->AddText(
					ImGui::GetFont(),
					ImGui::GetFontSize(),
					ImVec2(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f),
					IM_COL32(255, 255, 255, 255),
					textBuf
				);

				// If dragging, accumulate deltas into 'value'
				if (window->IsDragging() && activeDragArea == label)
				{
					bool leftDown = io.MouseDown[0];
					if (!leftDown)
					{
						// Mouse released => stop dragging
						window->SetDragOnGoing(false);
						activeDragArea.clear();
					}
					else
					{
						// Use your window->GetDeltaDrag() or some method
						POINT delta = window->GetDeltaDrag();
						double dx = static_cast<double>(delta.x) - st.lastDelta.x;

						value += dx * speed;

						if (minVal != FLT_MAX && value < minVal)
							value = minVal;

						if (maxVal != 0.0f && value > maxVal)
							value = maxVal;

						changed = (dx != 0.0);

						st.lastDelta.x = static_cast<float>(delta.x);
					}
				}
				else
				{
					// If user clicked => start dragging (re-check in case above wasn't triggered)
					if (clicked)
					{
						if (!window->IsDragging())
						{
							window->SetDragOnGoing(true);
							st.startValue = value;
							st.lastDelta = ImVec2(0, 0);
						}
					}
				}
			}
			else
			{
				// -- Editing Mode (InputText) --

				ImGui::SetNextItemWidth(dragAreaSize.x);

				// Provide an InputText for the user to type the double
				if (ImGui::InputText(
					"##doubleInput",
					st.inputBuf,
					IM_ARRAYSIZE(st.inputBuf),
					ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll
				))
				{
					// user pressed Enter => parse the double
					double newVal = value;
					if (std::sscanf(st.inputBuf, "%lf", &newVal) == 1)
					{
						value = newVal;
						changed = true;
					}
					st.isEditing = false; // done editing
				}

				// If user clicks away or defocuses, finalize
				if (ImGui::IsItemDeactivated() && st.isEditing)
				{
					double newVal = value;
					if (std::sscanf(st.inputBuf, "%lf", &newVal) == 1)
					{
						value = newVal;
						changed = true;
					}
					st.isEditing = false;
				}
			}

			ImGui::PopID();
			return changed;
		}

		bool ManualDragFloat3(const std::string& label, DirectX::XMFLOAT3& values, float speed, float resetValue, WindowsWindow* window, std::string& activeDragArea)
		{
			bool changed = false;

			ImGuiIO& io = ImGui::GetIO();
			auto boldFont = io.Fonts->Fonts[0];

			// layout example: we do columns or a simple horizontal layout
			ImGui::PushID(label.c_str());

			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 105.0f);

			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(label.c_str());
			ImGui::NextColumn();

			float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
			ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };
			ImVec2 dragAreaSize = { 54.0f, lineHeight };

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 4.0f));

			// We'll do X
			{
				// colored button for "X"
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.15f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.1f, 0.15f, 1.0f));
				ImGui::PushFont(boldFont);
				if (ImGui::Button("X", buttonSize))
				{
					values.x = resetValue;
					changed = true;
				}
				ImGui::PopStyleColor(3);
				ImGui::PopFont();
				ImGui::SameLine();

				std::string dragArea1 = "##" + label + "dragarea1";

				changed |= ManualDragFloat(dragArea1.c_str(), values.x, window, activeDragArea, speed, dragAreaSize);

				ImGui::SameLine();
			}

			// Y
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
				ImGui::PushFont(boldFont);
				if (ImGui::Button("Y", buttonSize))
				{
					values.y = resetValue;
					changed = true;
				}
				ImGui::PopStyleColor(3);
				ImGui::PopFont();
				ImGui::SameLine();

				std::string dragArea2 = "##" + label + "dragarea2";

				changed |= ManualDragFloat(dragArea2.c_str(), values.y, window, activeDragArea, speed, dragAreaSize);

				ImGui::SameLine();
			}

			// Z
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.25f, 0.8f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.35f, 0.9f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.25f, 0.8f, 1.0f));
				ImGui::PushFont(boldFont);
				if (ImGui::Button("Z", buttonSize ))
				{
					values.z = resetValue;
					changed = true;
				}
				ImGui::PopStyleColor(3);
				ImGui::PopFont();
				ImGui::SameLine();

				std::string dragArea3 = "##" + label + "dragarea3";

				changed |= ManualDragFloat(dragArea3.c_str(), values.z, window, activeDragArea, speed, dragAreaSize);
			}

			ImGui::PopStyleVar();

			ImGui::Columns(1);
			ImGui::PopID();

			return changed;
		}

		bool ManualDragFloat3(const std::string& label, Vector3& values, float speed, float resetValue, WindowsWindow* window, std::string& activeDragArea)
		{
			bool changed = false;

			ImGuiIO& io = ImGui::GetIO();
			auto boldFont = io.Fonts->Fonts[0];

			// layout example: we do columns or a simple horizontal layout
			ImGui::PushID(label.c_str());

			ImGui::TableSetColumnIndex(0);
			ImGui::TextWrapped(label.c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::PushItemWidth(-1);

			float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
			ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };
			ImVec2 dragAreaSize = { 54.0f, lineHeight };

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 4.0f));

			// We'll do X
			{
				// colored button for "X"
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.15f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.1f, 0.15f, 1.0f));
				ImGui::PushFont(boldFont);
				if (ImGui::Button("X", buttonSize))
				{
					values.x = resetValue;
					changed = true;
				}
				ImGui::PopStyleColor(3);
				ImGui::PopFont();
				ImGui::SameLine();

				std::string dragArea1 = "##" + label + "dragarea1";

				changed |= ManualDragDouble(dragArea1.c_str(), values.x, window, activeDragArea, speed, dragAreaSize);

				ImGui::SameLine();
			}

			// Y
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
				ImGui::PushFont(boldFont);
				if (ImGui::Button("Y", buttonSize))
				{
					values.y = resetValue;
					changed = true;
				}
				ImGui::PopStyleColor(3);
				ImGui::PopFont();
				ImGui::SameLine();

				std::string dragArea2 = "##" + label + "dragarea2";

				changed |= ManualDragDouble(dragArea2.c_str(), values.y, window, activeDragArea, speed, dragAreaSize);

				ImGui::SameLine();
			}

			// Z
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.25f, 0.8f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.35f, 0.9f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.25f, 0.8f, 1.0f));
				ImGui::PushFont(boldFont);
				if (ImGui::Button("Z", buttonSize))
				{
					values.z = resetValue;
					changed = true;
				}
				ImGui::PopStyleColor(3);
				ImGui::PopFont();
				ImGui::SameLine();

				std::string dragArea3 = "##" + label + "dragarea3";

				changed |= ManualDragDouble(dragArea3.c_str(), values.z, window, activeDragArea, speed, dragAreaSize);
			}

			ImGui::PopStyleVar();

			ImGui::PopItemWidth();

			ImGui::PopID();

			return changed;
		}

		bool ManualDragDouble3(const std::string& label, Vector3& values, float speed, float resetValue, WindowsWindow* window, std::string& activeDragArea)
		{
			bool changed = false;

			ImGuiIO& io = ImGui::GetIO();
			auto boldFont = io.Fonts->Fonts[0];

			// layout example: we do columns or a simple horizontal layout
			ImGui::PushID(label.c_str());

			ImGui::TableSetColumnIndex(0);
			ImGui::TextWrapped(label.c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::PushItemWidth(-1);

			float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
			ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };
			ImVec2 dragAreaSize = { 54.0f, lineHeight };

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 4.0f));

			// We'll do X
			{
				// colored button for "X"
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.15f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.1f, 0.15f, 1.0f));
				ImGui::PushFont(boldFont);
				if (ImGui::Button("X", buttonSize))
				{
					values.x = resetValue;
					changed = true;
				}
				ImGui::PopStyleColor(3);
				ImGui::PopFont();
				ImGui::SameLine();

				std::string dragArea1 = "##" + label + "dragarea1";

				changed |= ManualDragDouble(dragArea1.c_str(), values.x, window, activeDragArea, speed, dragAreaSize);

				ImGui::SameLine();
			}

			// Y
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
				ImGui::PushFont(boldFont);
				if (ImGui::Button("Y", buttonSize))
				{
					values.y = resetValue;
					changed = true;
				}
				ImGui::PopStyleColor(3);
				ImGui::PopFont();
				ImGui::SameLine();

				std::string dragArea2 = "##" + label + "dragarea2";

				changed |= ManualDragDouble(dragArea2.c_str(), values.y, window, activeDragArea, speed, dragAreaSize);

				ImGui::SameLine();
			}

			// Z
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.25f, 0.8f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.35f, 0.9f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.25f, 0.8f, 1.0f));
				ImGui::PushFont(boldFont);
				if (ImGui::Button("Z", buttonSize))
				{
					values.z = resetValue;
					changed = true;
				}
				ImGui::PopStyleColor(3);
				ImGui::PopFont();
				ImGui::SameLine();

				std::string dragArea3 = "##" + label + "dragarea3";

				changed |= ManualDragDouble(dragArea3.c_str(), values.z, window, activeDragArea, speed, dragAreaSize);
			}

			ImGui::PopStyleVar();

			ImGui::Columns(1);
			ImGui::PopID();

			return changed;
		}
	}
}