#pragma once

#include "Toast/Core/Timestep.h"
#include "Toast/Core/Math/Quaternion.h"

#include <DirectXMath.h>

namespace Toast {

	class Scene;
	class Camera;
	class Entity;
	struct SoundSettings;
	struct SoundPlayback;
	struct AudioComponent;

	class AudioSystem
	{
	public:
		static constexpr float MinPitch = 0.5f;
		static constexpr float MaxPitch = 2.0f;

		explicit AudioSystem(Scene* scene) : mScene(scene) {}

		void OnUpdate(Timestep ts, Camera* listenerCamera, const Quaternion& cameraRotation);
		void StopAll();
	private:
		struct Emitter3D
		{
			DirectX::XMFLOAT3 Position;
			DirectX::XMFLOAT3 Velocity;
			float MinDistance;
			float DopplerScale;
		};

		void UpdateListener(float dt, Camera& camera, const Quaternion& cameraRotation, float speedOfSound);
		Emitter3D BuildEmitter(Entity entity, AudioComponent& audio, Camera& camera, float dt, float speedOfSound);
		void UpdateSound(const SoundSettings& settings, SoundPlayback& playbackm, bool loop, const Emitter3D* emitter);
		static float ApplyEmitter(uint32_t slot, const Emitter3D* emitter);
	private:
		Scene* mScene;

		DirectX::XMFLOAT3 mLastListenerPosition = { 0.0f, 0.0f, 0.0f };
		bool mHasLastListenerPosition = false;
	};
	
}