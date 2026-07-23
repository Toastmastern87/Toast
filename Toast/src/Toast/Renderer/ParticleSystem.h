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
		bool Init();

		// Returns every slot to the free pile and zeroes the counters.
		void Reset();

		// Old CPU Version, will be removed once the full transition to the new particle system is completed
		void OnUpdate(float dt, ParticlesComponent& particles, DirectX::XMFLOAT3 spawnPos, DirectX::XMFLOAT3 spawnSize, DirectX::XMMATRIX roationQuat, size_t maxNrOfParticles, DirectX::XMFLOAT3 velocity);

		Vector3 RandomVelocityInCone(const Vector3& baseDir, double coneAngleDegrees);
		DirectX::XMFLOAT3 RandomPointInBox(const DirectX::XMFLOAT3& boxCenter, const DirectX::XMFLOAT3& boxSize, float biasExponent);
		float BiasedRandomValue(float scale, float biasExponent);

		Ref<StructuredBuffer> GetParticleBuffer() { return mParticleBuffer; }

		// TEMP CODE!
		void DebugValidatePool();
	private:
		// The pool of particles. 262,144 slots of GPUParticle data (~20 MB).
		Ref<StructuredBuffer> mParticleBuffer;

		// The two ALIVE lists (ping-pong). 1 MB each.
		// These hold uint SLOT INDICES, not particle data. A value of [7, 42, 3]
		// means "slots 7, 42 and 3 currently hold live particles."
		// The list that currently ISN'T being used to display the particles is used to decide what next
		// frame will include.
		Ref<StructuredBuffer> mAliveListA;
		Ref<StructuredBuffer> mAliveListB;

		// The DEAD list. 1 MB. A stack of uint SLOT INDICES that are free.
		// "Dead" means available-to-be-born-into. Starts as 0,1,2,...,N-1.
		Ref<StructuredBuffer> mDeadList;

		// The COUNTERS. Exactly 4 uints - 16 bytes TOTAL, not per particle.
		// This is the "whiteboard" the GPU keeps for itself:
		//
		//   [0] AliveCount          alive this frame -> sizes the simulate dispatch
		//   [1] DeadCount           free slots left  -> emit checks this
		//   [2] EmitCount           reserved for GPU-side spawn requests 
		//   [3] AliveCountAfterSim  survivors -> becomes next frame's AliveCount
		// 
		// INVARIANT: AliveCount + DeadCount == MAX_PARTICLES, always. If that
		// ever drifts, an atomic is wrong. It is the single best health check
		// in this entire system.
		// ---------------------------------------------------------------------
		Ref<StructuredBuffer> mCounters;

		// The INDIRECT ARGS buffer. 64 bytes. Cannot be a StructuredBuffer -
		// it needs DRAWINDIRECT_ARGS + ALLOW_RAW_VIEWS misc flags, and it isn't
		// an array of structs, it's a parameter blob.
		Microsoft::WRL::ComPtr<ID3D11Buffer>              mIndirectArgs;
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> mIndirectArgsUAV;

		// Which alive list is "current"
		uint32_t mAlivePingPong = 0;

		// Advanced once per frame so that the GPU RNG differs between frames
		uint32_t mFrameSeed = 0;

		// Guards against a second initialize.
		bool mInitialized = false;
	};

}