#include "tpch.h"
#include "AudioEngine.h"

namespace Toast {

	namespace {
	
		X3DAUDIO_VECTOR ToX3DVector(const DirectX::XMFLOAT3& v)
		{
			return{ v.x, v.y, v.z };
		}

		bool IsFinite(const DirectX::XMFLOAT3& v)
		{
			return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
		}
		
	}

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

		InitX3DAudio(details.InputChannels);

		sAudioData.Initialized = true;

		TOAST_CORE_INFO("AudioEngine: initialized, %d output channels at %d Hz", details.InputChannels, details.InputSampleRate);
	}

	void AudioEngine::InitX3DAudio(uint32_t outputChannels)
	{
		DWORD channelMask = 0;
		HRESULT hr = sAudioData.MasteringVoice->GetChannelMask(&channelMask);
		if (FAILED(hr) || channelMask == 0)
		{
			TOAST_CORE_WARN("AudioEngine: no speaker channel mask, 3D audio disabled");
			return;
		}

		hr = X3DAudioInitialize(channelMask, X3DAUDIO_SPEED_OF_SOUND, sAudioData.X3DInstance);
		if (FAILED(hr))
		{
			TOAST_CORE_WARN("AudioEngine: X3DAudioInitialize failed, 3D audio disabled");
			return;
		}

		sAudioData.ChannelMask = channelMask;
		sAudioData.SpeedOfSound = X3DAUDIO_SPEED_OF_SOUND;

		sAudioData.OutputChannels = outputChannels;
		sAudioData.MatrixCoefficients.assign(outputChannels, 0.0f);

		sAudioData.Listener = {};
		sAudioData.Listener.OrientFront = { 0.0f, 0.0f, 1.0f };
		sAudioData.Listener.OrientTop = { 0.0f, 1.0f, 0.f };
	
		sAudioData.X3DInitialized = true;

		TOAST_CORE_INFO("AudioEngine: X3DAudio initialized, speaker channel mask 0x%X", sAudioData.ChannelMask);
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
			slot.Spatialized = false;
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

		sAudioData.X3DInitialized = false;
		sAudioData.MatrixCoefficients.clear();
		sAudioData.OutputChannels = 0;
		sAudioData.SpeedOfSound = 0.0f;
		sAudioData.ChannelMask = 0;

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
		slot.Spatialized = false;
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

		Clear3D(slot);
		voiceSlot.InUse = false;
	}

	IXAudio2SourceVoice* AudioEngine::GetVoice(uint32_t slot)
	{
		if (slot >= VoicePoolSize)
			return nullptr;

		return sAudioData.VoicePool[slot].Voice;
	}

	void AudioEngine::SetSpeedOfSound(float speedOfSound)
	{
		if (!sAudioData.X3DInitialized || speedOfSound <= 0.0f || speedOfSound == sAudioData.SpeedOfSound)
			return;

		if (FAILED(X3DAudioInitialize(sAudioData.ChannelMask, speedOfSound, sAudioData.X3DInstance)))
		{
			TOAST_CORE_WARN("AudioEngine: could not set speed of sound to %.1f m/s", speedOfSound);
			return;
		}

		sAudioData.SpeedOfSound = speedOfSound;
	}

	void AudioEngine::SetListener(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& forward, const DirectX::XMFLOAT3& up, const DirectX::XMFLOAT3& velocity)
	{
		if (!sAudioData.X3DInitialized)
			return;

		if (!IsFinite(position) || !IsFinite(forward) || !IsFinite(up) || !IsFinite(velocity))
		{
			TOAST_CORE_WARN("AudioEngine: listener has a non-finite value, keeping the previous listener");
			return;
		}

		X3DAUDIO_LISTENER& listener = sAudioData.Listener;
		listener.Position = ToX3DVector(position);
		listener.Velocity = ToX3DVector(velocity);
		listener.OrientFront = ToX3DVector(forward);
		listener.OrientTop = ToX3DVector(up);
	}

	bool AudioEngine::Apply3D(uint32_t slot, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& velocity, float minDistance, float dopplerScale, float& outDopplerFactor)
	{
		outDopplerFactor = 1.0f;

		if (!sAudioData.X3DInitialized || slot >= VoicePoolSize)
			return false;

		VoiceSlot& voiceSlot = sAudioData.VoicePool[slot];
		if (!voiceSlot.InUse || voiceSlot.Voice == nullptr)
			return false;

		// X3DAudio positions one point; a stereo clip has no single position
		if (voiceSlot.Format.nChannels != 1)
			return false;

		if (!IsFinite(position) || !IsFinite(velocity) || !std::isfinite(minDistance) || !std::isfinite(dopplerScale))
			return false;

		X3DAUDIO_EMITTER emitter = {};
		emitter.OrientFront = { 0.0f, 0.0f, 1.0f };
		emitter.OrientTop = { 0.0f, 1.0f, 0.0f };
		emitter.Position = ToX3DVector(position);
		emitter.Velocity = ToX3DVector(velocity);
		emitter.ChannelCount = 1;
		emitter.CurveDistanceScaler = std::max(minDistance, FLT_MIN);
		emitter.DopplerScaler = std::max(dopplerScale, 0.0f);

		X3DAUDIO_DSP_SETTINGS dsp = {};
		dsp.SrcChannelCount = 1;
		dsp.DstChannelCount = sAudioData.OutputChannels;
		dsp.pMatrixCoefficients = sAudioData.MatrixCoefficients.data();

		X3DAudioCalculate(sAudioData.X3DInstance, &sAudioData.Listener, &emitter, X3DAUDIO_CALCULATE_MATRIX | X3DAUDIO_CALCULATE_DOPPLER, &dsp);

		if (FAILED(voiceSlot.Voice->SetOutputMatrix(sAudioData.MasteringVoice, 1, sAudioData.OutputChannels, dsp.pMatrixCoefficients)))
			return false;

		voiceSlot.Spatialized = true;
		outDopplerFactor = dsp.DopplerFactor;
		return true;
	}

	void AudioEngine::Clear3D(uint32_t slot)
	{
		if (slot >= VoicePoolSize)
			return;

		VoiceSlot& voiceSlot = sAudioData.VoicePool[slot];
		if (!voiceSlot.Spatialized || voiceSlot.Voice == nullptr)
			return;

		std::vector<float>& levels = sAudioData.MatrixCoefficients;
		std::fill(levels.begin(), levels.end(), 0.0f);

		constexpr uint32_t frontLeftRight = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
		if ((sAudioData.ChannelMask & frontLeftRight) == frontLeftRight)
			levels[0] = levels[1] = 1.0f;
		else
			levels[0] = 1.0f;

		voiceSlot.Voice->SetOutputMatrix(sAudioData.MasteringVoice, 1, sAudioData.OutputChannels, levels.data());
		voiceSlot.Spatialized = false;
	}

}