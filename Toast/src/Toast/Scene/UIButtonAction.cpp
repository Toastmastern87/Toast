#include "tpch.h"
#include "UIButtonAction.h"

#include "Toast/Scene/Entity.h"

namespace Toast {

	const char* UIButtonActionTypeToString(UIButtonActionType type)
	{
		switch (type)
		{
		case UIButtonActionType::None:
			return "None";
		case UIButtonActionType::SetUIComponentVisible:
			return "SetUIComponentVisible";
		case UIButtonActionType::PlayAnimation:
			return "PlayAnimation";
		case UIButtonActionType::StopAnimation:
			return "StopAnimation";
		}

		return "None";
	}

	UIButtonActionType UIButtonActionTypeFromString(const std::string& str)
	{
		if (str == "SetUIComponentVisible")
			return UIButtonActionType::SetUIComponentVisible;
		if (str == "PlayAnimation")
			return UIButtonActionType::PlayAnimation;
		if (str == "StopAnimation")
			return UIButtonActionType::StopAnimation;

		return UIButtonActionType::None;
	}

	bool EntityIsValidTargetFor(UIButtonActionType type, Entity& entity)
	{
		switch (type)
		{
		case UIButtonActionType::SetUIComponentVisible:
			return entity.HasComponent<UIPanelComponent>() || entity.HasComponent<UITextComponent>() || entity.HasComponent<UIButtonComponent>() || entity.HasComponent<UIImageComponent>();
		case UIButtonActionType::PlayAnimation:
		case UIButtonActionType::StopAnimation:
			return entity.HasComponent<MeshComponent>();
		default:
			return true;
		}
	}

	static void SetUIVisible(Entity entity, bool visible)
	{
		if (entity.HasComponent<UIPanelComponent>())
			entity.GetComponent<UIPanelComponent>().Visible = visible;
		else if (entity.HasComponent<UIButtonComponent>())
			entity.GetComponent<UIButtonComponent>().Visible = visible;
		else if (entity.HasComponent<UITextComponent>())
			entity.GetComponent<UITextComponent>().Visible = visible;
		else if (entity.HasComponent<UIImageComponent>())
			entity.GetComponent<UIImageComponent>().Visible = visible;
	}

	void UIButtonAction::Execute(Scene* scene) const
	{
		if (!scene)
			return;

		if (Type == UIButtonActionType::None)
			return;

		if (TargetEntity == 0)
			return;

		Entity target = scene->FindEntityByUUID(TargetEntity);
		if (!target)
			return;

		switch (Type)
		{
			case UIButtonActionType::SetUIComponentVisible:
			{
				SetUIVisible(target, BoolParam);
				break;
			}
			case UIButtonActionType::PlayAnimation:
			{
				if (StringParam.empty())
					break;

				if (!target.HasComponent<MeshComponent>())
					break;

				auto& mc = target.GetComponent<MeshComponent>();

				Ref<Mesh> mesh = AssetManager::GetAsset<Mesh>(mc.MeshHandle);
				if (!mesh)
					break;

				if (!mesh->HasAnimation(StringParam))
				{
					TOAST_CORE_WARN("UIButtonAction: animation '%s' not found", StringParam.c_str());
					break;
				}

				if (BoolParam)
					mc.Playbacks[StringParam].PlayReverse();
				else
					mc.Playbacks[StringParam].Play();

				break;
			}
			case UIButtonActionType::StopAnimation:
			{
				if (!target.HasComponent<MeshComponent>())
					break;

				auto& mc = target.GetComponent<MeshComponent>();

				if (StringParam.empty())
				{
					for (auto& [name, playback] : mc.Playbacks)
					{
						playback.IsActive = false;
						playback.TimeElapsed = 0.0f;
					}
				}
				else 
				{
					auto it = mc.Playbacks.find(StringParam);
					if (it == mc.Playbacks.end())
						break;

					it->second.IsActive = false;
					it->second.TimeElapsed = 0.0f;
				}

				break;
			}
			default:
			{
				TOAST_CORE_WARN("ExecuteUIButtonAction: unhandled action type '%s'", UIButtonActionTypeToString(Type));
				break;
			}
		}
	}

}