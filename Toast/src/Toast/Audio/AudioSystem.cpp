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

	namespace {
	
		void ApplySettings(IXAudio2SourceVoice* voice, const SoundSettings& settings, float dopplerFactor)
		{
			voice->SetVolume(std::clamp(settings.Volume, 0.0f, 1.0f));
			const float pitch = std::clamp(settings.Pitch, AudioSystem::MinPitch, AudioSystem::MaxPitch);
			voice->SetFrequencyRatio(std::clamp(pitch * dopplerFactor, XAUDIO2_MIN_FREQ_RATIO, AudioEngine::MaxFrequencyRatio));
		}

		DirectX::XMFLOAT3 TrackVelocity(const DirectX::XMFLOAT3& position, DirectX::XMFLOAT3& lastPosition, bool& hasLastPosition, float dt, float speedOfSound)
		{
			DirectX::XMFLOAT3 velocity = { 0.0f, 0.0f, 0.0f };

			if (hasLastPosition && dt > 0.0f)
			{
				velocity = { (position.x - lastPosition.x) / dt, (position.y - lastPosition.y) / dt , (position.z - lastPosition.z) / dt };

				const float speedSq = velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z;
				if(speedSq >= speedOfSound * speedOfSound)
					velocity = { 0.0f, 0.0f, 0.0f };
			}

			lastPosition = position;
			hasLastPosition = true;
			return velocity;
		}

	}

	void AudioSystem::OnUpdate(Timestep ts, Camera* listenerCamera, const Quaternion& cameraRotation)
	{
		TOAST_PROFILE_FUNCTION();

		float speedOfSound = X3DAUDIO_SPEED_OF_SOUND;
		if (auto planet = mScene->GetPlanet())
			speedOfSound = planet->GetSpeedOfSound();
		AudioEngine::SetSpeedOfSound(speedOfSound);

		const float dt = ts.GetSeconds();

		if (listenerCamera)
			UpdateListener(dt, *listenerCamera, cameraRotation, speedOfSound);

		auto ubc = mScene->GetRegistry().view<UIButtonComponent>();
		for (auto entity : ubc)
		{
			Entity e{ entity, mScene };
			auto& button = e.GetComponent<UIButtonComponent>();
			UpdateSound(button.PressSound, button.PressPlayback, false, nullptr);
			UpdateSound(button.ReleaseSound, button.ReleasePlayback, false, nullptr);
		}

		auto ac = mScene->GetRegistry().view<AudioComponent>();
		for (auto entity : ac)
		{
			Entity e{ entity, mScene };
			auto& audio = e.GetComponent<AudioComponent>();

			const bool wasPlaying = audio.Playback.IsPlaying();

			if (audio.Spatial && listenerCamera)
			{
				const Emitter3D emitter = BuildEmitter(e, audio, *listenerCamera, dt, speedOfSound);
				UpdateSound(audio.Sound, audio.Playback, audio.Loop, &emitter);
			}
			else
			{
				audio.HasLastPosition = false;
				UpdateSound(audio.Sound, audio.Playback, audio.Loop, nullptr);
			}

			const bool justStarted = !wasPlaying && audio.Playback.IsPlaying();
			if(justStarted && audio.Spatial && audio.Playback.PlayingClip && audio.Playback.PlayingClip->GetFormat().nChannels != 1)
				TOAST_CORE_WARN("AudioSystem: clip %llu is not mono, spatial sounds need a mono clip; playing it in 2D", (uint64_t)audio.Sound.ClipHandle);
		}
	}

	void AudioSystem::StopAll()
	{
		auto buttons = mScene->GetRegistry().view<UIButtonComponent>();
		for (auto entity : buttons)
		{
			Entity e{ entity, mScene };
			e.GetComponent<UIButtonComponent>().PressPlayback.Stop();
			e.GetComponent<UIButtonComponent>().ReleasePlayback.Stop();
		}

		auto ac = mScene->GetRegistry().view<AudioComponent>();
		for (auto entity : ac)
		{
			Entity e{ entity, mScene };
			e.GetComponent<AudioComponent>().Playback.Stop();
		}
	}

	void AudioSystem::UpdateListener(float dt, Camera& camera, const Quaternion& cameraRotation, float speedOfSound)
	{
		const DirectX::XMFLOAT4X4 transform = camera.GetInvViewMatrix();

		const DirectX::XMFLOAT3 position = { transform._41, transform._42, transform._43 };

		const Vector3 forwardWS = camera.GetForwardVectorWS(cameraRotation);
		const Vector3 upWS = camera.GetUpVectorWS(cameraRotation);
		DirectX::XMFLOAT3 forward = { (float)forwardWS.x, (float)forwardWS.y, (float)forwardWS.z };
		DirectX::XMFLOAT3 up = { (float)upWS.x, (float)upWS.y, (float)upWS.z };

		const DirectX::XMFLOAT3 velocity = TrackVelocity(position, mLastListenerPosition, mHasLastListenerPosition, dt, speedOfSound);

		AudioEngine::SetListener(position, forward, up, velocity);
	}

	AudioSystem::Emitter3D AudioSystem::BuildEmitter(Entity entity, AudioComponent& audio, Camera& camera, float dt, float speedOfSound)
	{
		DirectX::XMFLOAT4X4 world;
		DirectX::XMStoreFloat4x4(&world, mScene->ComposeWorldTransform(entity));

		const DirectX::XMFLOAT3& worldTranslation = camera.GetWorldTranslation();

		Emitter3D emitter;
		emitter.Position = { world._41 + worldTranslation.x, world._42 + worldTranslation.x , world._43 + worldTranslation.z };

		if (!audio.Playback.IsPlaying())
			audio.HasLastPosition = false;

		emitter.Velocity = TrackVelocity(emitter.Position, audio.LastPosition, audio.HasLastPosition, dt, speedOfSound);
		emitter.MinDistance = audio.MinDistance;
		emitter.DopplerScale = audio.DopplerScale;

		return emitter;
	}

	void AudioSystem::UpdateSound(const SoundSettings& settings, SoundPlayback& playback, bool loop, const Emitter3D* emitter)
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
			else
				ApplySettings(voice, settings, ApplyEmitter(playback.VoiceSlot, emitter));
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
		buffer.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;

		ApplySettings(voice, settings, ApplyEmitter(slot, emitter));

		if (FAILED(voice->SubmitSourceBuffer(&buffer)) || FAILED(voice->Start(0)))
		{
			TOAST_CORE_ERROR("AudioSystem: failed to start sound clip %llu", (uint64_t)settings.ClipHandle);
			playback.Release();
		}
	}

	float AudioSystem::ApplyEmitter(uint32_t slot, const Emitter3D* emitter)
	{
		float dopplerFactor = 1.0f;

		if (!emitter || !AudioEngine::Apply3D(slot, emitter->Position, emitter->Velocity, emitter->MinDistance, emitter->DopplerScale, dopplerFactor))
			AudioEngine::Clear3D(slot);

		return dopplerFactor;
	}

}