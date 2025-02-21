#pragma once

#include "Toast/Scene/Components.h"

#include "Toast/Core/Math/Vector.h"

namespace Toast {

	struct Particle {
		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT3 Velocity;
		DirectX::XMFLOAT3 StartColor;
		DirectX::XMFLOAT3 EndColor;
		float ColorBlendFactor;
		float Age;
		float Lifetime;
		float Size;
		float GrowRate;
		float BurstInitial;
		float BurstDecay;
	};

	enum class EmitFunction
	{
		NONE = 0,
		CONE = 1,
		BOX = 2
	};

	class ParticleSystem
	{
	public:
		ParticleSystem() = default;

		bool Initialize();
		void OnUpdate(float dt, ParticlesComponent& particles, DirectX::XMFLOAT3 spawnPos, DirectX::XMFLOAT3 spawnSize, DirectX::XMMATRIX roationQuat, size_t maxNrOfParticles);

		Vector3 RandomVelocityInCone(const Vector3& baseDir, double coneAngleDegrees);
		DirectX::XMFLOAT3 RandomPointInBox(const DirectX::XMFLOAT3& boxCenter, const DirectX::XMFLOAT3& boxSize, float biasExponent);
		float BiasedRandomValue(float scale, float biasExponent);
	};

}