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
	// delta time can be enormous, and the spawn accumulator would ask for hundreds of
	// thousands of particles in a single dispatch - instantly draining the pool.
	static constexpr uint32_t MAX_EMIT_PER_EMITTER_PER_FRAME = 4096;

	// Must match the values in ParticleCommon.hlsli, used for indirect drawing
	static constexpr uint32_t ARGS_OFFSET_DISPATCH = 0;
	static constexpr uint32_t ARGS_OFFSET_DRAW = 16;

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
		float				StartIntensity;		// 80 
		float				EndIntensity;		// 84
		float				IntensityFalloff;	// 88
		float				SoftFadeDistance;	// 92
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
		float				LifeTimeJitter;
		float				SizeJitter;
		float				SpeedJitter;
		DirectX::XMFLOAT3	PrevSpawnPosition;
		float				SoftFadeDistance;
		float				StartIntensity;		
		float				EndIntensity;		
		float				IntensityFalloff;	
		float				_pad1;
	};

}