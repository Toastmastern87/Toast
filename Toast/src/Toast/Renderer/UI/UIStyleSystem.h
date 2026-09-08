#pragma once

#include "Toast/Assets/Asset.h"

#include "Toast/Renderer/UI/StyleSheet.h"

#include <filesystem>

namespace Toast {

	class Scene;
	class Entity;

	struct UIPanelComponent;
	struct UIButtonComponent;
	struct UITextComponent;

	class UIStyleSystem
	{
	public:
		static void Init();
		static void Shutdown();

		static void SetActiveScene(Scene* scene);

		static void ResolveEntity(Entity entity);
		static void ResolveAll(Scene* scene);

		static AssetHandle CreateStyleSheet(const std::filesystem::path& relativePath);
		static AssetHandle CreateStyleSheetFromComponent(Entity entity, const std::filesystem::path& relativePath);

		static void ResetUnoverridden(UIPanelComponent& component);
		static void ResetUnoverridden(UIButtonComponent& component);
		static void ResetUnoverridden(UITextComponent& component);

		static void StartFileWatcher();
		static void StopFileWatcher();

		static void ReloadAllStyleSheets();

		static const StyleBlock* GetBlock(AssetHandle sheet);
	private:
		template<typename T>
		static void ResolveComponent(T& component)
		{
			if (component.Style.Sheet == AssetHandle(0))
				return;

			ResetUnoverridden(component);
			ApplyStyle(component);
		}

		static void ApplyStyle(UIPanelComponent& component);
		static void ApplyStyle(UIButtonComponent& component);
		static void ApplyStyle(UITextComponent& component);

		static StyleBlock CaptureFromComponent(const UIPanelComponent& component);
		static StyleBlock CaptureFromComponent(const UIButtonComponent& component);
		static StyleBlock CaptureFromComponent(const UITextComponent& component);
	};

}