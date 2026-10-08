#include "tpch.h"
#include "AudioEngine.h"

namespace Toast {

	constexpr float MaxFrequencyRatio = 4.0f;

	bool FormatsMatch(const WAVEFORMATEX& a, const WAVEFORMATEX& b)
	{
		return a.wFormatTag == b.wFormatTag
			&& a.nChannels == b.nChannels
			&& a.nSamplesPerSec == b.nSamplesPerSec
			&& a.wBitsPerSample == b.wBitsPerSample;
	}

	AudioEngine::AudioData AudioEngine::sAudioData;

	void AudioEngine::Init()
	{
		TOAST_PROFILE_FUNCTION();

		// Apartment-threaded (STA), matching FileDialogs::OpenFolder in
		// PlatformUtils.cpp. XAudio2 works under either model; IFileDialog does not.
		HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

		if (SUCCEEDED(hr))
			sAudioData.ComInitializedByUs = true;
		else if (hr == RPC_E_CHANGED_MODE)
		{
			sAudioData.ComInitializedByUs = false;
			TOAST_CORE_WARN("AudioEngine: COM already initialized as MTA, continuing (folder dialogs may fail)");
		}
		else
		{
			TOAST_CORE_ASSERT(false, "AudioEngine: CoInitializeEx failed");
			return;
		}

		hr = XAudio2Create(sAudioData.XAudio2Instance.GetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR);
		TOAST_CORE_ASSERT(SUCCEEDED(hr), "AudioEngine: XAudio2Create failed");

#ifdef TOAST_DEBUG
		XAUDIO2_DEBUG_CONFIGURATION debugConfig = {};
		debugConfig.TraceMask = XAUDIO2_LOG_ERRORS | XAUDIO2_LOG_WARNINGS;
		debugConfig.BreakMask = XAUDIO2_LOG_ERRORS;
		sAudioData.XAudio2Instance->SetDebugConfiguration(&debugConfig, nullptr);
#endif

		hr = sAudioData.XAudio2Instance->CreateMasteringVoice(&sAudioData.MasteringVoice);
		TOAST_CORE_ASSERT(SUCCEEDED(hr), "AudioEngine: CreateMasteringVoice failed");

		XAUDIO2_VOICE_DETAILS details = {};
		sAudioData.MasteringVoice->GetVoiceDetails(&details);

		sAudioData.Initialized = true;

		TOAST_CORE_INFO("AudioEngine: initialized, %d output channels at %d Hz", details.InputChannels, details.InputSampleRate);
	}

	void AudioEngine::Shutdown()
	{
		TOAST_PROFILE_FUNCTION();

		if (!sAudioData.Initialized)
			return;

		if (sAudioData.XAudio2Instance)
			sAudioData.XAudio2Instance->StopEngine();

		for (VoiceSlot& slot : sAudioData.VoicePool)
		{
			if (slot.Voice != nullptr)
			{
				slot.Voice->DestroyVoice();
				slot.Voice = nullptr;
			}

			slot.Format = {};
			slot.InUse = false;
		}

		if (sAudioData.MasteringVoice != nullptr)
		{
			sAudioData.MasteringVoice->DestroyVoice();
			sAudioData.MasteringVoice = nullptr;
		}

		sAudioData.XAudio2Instance.Reset();

		if (sAudioData.ComInitializedByUs)
		{
			CoUninitialize();
			sAudioData.ComInitializedByUs = false;
		}

		sAudioData.Initialized = false;

		TOAST_CORE_INFO("AudioEngine: shutdown complete");
	}

	uint32_t AudioEngine::AcquireVoice(const WAVEFORMATEX& format)
	{
		TOAST_CORE_ASSERT(sAudioData.Initialized, "AudioEngine: AquireVoice called before Init()");

		auto& pool = sAudioData.VoicePool;

		// Pass 1: a free slot whose voice already uses this format
		for (uint32_t i = 0; i < VoicePoolSize; i++)
		{
			if (!pool[i].InUse && pool[i].Voice != nullptr && FormatsMatch(pool[i].Format, format))
			{
				pool[i].InUse = true;
				return i;
			}
		}

		// Pass 2: pick a free slot to build a voice in
		uint32_t target = UINT32_MAX;

		for (uint32_t i = 0; i < VoicePoolSize; i++)
		{
			if (!pool[i].InUse && pool[i].Voice == nullptr)
			{
				target = i;
				break;
			}
		}

		if (target == UINT32_MAX)
		{
			for (uint32_t i = 0; i < VoicePoolSize; i++)
			{
				if (!pool[i].InUse)
				{
					target = i;
					break;
				}
			}
		}

		if (target == UINT32_MAX)
		{
			TOAST_CORE_WARN("AudioEngine: voice pool exhausted, sound dropped");
			return UINT32_MAX;
		}

		VoiceSlot& slot = pool[target];

		if (slot.Voice != nullptr)
		{
			slot.Voice->DestroyVoice();
			slot.Voice = nullptr;
		}

		HRESULT hr = sAudioData.XAudio2Instance->CreateSourceVoice(&slot.Voice, &format, 0, MaxFrequencyRatio, nullptr, nullptr, nullptr);

		if (FAILED(hr))
		{
			slot.Voice = nullptr;
			TOAST_CORE_ERROR("AudioEngine: CreateSourceVoice failed for slot %d", target);
			return UINT32_MAX;
		}

		slot.Format = format;
		slot.InUse = true;

		return target;
	}

	void AudioEngine::ReleaseVoice(uint32_t slot)
	{
		if (slot >= VoicePoolSize)
			return;

		VoiceSlot& voiceSlot = sAudioData.VoicePool[slot];

		if (voiceSlot.Voice != nullptr)
		{
			voiceSlot.Voice->Stop();
			voiceSlot.Voice->FlushSourceBuffers();
		}

		voiceSlot.InUse = false;
	}

	IXAudio2SourceVoice* AudioEngine::GetVoice(uint32_t slot)
	{
		if (slot >= VoicePoolSize)
			return nullptr;

		return sAudioData.VoicePool[slot].Voice;
	}

}