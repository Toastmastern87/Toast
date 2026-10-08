#pragma once

#include "Toast/Core/Base.h"
#include "Toast/Assets/Asset.h"

#include <cstdint>

namespace Toast {

	class AudioClip;

	struct SoundSettings 
	{
		AssetHandle ClipHandle = 0;
		float Volume = 1.0f;
		float Pitch = 1.0f;
	};

	struct SoundPlayback 
	{
		bool PlayRequested = false;

		uint32_t VoiceSlot = UINT32_MAX;
		Ref<AudioClip> PlayingClip;

		SoundPlayback() = default;
		~SoundPlayback();

		SoundPlayback(const SoundPlayback&);
		SoundPlayback& operator=(const SoundPlayback& other);

		SoundPlayback(SoundPlayback&& other) noexcept;
		SoundPlayback& operator=(SoundPlayback&& other) noexcept;

		// Stops and the sound and returns the voice to the pool.
		void Release();
	};

}