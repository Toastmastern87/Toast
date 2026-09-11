#include "tpch.h"
#include "UIStyleSystem.h"

#include "Toast/Core/Application.h"

#include "Toast/Scene/Components.h"
#include "Toast/Scene/Entity.h"
#include "Toast/Scene/Scene.h"

#include "Toast/Assets/AssetManager.h"

#include "Toast/Renderer/Renderer2D.h"

#include "FileWatch.h"

namespace Toast {

	struct UIStyleSystemData
	{
		Scene* ActiveScene = nullptr;
		
		Scope<filewatch::FileWatch<std::string>> Watcher;

		bool ReloadPending = false;

	};

	static Scope<UIStyleSystemData> sData = CreateScope<UIStyleSystemData>();

	static void OnStyleSheetFileSystemEvent(const std::string& path, const filewatch::Event change_type)
	{
		if (change_type != filewatch::Event::modified)
			return;

		if (std::filesystem::path(path).extension() != ".css")
			return;

		if (sData->ReloadPending)
			return;

		sData->ReloadPending = true;

		Application::Get().SubmitToMainThread([]()
			{
				UIStyleSystem::ReloadAllStyleSheets();
				sData->ReloadPending = false;
			});
	}

	void UIStyleSystem::Init()
	{
		TOAST_CORE_INFO("UIStyleSystem initialized.");
	}

	void UIStyleSystem::Shutdown()
	{
		sData->Watcher.reset();
		sData->ActiveScene = nullptr;
	}

	void UIStyleSystem::SetActiveScene(Scene* scene)
	{
		sData->ActiveScene = scene;
	}

	void UIStyleSystem::ResolveEntity(Entity entity)
	{
		if (entity.HasComponent<UIPanelComponent>())
			ResolveComponent(entity.GetComponent<UIPanelComponent>());

		if (entity.HasComponent<UIButtonComponent>())
			ResolveComponent(entity.GetComponent<UIButtonComponent>());

		if (entity.HasComponent<UITextComponent>())
			ResolveComponent(entity.GetComponent<UITextComponent>());
	}

	void UIStyleSystem::ResolveAll(Scene* scene)
	{
		if (!scene)
			return;

		auto& registry = scene->GetRegistry();

		for (auto entityID : registry.view<UIPanelComponent>())
			ResolveComponent(registry.get<UIPanelComponent>(entityID));

		for (auto entityID : registry.view<UIButtonComponent>())
			ResolveComponent(registry.get<UIButtonComponent>(entityID));

		for (auto entityID : registry.view<UITextComponent>())
			ResolveComponent(registry.get<UITextComponent>(entityID));
	}

	AssetHandle UIStyleSystem::CreateStyleSheet(const std::filesystem::path& relativePath)
	{
		auto fullPath = AssetManager::GetAssetDirectory() / relativePath;

		if (std::filesystem::exists(fullPath))
		{
			TOAST_CORE_WARN("UIStyleSystem: '%s' already exists, importing the existing file.", fullPath.string().c_str());
			return AssetManager::ImportAsset(relativePath);
		}

		std::filesystem::create_directories(fullPath.parent_path());

		StyleSheet templateSheet;
		templateSheet.SetTemplateDefaults();

		if (!templateSheet.SaveToFile(fullPath))
			return AssetHandle(0);
		
		AssetHandle handle = AssetManager::ImportAsset(relativePath);

		TOAST_CORE_INFO("UIStyleSystem: Created stylesheed '%s' (handle : % llu)", relativePath.string().c_str(), (uint64_t)handle);

		return handle;
	}

