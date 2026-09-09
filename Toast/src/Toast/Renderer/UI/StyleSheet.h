#pragma once

#include "Toast/Assets/Asset.h"

#include "Toast/Scene/Components.h"

#include <DirectXMath.h>
#include <filesystem>

namespace Toast {

	template<typename T>
	struct StyleValue
	{
		T Value{};
		bool Set = false;

		void Assign(const T& v) { Value = v; Set = true; }
	};

	struct StyleBlock
	{
		StyleValue<DirectX::XMFLOAT4> Color;
		StyleValue<float> CornerRadius;
		StyleValue<bool> Visible;
		StyleValue<bool> UseColor;
		StyleValue<float> TransitionSeconds;

		StyleValue<DirectX::XMFLOAT4> BackgroundState[(size_t)UIState::Count];
		StyleValue<AssetHandle> BackgroundImageState[(size_t)UIState::Count];
	};

	static_assert(std::is_trivially_copyable_v<StyleBlock>, "StyleBlock must stay POD - AssetSerializer writes it directly!");

	class StyleSheet : public Asset
	{
	public:
		StyleSheet() = default;

		bool ParseFromFile(const std::filesystem::path& filepath);
		bool SaveToFile(const std::filesystem::path& filepath) const;

		const StyleBlock& GetBlock() const { return mBlock; }

		// Only used by AssetSerializer::DeserializeStyleSheet when loading a
		// baked .tasset. The editor always populates mBlock by parsing.
		void SetBlock(const StyleBlock& block) { mBlock = block; }

		void SetTemplateDefaults()
		{
			mBlock = StyleBlock{};

			mBlock.BackgroundState[(size_t)UIState::Normal].Assign(DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));
			mBlock.CornerRadius.Assign(0.0f);
			mBlock.UseColor.Assign(true);
			mBlock.Visible.Assign(true);
		}

		virtual bool IsValid() const { return true; }

		virtual AssetType GetAssetType() const override { return AssetType::StyleSheet; }
	private:
		StyleBlock mBlock;
	};

}