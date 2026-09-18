#pragma once

#include <string>

#include "Toast/Core/UUID.h"

namespace Toast {

	class Scene;
	class Entity;

	enum class UIButtonActionType : uint8_t
	{
		None = 0,
		SetUIComponentVisible = 1,
		PlayAnimation = 2,
		StopAnimation = 3,
	};

	struct UIButtonAction 
	{
		std::string Name;
		UIButtonActionType Type = UIButtonActionType::None;
		UUID TargetEntity = 0;
		std::string StringParam;
		bool BoolParam = false; // SetUIComponentVisible: visible state / PlayAnimation: play in reverse

		void Execute(Scene* scene) const;
	};

	const char* UIButtonActionTypeToString(UIButtonActionType type);
	UIButtonActionType UIButtonActionTypeFromString(const std::string& str);

	bool EntityIsValidTargetFor(UIButtonActionType type, Entity& entity);

}