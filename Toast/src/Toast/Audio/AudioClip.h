#pragma once

#include "Toast/Assets/Asset.h"

#include <windows.h>

#include <filesystem>
#include <vector>

namespace Toast {

	class AudioClip : public Asset 
	{
	public:
		AudioClip() = default;
		explicit AudioClip(const std::filesystem::path& filepath);

		AssetType GetAssetType() const { return AssetType::AudioClip; }

		void SetPCMData(uint16_t formatTag, uint16_t channels, uint32_t sampleRate, uint16_t bitsPerSample, std::vector<uint8_t> data);

		const WAVEFORMATEX& GetFormat() const { return mFormat; }
		const std::vector<uint8_t>& GetData() const { return mData; }
	private:
		bool LoadFromWav(const std::filesystem::path& filepath);

		WAVEFORMATEX mFormat = {};
		std::vector<uint8_t> mData;
	};

}