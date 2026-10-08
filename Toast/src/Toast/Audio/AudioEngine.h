#pragma once

#include "Toast/Core/Base.h"

#include <xaudio2.h>
#include <wrl.h>

namespace Toast {

	class AudioEngine
	{
	public:
		static void Init();
		static void Shutdown();

		static uint32_t AcquireVoice(const WAVEFORMATEX& format);
		static void ReleaseVoice(uint32_t slot);
		static IXAudio2SourceVoice* GetVoice(uint32_t slot);
	private:
		static constexpr uint32_t VoicePoolSize = 32;

		struct VoiceSlot 
		{
			IXAudio2SourceVoice* Voice = nullptr;
			WAVEFORMATEX Format = {};
			bool InUse = false;
		};

		struct AudioData
		{
			Microsoft::WRL::ComPtr<IXAudio2> XAudio2Instance;
			IXAudio2MasteringVoice* MasteringVoice = nullptr;

			std::array<VoiceSlot, VoicePoolSize> VoicePool;
			 
			bool Initialized = false;
			bool ComInitializedByUs = false;
		};

		static AudioData sAudioData;
	};

}