	Toast::AssetHandle UIStyleSystem::CreateStyleSheetFromComponent(Entity entity, const std::filesystem::path& relativePath)
	{
		auto fullPath = AssetManager::GetAssetDirectory() / relativePath;

		if (std::filesystem::exists(fullPath))
		{
			TOAST_CORE_WARN("UIStyleSystem: '%s' already exists, attaching the existing file instead.", relativePath.string().c_str());
			return AssetManager::ImportAsset(relativePath);
		}

		std::filesystem::create_directories(fullPath.parent_path());

		StyleSheet sheet;

		if (entity.HasComponent<UIPanelComponent>())
			sheet.SetBlock(CaptureFromComponent(entity.GetComponent<UIPanelComponent>()));
		else if (entity.HasComponent<UIButtonComponent>())
			sheet.SetBlock(CaptureFromComponent(entity.GetComponent<UIButtonComponent>()));
		else if (entity.HasComponent<UITextComponent>())
			sheet.SetBlock(CaptureFromComponent(entity.GetComponent<UITextComponent>()));
		else
			return AssetHandle(0);

		if (!sheet.SaveToFile(fullPath))
			return AssetHandle(0);

		AssetHandle handle = AssetManager::ImportAsset(relativePath);

		TOAST_CORE_INFO("UIStyleSystem: Created stylesheet '%s' from entity '%s' (handle: %llu)", relativePath.string().c_str(), entity.GetComponent<TagComponent>().Tag.c_str(), (uint64_t)handle);

		return handle;
	}

	void UIStyleSystem::ResetUnoverridden(UIPanelComponent& component)
	{
		const UIPanelComponent defaults;
		const uint32_t overrides = component.Style.Overrides;

		if (!(overrides & UIStyleProp_Background))
			component.Color = defaults.Color;
		if (!(overrides & UIStyleProp_CornerRadius))
			component.CornerRadius = defaults.CornerRadius;
		if (!(overrides & UIStyleProp_Visible))
			component.Visible = defaults.Visible;
		if (!(overrides & UIStyleProp_UseColor))
			component.UseColor = defaults.UseColor;
		if (!(overrides & UIStyleProp_BorderWidth))
			component.BorderWidth = defaults.BorderWidth;
		if (!(overrides & UIStyleProp_BorderColor))
			component.BorderColor = defaults.BorderColor;

		if (!(overrides & UIStyleProp_BackgroundImage))
		{	
			component.TextureHandle = defaults.TextureHandle;
			component.TextureIndex = defaults.TextureIndex;
		}
	}

	void UIStyleSystem::ResetUnoverridden(UIButtonComponent& component)
	{
		const UIButtonComponent defaults;
		const uint32_t overrides = component.Style.Overrides;

		for (uint32_t i = 0; i < (uint32_t)UIState::Count; i++)
		{
			const UIState state = (UIState)i;

			if (!(overrides & StatePropBit(UIStyleProp_Background, state)))
				component.States[i].Color = defaults.States[i].Color;

			if (!(overrides & StatePropBit(UIStyleProp_BackgroundImage, state)))
			{
				component.States[i].TextureHandle = defaults.States[i].TextureHandle;
				component.States[i].TextureIndex = defaults.States[i].TextureIndex;
			}
		}

		if (!(overrides & UIStyleProp_CornerRadius))
			component.CornerRadius = defaults.CornerRadius;
		if (!(overrides & UIStyleProp_Visible))
			component.Visible = defaults.Visible;
		if (!(overrides & UIStyleProp_UseColor))
			component.UseColor = defaults.UseColor;
		if (!(overrides & UIStyleProp_Transition))
			component.TransitionSeconds = defaults.TransitionSeconds;
		if (!(overrides & UIStyleProp_BorderWidth))
			component.BorderWidth = defaults.BorderWidth;
		if (!(overrides & UIStyleProp_BorderColor))
			component.BorderColor = defaults.BorderColor;
	}

	void UIStyleSystem::ResetUnoverridden(UITextComponent& component)
	{
		const UITextComponent defaults;
		const uint32_t overrides = component.Style.Overrides;

		if (!(overrides & UIStyleProp_Color))
			component.Color = defaults.Color;

		if (!(overrides & UIStyleProp_Visible))
			component.Visible = defaults.Visible;

		if (!(overrides & UIStyleProp_FontSize))
			component.FontSize = defaults.FontSize;

		if (!(overrides & UIStyleProp_TextAlign))
		{
			component.AlignH = defaults.AlignH;
			component.AlignV = defaults.AlignV;
		}
	}

	void UIStyleSystem::ResetUnoverridden(UIImageComponent& component)
	{
		const UIImageComponent defaults;
		const uint32_t overrides = component.Style.Overrides;

		if (!(overrides & UIStyleProp_Tint))
			component.Tint = defaults.Tint;

		if (!(overrides & UIStyleProp_ImageFit))
			component.Fit = defaults.Fit;

		if (!(overrides & UIStyleProp_CornerRadius))
			component.CornerRadius = defaults.CornerRadius;

		if (!(overrides & UIStyleProp_Visible))
			component.Visible = defaults.Visible;
	}

