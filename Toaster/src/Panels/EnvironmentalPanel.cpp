#include "EnvironmentalPanel.h"

#include "../FontAwesome.h"

#include "Toast/Assets/AssetManager.h"
#include "Toast/ImGui/ImGuiHelpers.h"

#include "Toast/Utils/PlatformUtils.h"

#include "imgui/imgui.h"

namespace Toast {

	static AssetHandle ImportLinearTexture(const std::filesystem::path& path)
	{
		const auto relativePath = std::filesystem::relative(path, AssetManager::GetAssetDirectory());
		const bool insideAssets = !relativePath.empty() && *relativePath.begin() != "..";

		const AssetHandle handle = insideAssets ? AssetManager::ImportAsset(relativePath) : AssetManager::ImportExternalAsset(path, "Textures");
		if (handle == AssetHandle(0))
			return handle;

		AssetEntry* entry = AssetManager::GetEntry(handle);
		if (entry && entry->Texture2DSettings.ForceSRGB)
		{
			entry->Texture2DSettings.ForceSRGB = false;
			if (AssetManager::IsAssetLoaded(handle))
				AssetManager::ReloadAsset(handle);
		}

		return handle;
	}

	EnvironmentalPanel::EnvironmentalPanel(Scene* context)
	{
		SetContext(context);
	}

	void EnvironmentalPanel::SetContext(Scene* context)
	{
		mContext = context;
	}

	void EnvironmentalPanel::OnImGuiRender()
	{
		ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV;
		ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

		Scene::Environment& environment = mContext->GetEnvirontment();

		ImGui::Begin(ICON_TOASTER_CLOUD " Environment");

		// ---------------- Sun ----------------
		ImGui::Separator();
		ImGui::Text("Lightning Gains");

		ImGui::Text("Diffuse IBL Gain");
		ImGui::DragFloat("##DiffuseIBLGain", &environment.DiffuseIBLGain, 0.1f, 0.0f, FLT_MAX, "%.1f");

		ImGui::Text("Specular IBL Gain");
		ImGui::DragFloat("##SpecularIBLGain", &environment.SpecularIBLGain, 0.1f, 0.0f, FLT_MAX, "%.1f");

		ImGui::Text("Night Sky Light");
		ImGui::DragFloat("##NightSkyLight", &environment.NightSkyLight, 0.0001f, 0.0f, 0.05f, "%.5f");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Starlight on the ground at night (uniform sky radiance).\n0.0015 = ground dark but readable at Min EV -6.");

		// ---------------- Sun ----------------
		ImGui::Separator();
		ImGui::Text("Sun");

		ImGui::Text("Sun Disc Toggle");
		ImGui::Checkbox("##SunDiscToggle", &environment.SunDiscToggle);

		ImGui::Text("Sun Intensity");
		ImGui::DragFloat("##SunIntensity", &environment.SunIntensity, 0.1f, 0.0f, FLT_MAX, "%.1f");

		ImGui::Text("Sun Disc Radius (rad)");
		ImGui::DragFloat("##SunDiscRadius", &environment.SunDiscRadius, 0.000001f, 0.0f, FLT_MAX, "%.6f");


		// ------------- Starburst (FFT of a 6-blade aperture) -------------
		ImGui::Separator();
		ImGui::Text("Starburst");

		if (ImGui::BeginTable("##StarburstTextureTable", 2))
		{
			ImGui::TableSetupColumn("##Label", ImGuiTableColumnFlags_WidthFixed, 85.0f);
			ImGui::TableSetupColumn("##Value", ImGuiTableColumnFlags_WidthStretch);

			const std::filesystem::path assetRoot = AssetManager::GetAssetDirectory();
			std::string pickedPath;
			if (ImGuiHelpers::TextureSlotRow("Texture", environment.StarburstTextureHandle, assetRoot, assetRoot / "Textures", pickedPath))
			{
				const AssetHandle picked = ImportLinearTexture(pickedPath);
				if (picked != AssetHandle(0))
					environment.StarburstTextureHandle = picked;
			}

			if (environment.StarburstTextureHandle != AssetHandle(0))
			{
				ImGui::SameLine();
				if (ImGui::Button("X##ClearStarburstTexture"))
					environment.StarburstTextureHandle = AssetHandle(0);
			}

			ImGui::EndTable();
		}

		ImGui::Text("Size");
		ImGui::DragFloat("##StarburstSize", &environment.StarburstSize, 0.01f, 0.05f, 3.0f, "%.2f");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Sprite diameter as a fraction of the screen height. The rays end at its edge.");

		ImGui::Text("Intensity");
		ImGui::DragFloat("##StarburstIntensity", &environment.StarburstIntensity, 10.0f, 0.0f, 100000.0f, "%.0f");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Brightness of the rays. The sun's color and visibility (haze, limb, Starship) scale it further.");

		// ------------- Lens Glare and Ghost -------------
		ImGui::Separator();
		ImGui::Text("Lens Ghosts");

		ImGui::Text("Intensity");
		ImGui::DragFloat("##GhostIntensity", &environment.GhostIntensity, 0.01f, 0.0f, 10.0f, "%.2f");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Brightness of the ghosts. The sun's color and visibility (haze, limb, Starship, off screen) scale it further.");

		ImGui::Text("Scale");
		ImGui::DragFloat("##GhostScale", &environment.GhostScale, 0.01f, 0.1f, 5.0f, "%.2f");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Size of the ghosts (1 = the default layout).");

		// ---------------- Stars ----------------
		ImGui::Separator();
		ImGui::Text("Stars");

		ImGui::Text("Star Nits");
		ImGui::DragFloat("##StarNits", &environment.StarNits, 10.0f, 0.0f, FLT_MAX, "%.0f");

		ImGui::Text("Twilight Start (deg)");
		ImGui::DragFloat("##TwilightStartDeg", &environment.TwilightStartDeg, 0.1f, -60.0f, 60.0f, "%.1f");

		ImGui::Text("Twilight End (deg)");
		ImGui::DragFloat("##TwilightEndDeg", &environment.TwilightEndDeg, 0.1f, -60.0f, 60.0f, "%.1f");

		ImGui::Text("Space Fade Start");
		ImGui::DragFloat("##SpaceFadeStart", &environment.SpaceFadeStart, 0.01f, 0.0f, 1.0f, "%.2f");

		ImGui::Text("Space Fade End");
		ImGui::DragFloat("##SpaceFadeEnd", &environment.SpaceFadeEnd, 0.01f, 0.0f, 1.0f, "%.2f");

		ImGui::End();
	}

}