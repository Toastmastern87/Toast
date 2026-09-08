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

		if (!(overrides & UIStyleProp_Background))
			component.Color = defaults.Color;
		if (!(overrides & UIStyleProp_CornerRadius))
			component.CornerRadius = defaults.CornerRadius;
		if (!(overrides & UIStyleProp_Visible))
			component.Visible = defaults.Visible;
		if (!(overrides & UIStyleProp_UseColor))
			component.UseColor = defaults.UseColor;

		if (!(overrides & UIStyleProp_BackgroundImage))
		{
			component.TextureHandle = defaults.TextureHandle;
			component.TextureIndex = defaults.TextureIndex;
		}

		if (!(overrides & UIStyleProp_BackgroundImageClick))
		{
			component.ClickTextureHandle = defaults.ClickTextureHandle;
			component.ClickTextureIndex = defaults.ClickTextureIndex;
		}
	}

	void UIStyleSystem::ResetUnoverridden(UITextComponent& component)
	{
		const UITextComponent defaults;
		const uint32_t overrides = component.Style.Overrides;

		if (!(overrides & UIStyleProp_Background))
			component.Color = defaults.Color;

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

		if(block->Background.Set && !(overrides & UIStyleProp_Background))
			component.Color = block->Background.Value;

		if (block->CornerRadius.Set && !(overrides & UIStyleProp_CornerRadius))
			component.CornerRadius = block->CornerRadius.Value;

		if (block->Visible.Set && !(overrides & UIStyleProp_Visible))
			component.Visible = block->Visible.Value;

		if (block->UseColor.Set && !(overrides & UIStyleProp_UseColor))
			component.UseColor = block->UseColor.Value;

		if (block->BackgroundImage.Set && !(overrides & UIStyleProp_Background))
		{	
			component.TextureHandle = block->BackgroundImage.Value;
			component.TextureIndex = Renderer2D::GetRendererData()->UITextureArray->GetSliceIndexForHandle(component.TextureHandle);
		}
	}

	void UIStyleSystem::ApplyStyle(UIButtonComponent& component)
	{
		const StyleBlock* block = GetBlock(component.Style.Sheet);
		if (!block)
			return;

		const uint32_t overrides = component.Style.Overrides;

		if (block->Background.Set && !(overrides & UIStyleProp_Background))
			component.Color = block->Background.Value;

		if (block->BackgroundClick.Set && !(overrides & UIStyleProp_BackgroundClick))
			component.ClickColor = block->Background.Value;

		if (block->CornerRadius.Set && !(overrides & UIStyleProp_CornerRadius))
			component.CornerRadius = block->CornerRadius.Value;

		if (block->Visible.Set && !(overrides & UIStyleProp_Visible))
			component.Visible = block->Visible.Value;

		if (block->UseColor.Set && !(overrides & UIStyleProp_UseColor))
			component.UseColor = block->UseColor.Value;

		if (block->BackgroundImage.Set && !(overrides & UIStyleProp_Background))
		{
			component.TextureHandle = block->BackgroundImage.Value;
			component.TextureIndex = Renderer2D::GetRendererData()->UITextureArray->GetSliceIndexForHandle(component.TextureHandle);
		}

		if (block->BackgroundClickImage.Set && !(overrides & UIStyleProp_BackgroundImageClick))
		{
			component.ClickTextureHandle = block->BackgroundClickImage.Value;
			component.ClickTextureIndex = Renderer2D::GetRendererData()->UITextureArray->GetSliceIndexForHandle(component.ClickTextureHandle);
		}
	}

	void UIStyleSystem::ApplyStyle(UITextComponent& component)
	{
		const StyleBlock* block = GetBlock(component.Style.Sheet);
		if (!block)
			return;

		const uint32_t overrides = component.Style.Overrides;

		if (block->Background.Set && !(overrides & UIStyleProp_Background))
			component.Color = block->Background.Value;

		if (block->Visible.Set && !(overrides & UIStyleProp_Visible))
			component.Visible = block->Visible.Value;
	}

	StyleBlock UIStyleSystem::CaptureFromComponent(const UIPanelComponent& component)
	{
		StyleBlock block;

		block.Background.Assign(component.Color);
		block.CornerRadius.Assign(component.CornerRadius);
		block.Visible.Assign(component.Visible);
		block.UseColor.Assign(component.UseColor);

		if (component.TextureHandle != AssetHandle(0))
			block.BackgroundImage.Assign(component.TextureHandle);

		return block;
	}

	StyleBlock UIStyleSystem::CaptureFromComponent(const UIButtonComponent& component)
	{
		StyleBlock block;

		block.Background.Assign(component.Color);
		block.BackgroundClick.Assign(component.ClickColor);
		block.CornerRadius.Assign(component.CornerRadius);
		block.Visible.Assign(component.Visible);
		block.UseColor.Assign(component.UseColor);

		if (component.TextureHandle != AssetHandle(0))
			block.BackgroundImage.Assign(component.TextureHandle);

		if (component.ClickTextureHandle != AssetHandle(0))
			block.BackgroundClickImage.Assign(component.ClickTextureHandle);

		return block;
	}

	StyleBlock UIStyleSystem::CaptureFromComponent(const UITextComponent& component)
	{
		StyleBlock block;

		block.Background.Assign(component.Color);
		block.Visible.Assign(component.Visible);

		return block;
	}

}