#include "tpch.h"
#include "AudioClip.h"

#include <fstream>

namespace Toast {

	namespace {

		// WAV format tags
		constexpr uint16_t WavFormatPCM = 0x0001;
		constexpr uint16_t WavFormatFloat = 0x0003;
		constexpr uint16_t WavFormatExtensible = 0xFFFE;

		// Range XAudio2 accepts
		constexpr uint32_t MinSampleRate = 1000;
		constexpr uint32_t MaxSampleRate = 200000;

		uint16_t ReadU16(const uint8_t* p) { uint16_t v; memcpy(&v, p, sizeof(v)); return v; }
		uint32_t ReadU32(const uint8_t* p) { uint32_t v; memcpy(&v, p, sizeof(v)); return v; }

		bool ChunkIdIs(const uint8_t* p, const char* id) { return memcmp(p, id, 4) == 0; }

		const char* FormatTagName(uint16_t tag) { return tag == WavFormatFloat ? "float" : "PCM"; }

		std::vector<uint8_t> ConvertPCM24ToFloat(const uint8_t* source, size_t sampleCount)
		{
			std::vector<uint8_t> out(sampleCount * sizeof(float));

			for (size_t i = 0; i < sampleCount; i++)
			{
				const uint8_t* s = source + i * 3;

				const uint32_t packed = (uint32_t(s[0]) << 8) | (uint32_t(s[1]) << 16 | uint32_t(s[2]) << 24);
				const int32_t value = static_cast<int32_t>(packed) >> 8;

				const float sample = static_cast<float>(value) / 8388608.0f;
				memcpy(out.data() + i * sizeof(float), &sample, sizeof(float));
			}

			return out;
		}

	}

	AudioClip::AudioClip(const std::filesystem::path& filepath)
	{
		if (!LoadFromWav(filepath))
			SetFlags(AssetFlag::Invalid);
	}

	void AudioClip::SetPCMData(uint16_t formatTag, uint16_t channels, uint32_t sampleRate, uint16_t bitsPerSample, std::vector<uint8_t> data)
	{
		mFormat = {};
		mFormat.wFormatTag = formatTag;
		mFormat.nChannels = channels;
		mFormat.nSamplesPerSec = sampleRate;
		mFormat.wBitsPerSample = bitsPerSample;
		mFormat.nBlockAlign = static_cast<WORD>(channels * bitsPerSample / 8);
		mFormat.nAvgBytesPerSec = sampleRate * mFormat.nBlockAlign;
		mFormat.cbSize = 0;

		mData = std::move(data);
	}

