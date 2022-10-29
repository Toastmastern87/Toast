#pragma once

#include "Toast/Renderer/Texture.h"

#include <filesystem>

namespace Toast {

	struct MSDFData;

	class Font
	{
	public:
		Font(const std::string& filepath);
		~Font();

		Ref<Texture2D> GetFontAtlas() const { return mTextureAtlas; }
		const MSDFData* GetMSDFData() const { return mMSDFData; }
		const std::string& GetFilePath() { return mFilePath; }

		static void StaticInit();
		static Ref<Font> GetDefaultFont();
	private:
		std::string mFilePath = "";
		Ref<Texture2D> mTextureAtlas;
		MSDFData* mMSDFData = nullptr;
	};
}