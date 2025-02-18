#pragma once

#include "Platform/Windows/WindowsWindow.h"

#include "Toast/Core/Math/Vector.h"

#include "../vendor/imgui/imgui.h"
#include "../vendor/imgui/imgui_internal.h"

#include <string>
#include <float.h> 
#include <cfloat>

namespace Toast
{
	namespace ImGuiHelpers
	{

		bool ManualDragFloat(const char* label, float& value, WindowsWindow* window, std::string& activeDragArea, float speed = 0.1f, ImVec2 dragAreaSize = { 10.0f, 10.0f }, const char* displayFormat = "%.1f", float minVal = -DBL_MAX, float maxVal = FLT_MAX);
		bool ManualDragFloat3(const std::string& label, DirectX::XMFLOAT3& values, float speed, float resetValue, WindowsWindow* window, std::string& activeDragArea);
		bool ManualDragFloat3(const std::string& label, Vector3& values, float speed, float resetValue, WindowsWindow* window, std::string& activeDragArea);

		bool ManualDragDouble(const char* label, double& value, WindowsWindow* window, std::string& activeDragArea, float speed = 0.1f, ImVec2 dragAreaSize = { 10.0f, 10.0f }, const char* displayFormat = "%.1f", double minVal = -DBL_MAX, double maxVal = DBL_MAX);
		bool ManualDragDouble3(const std::string& label, Vector3& values, float speed, float resetValue, WindowsWindow* window, std::string& activeDragArea);
	}
}