	bool AudioClip::LoadFromWav(const std::filesystem::path& filepath)
	{
		TOAST_PROFILE_FUNCTION();

		const std::string name = filepath.filename().string();

		std::ifstream in(filepath, std::ios::binary);
		if (!in.is_open())
		{
			TOAST_CORE_ERROR("AudioClip: could not open '%s'", name.c_str());
			return false;
		}

		const size_t fileSize = static_cast<size_t>(std::filesystem::file_size(filepath));
		std::vector<uint8_t> file(fileSize);
		in.read(reinterpret_cast<char*>(file.data()), fileSize);

		if (!in)
		{
			TOAST_CORE_ERROR("AudioClip: failed to read '%s'", name.c_str());
			return false;
		}

		if (fileSize < 12 || !ChunkIdIs(&file[0], "RIFF") || !ChunkIdIs(&file[8], "WAVE"))
		{
			TOAST_CORE_ERROR("AudioClip: '%s' is not a WAV file", name.c_str());
			return false;
		}

		// walk the chunks
		const uint8_t* fmt = nullptr;
		uint32_t fmtSize = 0;
		const uint8_t* samples = nullptr;
		size_t sampleBytes = 0;

		size_t offset = 12;
		while (offset + 8 <= fileSize)
		{
			const uint8_t* chunk = &file[offset];
			const uint32_t chunkSize = ReadU32(chunk + 4);
			const size_t bodyStart = offset + 8;
			const size_t available = fileSize - bodyStart;

			if (ChunkIdIs(chunk, "fmt "))
			{
				if (chunkSize > available)
				{
					TOAST_CORE_ERROR("AudioClip: '%s' has a truncated fmt chunk", name.c_str());
					return false;
				}

				fmt = chunk + 8;
				fmtSize = chunkSize;
			}
			else if (ChunkIdIs(chunk, "data"))
			{
				samples = chunk + 8;
				sampleBytes = std::min(static_cast<size_t>(chunkSize), available);

				if (chunkSize > available)
					TOAST_CORE_WARN("AudioClip: '%s' data chunk is truncated, using the %zu bytes present", name.c_str(), available);
			}

			offset = bodyStart + static_cast<size_t>(chunkSize) + (chunkSize & 1);
		}

		if (!fmt || fmtSize < 16)
		{
			TOAST_CORE_ERROR("AudioClip: '%s' has no valid fmt chunk", name.c_str());
			return false;
		}

		if (!samples)
		{
			TOAST_CORE_ERROR("AudioClip: '%s' has no data chunk", name.c_str());
			return false;
		}

		uint16_t formatTag = ReadU16(fmt + 0);
		const uint16_t channels = ReadU16(fmt + 2);
		const uint32_t sampleRate = ReadU32(fmt + 4);
		const uint16_t bitsPerSample = ReadU16(fmt + 14);

		if (formatTag == WavFormatExtensible)
		{
			if (fmtSize < 40)
			{
				TOAST_CORE_ERROR("AudioClip: '%s' has a malformed extensible fmt chunk", name.c_str());
				return false;
			}

			formatTag = ReadU16(fmt + 24);
		}

		const bool isPCM16 = formatTag == WavFormatPCM && bitsPerSample == 16;
		const bool isPCM24 = formatTag == WavFormatPCM && bitsPerSample == 24;
		const bool isFloat32 = formatTag == WavFormatFloat && bitsPerSample == 32;

		if (!isPCM16 && !isPCM24 && !isFloat32)
		{
			TOAST_CORE_ERROR("AudioClip: '%s' is %u-bit format %u. Only 16-bit PCM, 24-bit PCM and 32-bit float are supported; re-export it as 16-bit PCM.", name.c_str(), bitsPerSample, formatTag);
			return false;
		}

		if (channels < 1 || channels > 2)
		{
			TOAST_CORE_ERROR("AudioClip: '%s' has %u channels; only mono and stereo are supported", name.c_str(), channels);
			return false;
		}

		if (sampleRate < MinSampleRate || sampleRate > MaxSampleRate)
		{
			TOAST_CORE_ERROR("AudioClip: '%s' has unsupported sample rate of %u Hz", name.c_str(), sampleRate);
			return false;
		}

		const size_t blockAlign = static_cast<size_t>(channels) * bitsPerSample / 8;
		const size_t usableBytes = sampleBytes - (sampleBytes % blockAlign);

		if (usableBytes == 0)
		{
			TOAST_CORE_ERROR("AudioClip: '%s' contains no audio", name.c_str());
			return false;
		}

		if (isPCM24)
		{
			const size_t sampleCount = usableBytes / 3;
			SetPCMData(WavFormatFloat, channels, sampleRate, 32, ConvertPCM24ToFloat(samples, sampleCount));
		}
		else
			SetPCMData(formatTag, channels, sampleRate, bitsPerSample, std::vector<uint8_t>(samples, samples + usableBytes));

		const float seconds = static_cast<float>(usableBytes / blockAlign) / static_cast<float>(sampleRate);
		TOAST_CORE_INFO("AudioClip: loaded '%s' (%u ch, %u Hz, %u-bit %s%s, %.2f s)", name.c_str(), channels, sampleRate, bitsPerSample, FormatTagName(formatTag), isPCM24 ? ", converted to 32-bit float" : "", seconds);

		return true;
	}

}