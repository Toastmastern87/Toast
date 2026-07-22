#pragma once

#include "Toast/Renderer/ParticleCommon.h"

#include "Toast/Scene/Components.h"

#include "Toast/Core/Math/Vector.h"

#include <wrl.h>
#include <d3d11.h>

namespace Toast {

	struct ParticlesComponent;

	class ParticleSystem
	{
	public:
		ParticleSystem() = default;
		~ParticleSystem() = default;

		// Creates the GPU pool. Called ONCE, from Renderer::Init()
		bool Initialize();

		// Returns every slot to the free pile and zeroes the counters.
		void Reset();

		// Old CPU Version, will be removed once the full transition to the new particle system is completed
		void OnUpdate(float dt, ParticlesComponent& particles, DirectX::XMFLOAT3 spawnPos, DirectX::XMFLOAT3 spawnSize, DirectX::XMMATRIX roationQuat, size_t maxNrOfParticles, DirectX::XMFLOAT3 velocity);

		Vector3 RandomVelocityInCone(const Vector3& baseDir, double coneAngleDegrees);
		DirectX::XMFLOAT3 RandomPointInBox(const DirectX::XMFLOAT3& boxCenter, const DirectX::XMFLOAT3& boxSize, float biasExponent);
		float BiasedRandomValue(float scale, float biasExponent);
	};

}