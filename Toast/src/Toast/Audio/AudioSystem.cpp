#include "tpch.h"
#include "AudioSystem.h"

#include "Toast/Audio/AudioEngine.h"
#include "Toast/Audio/AudioClip.h"
#include "Toast/Audio/Sound.h"

#include "Toast/Assets/AssetManager.h"

#include "Toast/Scene/Scene.h"
#include "Toast/Scene/Entity.h"
#include "Toast/Scene/Components.h"

namespace Toast {

	void AudioSystem::OnUpdate()
	{
		TOAST_PROFILE_FUNCTION();

		auto buttons = mScene->GetRegistry().view<UIButtonComponent>();
		for (auto entity : buttons)
		{
			Entity e{ entity, mScene };
			auto& button = e.GetComponent<UIButtonComponent>();
			UpdateSound(button.ClickSound, button.ClickPlayback);
		}
	}

	void AudioSystem::StopAll()
	{
		auto buttons = mScene->GetRegistry().view<UIButtonComponent>();
		for (auto entity : buttons)
		{
			Entity e{ entity, mScene };
			auto& playback = e.GetComponent<UIButtonComponent>().ClickPlayback;
			playback.Release();
			playback.PlayRequested = false;
		}
	}

	void AudioSystem::UpdateSound(const SoundSettings& settings, SoundPlayback& playback)
	{
		// Free voices which sounds has finished
		if (playback.VoiceSlot != UINT32_MAX)
		{
			IXAudio2SourceVoice* voice = AudioEngine::GetVoice(playback.VoiceSlot);

			XAUDIO2_VOICE_STATE state = {};
			if (voice)
				voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);

			if (!voice || state.BuffersQueued == 0)
				playback.Release();
		}

		// Start new sounds that has been requested during the frame
		if (!playback.PlayRequested)
			return;

		playback.PlayRequested = false;

		if (settings.ClipHandle == 0)
			return;

		Ref<AudioClip> clip = AssetManager::GetAsset<AudioClip>(settings.ClipHandle);
		if (!clip)
		{
			TOAST_CORE_WARN("AudioSystem: sound clip %llu could not be loaded", (uint64_t)settings.ClipHandle);
			return;
		}

		// A new request on a sound that is already playing restarts it
		playback.Release();

		const uint32_t slot = AudioEngine::AcquireVoice(clip->GetFormat());
		if (slot == UINT32_MAX)
			return;

		playback.VoiceSlot = slot;
		playback.PlayingClip = clip;

		IXAudio2SourceVoice* voice = AudioEngine::GetVoice(slot);

		XAUDIO2_BUFFER buffer = {};
		buffer.AudioBytes = static_cast<uint32_t>(clip->GetData().size());
		buffer.pAudioData = clip->GetData().data();
		buffer.Flags = XAUDIO2_END_OF_STREAM;

		voice->SetVolume(std::clamp(settings.Volume, 0.0f, 1.0f));
		voice->SetFrequencyRatio(std::clamp(settings.Pitch, MinPitch, MaxPitch));

		if (FAILED(voice->SubmitSourceBuffer(&buffer)) || FAILED(voice->Start(0)))
		{
			TOAST_CORE_ERROR("AudioSystem: failed to start sound clip %llu", (uint64_t)settings.ClipHandle);
			playback.Release();
		}
	}

}