#pragma once

#include <DirectXMath.h>
#include <cstdint>

namespace Toast {

	// The fixed particle pool size, this value can be increased if more particles are needed
	static constexpr uint32_t MAX_PARTICLES = 1u << 18; // 262,144

	// Threads per compute thread group
	// MUST match PARTICLE_THREADGROUP_SIZE in ParticleCommon.hlsli
	static constexpr uint32_t PARTICLE_THREADGROUP_SIZE = 64;

	// Spawn shape
	// Values most match the EMITFUNCTION_* defines in ParticleCommon.hlsli
	enum class EmitFunction
	{
		NONE = 0,
		CONE = 1,
		BOX = 2
	};

	// One particle's GPU-side data.
	struct GPUParticle 
	{
		DirectX::XMFLOAT3	Position;			// 0 
		float				Age;				// 12
		DirectX::XMFLOAT3	Velocity;			// 16
		float				LifeTime;			// 28
		DirectX::XMFLOAT3	StartColor;			// 32 
		float				ColorBlendFactor;	// 44
		DirectX::XMFLOAT3	EndColor;			// 48 
		float				Size;				// 60 
		float				GrowRate;			// 64
		float				BurstInitial;		// 68
		float				BurstDecay;			// 72 
		uint32_t			EmitterIndex;		// 76
	};

	// The legacy CPU - simulated particle
	// Will be removed when fully transitioned to the new Particle system
	struct Particle 
	{
		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT3 Velocity;
		DirectX::XMFLOAT3 StartColor;
		DirectX::XMFLOAT3 EndColor;
		float Age;
		float LifeTime;
		float ColorBlendFactor;
		float Size;
		float GrowRate;
		float BurstInitial;
		float BurstDecay;
	};
}