#pragma once

namespace Toast {

	class Scene;
	struct SoundSettings;
	struct SoundPlayback;

	class AudioSystem
	{
	public:
		static constexpr float MinPitch = 0.5f;
		static constexpr float MaxPitch = 2.0f;

		explicit AudioSystem(Scene* scene) : mScene(scene) {}

		void OnUpdate();
		void StopAll();
	private:
		void UpdateSound(const SoundSettings& settings, SoundPlayback& playback);
	private:
		Scene* mScene;
	};
	
}