#include "tpch.h"
#include "Sound.h"

#include "Toast/Audio/AudioEngine.h"
#include "Toast/Audio/AudioClip.h"

namespace Toast {

	SoundPlayback::~SoundPlayback() 
	{
		Release();
	}

	SoundPlayback::SoundPlayback(const SoundPlayback&)
	{
		// Copy constructor, a copy starts silent so no data set here.
	}

	SoundPlayback& SoundPlayback::operator=(const SoundPlayback& other)
	{
		if (this != &other)
		{
			Release();
			PlayRequested = false;
		}

		return *this;
	}

	SoundPlayback::SoundPlayback(SoundPlayback&& other) noexcept
		: PlayRequested(other.PlayRequested), VoiceSlot(other.VoiceSlot), PlayingClip(std::move(other.PlayingClip))
	{
		other.PlayRequested = false;
		other.VoiceSlot = UINT32_MAX;
	}

	SoundPlayback& SoundPlayback::operator=(SoundPlayback&& other) noexcept
	{
		if (this != &other)
		{
			Release();

			PlayRequested = other.PlayRequested;
			VoiceSlot = other.VoiceSlot;
			PlayingClip = std::move(other.PlayingClip);

			other.PlayRequested = false;
			other.VoiceSlot = UINT32_MAX;
		}

		return *this;
	}

	void SoundPlayback::Release() 
	{
		if (VoiceSlot != UINT32_MAX)
		{
			AudioEngine::ReleaseVoice(VoiceSlot);
			VoiceSlot = UINT32_MAX;
		}

		PlayingClip = nullptr;
	}

}