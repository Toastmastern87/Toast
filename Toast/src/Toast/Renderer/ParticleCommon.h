#pragma once

#include <DirectXMath.h>
#include <cstdint>

namespace Toast {

	// The fixed particle pool size, this value can be increased if more particles are needed
	static constexpr uint32_t MAX_PARTICLES = 1u << 18; // 262,144

	// Maximum simultaneous emitters. 64 * 112 bytes = 7 KB - trivially small,
	// so this is generous rather than tuned.
	static constexpr uint32_t MAX_EMITTERS = 64;

	// Threads per compute thread group
	// MUST match PARTICLE_THREADGROUP_SIZE in ParticleCommon.hlsli
	static constexpr uint32_t PARTICLE_THREADGROUP_SIZE = 64;

	// Safety valve. After a frame hitch (level load, shader compile, breakpoint)
	// dt can be enormous, and the spawn accumulator would ask for hundreds of
	// thousands of particles in a single dispatch - instantly draining the pool.
	static constexpr uint32_t MAX_EMIT_PER_EMITTER_PER_FRAME = 4096;

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

	struct EmitterParamsGPU 
	{
		DirectX::XMFLOAT3	SpawnPosition;
		float				_pad0;
		DirectX::XMFLOAT3	SpawnSize;
		float				BiasExponent;
		DirectX::XMFLOAT3	Velocity;
		float				ConeAngleDegrees;
		DirectX::XMFLOAT3	StartColor;
		float				ColorBlendFactor;
		DirectX::XMFLOAT3	EndColor;
		float				MaxLifeTime;
		float				Size;
		float				GrowRate;
		float				BurstInitial;
		float				BurstDecay;
		uint32_t			EmitFunction;
		uint32_t			_pad1;
		uint32_t			_pad2;
		uint32_t			_pad3;
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