	void UIStyleSystem::StartFileWatcher()
	{
		auto assetDir = AssetManager::GetAssetDirectory();

		sData->Watcher = CreateScope<filewatch::FileWatch<std::string>>(assetDir.string(), OnStyleSheetFileSystemEvent);

		TOAST_CORE_INFO("UIStyleSystem: Watching '%s' for stylesheet changes", assetDir.string().c_str());
	}

	void UIStyleSystem::StopFileWatcher()
	{
		sData->Watcher.reset();
	}

	void UIStyleSystem::ReloadAllStyleSheets()
	{
		if (!sData->ActiveScene)
			return;

		// ⚠ Reload before resolve. Resolving first applies the stale block and
		// looks exactly like the watcher never fired.
		AssetManager::Each(AssetType::StyleSheet, [](AssetHandle handle, const AssetMetadata& metadata)
			{
				if (AssetManager::IsAssetLoaded(handle))
					AssetManager::ReloadAsset(handle);
			});

		ResolveAll(sData->ActiveScene);
	}

	const StyleBlock* UIStyleSystem::GetBlock(AssetHandle sheet)
	{
		if (sheet == AssetHandle(0))
			return nullptr;

		auto styleSheet = AssetManager::GetAsset<StyleSheet>(sheet);
		if (!styleSheet)
			return nullptr;

		return &styleSheet->GetBlock();

	}

	void UIStyleSystem::ApplyStyle(UIPanelComponent& component)
	{
		const StyleBlock* block = GetBlock(component.Style.Sheet);
		if (!block)
			return;

		const uint32_t overrides = component.Style.Overrides;

		if (block->BackgroundState[(size_t)UIState::Normal].Set && !(overrides & UIStyleProp_Background))
			component.Color = block->BackgroundState[(size_t)UIState::Normal].Value;

		if (block->CornerRadius.Set && !(overrides & UIStyleProp_CornerRadius))
			component.CornerRadius = block->CornerRadius.Value;

		if (block->Visible.Set && !(overrides & UIStyleProp_Visible))
			component.Visible = block->Visible.Value;

		if (block->UseColor.Set && !(overrides & UIStyleProp_UseColor))
			component.UseColor = block->UseColor.Value;

		if (block->BorderWidth.Set && !(overrides & UIStyleProp_BorderWidth))
			component.BorderWidth = block->BorderWidth.Value;

		if (block->BorderColor.Set && !(overrides & UIStyleProp_BorderColor))
			component.BorderColor = block->BorderColor.Value;

		if (block->BackgroundImageState[(size_t)UIState::Normal].Set && !(overrides & UIStyleProp_BackgroundImage))
		{
			component.TextureHandle = block->BackgroundImageState[(size_t)UIState::Normal].Value;
			component.TextureIndex = Renderer2D::GetRendererData()->UITextureArray->GetSliceIndexForHandle(component.TextureHandle);
		}
	}

	void UIStyleSystem::ApplyStyle(UIButtonComponent& component)
	{
		const StyleBlock* block = GetBlock(component.Style.Sheet);
		if (!block)
			return;

		const uint32_t overrides = component.Style.Overrides;

		for (size_t i = 0; i < (size_t)UIState::Count; i++)
		{
			const auto& src = block->BackgroundState[i].Set ? block->BackgroundState[i] : block->BackgroundState[0];

			if (src.Set && !(overrides & UIStyleProp_Background))
				component.States[i].Color = src.Value;
		}

		if (block->CornerRadius.Set && !(overrides & UIStyleProp_CornerRadius))
			component.CornerRadius = block->CornerRadius.Value;

		if (block->Visible.Set && !(overrides & UIStyleProp_Visible))
			component.Visible = block->Visible.Value;

		if (block->UseColor.Set && !(overrides & UIStyleProp_UseColor))
			component.UseColor = block->UseColor.Value;

		if (block->TransitionSeconds.Set && !(overrides & UIStyleProp_Transition))
			component.TransitionSeconds = block->TransitionSeconds.Value;

		if (block->BorderWidth.Set && !(overrides & UIStyleProp_BorderWidth))
			component.BorderWidth = block->BorderWidth.Value;

		if (block->BorderColor.Set && !(overrides & UIStyleProp_BorderColor))
			component.BorderColor = block->BorderColor.Value;

		component.Blended = component.States[(size_t)component.CurrentState];
		component.BlendFrom = component.Blended;
		component.StateBlend = 1.0f;
	}

