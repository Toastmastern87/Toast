#pragma once

#include "Toast/Core/Base.h"

#include <xaudio2.h>
#include <x3daudio.h>
#include <wrl.h>

namespace Toast {

	class AudioEngine
	{
	public:
		static constexpr float MaxFrequencyRatio = 4.0f;

		static void Init();
		static void Shutdown();

		static uint32_t AcquireVoice(const WAVEFORMATEX& format);
		static void ReleaseVoice(uint32_t slot);
		static IXAudio2SourceVoice* GetVoice(uint32_t slot);

		static void SetSpeedOfSound(float speedOfSound);

		static void SetListener(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& forward, const DirectX::XMFLOAT3& up, const DirectX::XMFLOAT3& velocity);

		static bool Apply3D(uint32_t slot, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& velocity, float minDistance, float dopplerScale, float& outDopplerFactor);
		static void Clear3D(uint32_t slot);
	private:
		static void InitX3DAudio(uint32_t outputChannels);
		                            
		static constexpr uint32_t VoicePoolSize = 32;

		struct VoiceSlot 
		{
			IXAudio2SourceVoice* Voice = nullptr;
			WAVEFORMATEX Format = {};
			bool InUse = false;
			bool Spatialized = false;
		};

		struct AudioData
		{
			Microsoft::WRL::ComPtr<IXAudio2> XAudio2Instance;
			IXAudio2MasteringVoice* MasteringVoice = nullptr;

			std::array<VoiceSlot, VoicePoolSize> VoicePool;

			X3DAUDIO_HANDLE X3DInstance = {};
			uint32_t ChannelMask = 0;
			float SpeedOfSound = 0.0f;
			uint32_t OutputChannels = 0;
			std::vector<float> MatrixCoefficients;
			X3DAUDIO_LISTENER Listener = {};
			bool X3DInitialized = false;
			 
			bool Initialized = false;
			bool ComInitializedByUs = false;
		};

		static AudioData sAudioData;
	};

}