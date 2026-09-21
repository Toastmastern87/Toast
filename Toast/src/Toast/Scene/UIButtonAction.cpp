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
		case UIButtonActionType::SetUIButtonToggled:
			return "SetUIButtonToggled";
		case UIButtonActionType::SetTimeScale:
			return "SetTimeScale";
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
		if (str == "SetUIButtonToggled")
			return UIButtonActionType::SetUIButtonToggled;
		if (str == "SetTimeScale")
			return UIButtonActionType::SetTimeScale;

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
		case UIButtonActionType::SetUIButtonToggled:
			return entity.HasComponent<UIButtonComponent>();
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

	Entity UIButtonAction::ResolveTarget(Scene* scene) const
	{
		if (TargetEntity == 0)
		{
			TOAST_CORE_WARN("UIButtonAction '%s': action type '%s' needs a target entity", Name.c_str(), UIButtonActionTypeToString(Type));
			return {};
		}

		Entity target = scene->FindEntityByUUID(TargetEntity);
		if (!target)
			TOAST_CORE_WARN("UIButtonAction '%s': target entity '%llu' not found", Name.c_str(), TargetEntity);

		return target;
	}

	void UIButtonAction::Execute(Scene* scene) const
	{
		if (!scene)
			return;

		if (Type == UIButtonActionType::None)
			return;

		switch (Type)
		{
			case UIButtonActionType::SetUIComponentVisible:
			{
				Entity target = ResolveTarget(scene);
				if (!target)
					break;

				SetUIVisible(target, BoolParam);
				break;
			}
			case UIButtonActionType::PlayAnimation:
			{
				Entity target = ResolveTarget(scene);
				if (!target)
					break;

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
				Entity target = ResolveTarget(scene);
				if (!target)
					break;

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
			case UIButtonActionType::SetUIButtonToggled:
			{
				Entity target = ResolveTarget(scene);
				if (!target)
					break;

				if (target.HasComponent<UIButtonComponent>())
					target.GetComponent<UIButtonComponent>().Toggled = BoolParam;

				break;
			}
			case UIButtonActionType::SetTimeScale:
			{
				scene->SetTimeScale(FloatParam);
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