	void UIStyleSystem::ApplyStyle(UITextComponent& component)
	{
		const StyleBlock* block = GetBlock(component.Style.Sheet);
		if (!block)
			return;

		const uint32_t overrides = component.Style.Overrides;

		if (block->Color.Set && !(overrides & UIStyleProp_Color))
			component.Color = block->Color.Value;

		if (block->Visible.Set && !(overrides & UIStyleProp_Visible))
			component.Visible = block->Visible.Value;

		if (block->FontSize.Set && !(overrides & UIStyleProp_FontSize))
			component.FontSize = block->FontSize.Value;

		if(!(overrides & UIStyleProp_TextAlign))
		{
			if (block->AlignH.Set)
				component.AlignH = block->AlignH.Value;

			if (block->AlignV.Set)
				component.AlignV = block->AlignV.Value;
		}
	}

	void UIStyleSystem::ApplyStyle(UIImageComponent& component)
	{
		const StyleBlock* block = GetBlock(component.Style.Sheet);
		if (!block)
			return;

		const uint32_t overrides = component.Style.Overrides;

		if (block->Tint.Set && !(overrides & UIStyleProp_Tint))
			component.Tint = block->Tint.Value;

		if (block->Fit.Set && !(overrides & UIStyleProp_ImageFit))
			component.Fit = block->Fit.Value;

		if (block->CornerRadius.Set && !(overrides & UIStyleProp_CornerRadius))
			component.CornerRadius = block->CornerRadius.Value;

		if (block->Visible.Set && !(overrides & UIStyleProp_Visible))
			component.Visible = block->Visible.Value;
	}

	StyleBlock UIStyleSystem::CaptureFromComponent(const UIPanelComponent& component)
	{
		StyleBlock block;

		block.BackgroundState[(size_t)UIState::Normal].Assign(component.Color);

		block.CornerRadius.Assign(component.CornerRadius);
		block.Visible.Assign(component.Visible);
		block.UseColor.Assign(component.UseColor);

		if (component.BorderWidth > 0.0f)
		{
			block.BorderWidth.Assign(component.BorderWidth);
			block.BorderColor.Assign(component.BorderColor);
		}

		if (component.TextureHandle != AssetHandle(0))
			block.BackgroundImageState[(size_t)UIState::Normal].Assign(component.TextureHandle);

		return block;
	}

	StyleBlock UIStyleSystem::CaptureFromComponent(const UIButtonComponent& component)
	{
		StyleBlock block;

		for (uint32_t i = 0; i < (uint32_t)UIState::Count; i++)
		{
			block.BackgroundState[i].Assign(component.States[i].Color);

			if (component.States[i].TextureHandle != AssetHandle(0))
				block.BackgroundImageState[i].Assign(component.States[i].TextureHandle);
		}

		block.CornerRadius.Assign(component.CornerRadius);
		block.Visible.Assign(component.Visible);
		block.UseColor.Assign(component.UseColor);

		if (component.TransitionSeconds > 0.0f)
			block.TransitionSeconds.Assign(component.TransitionSeconds);

		if (component.BorderWidth > 0.0f)
		{
			block.BorderWidth.Assign(component.BorderWidth);
			block.BorderColor.Assign(component.BorderColor);
		}

		return block;
	}

	StyleBlock UIStyleSystem::CaptureFromComponent(const UITextComponent& component)
	{
		StyleBlock block;

		block.Color.Assign(component.Color);
		block.Visible.Assign(component.Visible);

		block.FontSize.Assign(component.FontSize);
		block.AlignH.Assign(component.AlignH);
		block.AlignV.Assign(component.AlignV);

		return block;
	}

	StyleBlock UIStyleSystem::CaptureFromComponent(const UIImageComponent& component)
	{
		StyleBlock block;

		block.Tint.Assign(component.Tint);
		block.Fit.Assign(component.Fit);
		block.CornerRadius.Assign(component.CornerRadius);
		block.Visible.Assign(component.Visible);

		return block;
	}

}