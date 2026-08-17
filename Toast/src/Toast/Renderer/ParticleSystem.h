#pragma once

#include "Toast/Renderer/ParticleCommon.h"

#include "Toast/Scene/Components.h"

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

		// Load the shaders needed by the Particle System
		void LoadShaders();

		// Returns every slot to the free pile and zeros the counters.
		void Reset();

		void OnUpdate(float dt,	const std::vector<EmitterParamsGPU>& emitters, const std::vector<uint32_t>& emitCounts);

		Ref<StructuredBuffer> GetParticleBuffer() { return mParticleBuffer; }
		Ref<StructuredBuffer> GetDeadList() { return mDeadList; }
		Ref<StructuredBuffer> GetCounters() { return mCounters; }

		// The list being READ this frame (emit appends to it, simulate reads it).
		Ref<StructuredBuffer> GetCurrentAliveList() { return mAlivePingPong == 0 ? mAliveListA : mAliveListB; }
		// The list being WRITTEN this frame (simulate appends survivors to it).
		Ref<StructuredBuffer> GetNextAliveList() { return mAlivePingPong == 0 ? mAliveListB : mAliveListA; }

		ID3D11Buffer* GetIndirectArgs() { return mIndirectArgs.Get(); }
		ID3D11UnorderedAccessView* GetIndirectArgsUAV() { return mIndirectArgsUAV.Get(); }

		uint32_t GetFrameSeed() const { return mFrameSeed; }
		void AdvanceFrameSeed() { ++mFrameSeed; }

		void SetPlanetData(const DirectX::XMFLOAT3& planetCenter, float gravityStrength);

		// Advances the per-emitter spawn accumulator and returns how many
		// particles to spawn this frame. Mutates pc.ElapsedTime.
		static uint32_t ComputeEmitCount(ParticlesComponent& pc, float dt);

		// TEMP CODE!
		void DebugLogCounters(uint32_t everyNFrames);
	private:
		// Uploads all emitter params for this frame. Call once, before Emit().
		void UpdateEmitterParams(const std::vector<EmitterParamsGPU>& emitters);

		// Dispatches the emit compute shader for one emitter.
		void Emit(uint32_t emitterIndex, uint32_t emitCount, float dt);

		void Simulate(float dt);
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
		// This is the "white board" the GPU keeps for itself:
		//
		//   [0] AliveCount          alive this frame -> sizes the simulate dispatch
		//   [1] DeadCount           free slots left  -> emit checks this
		//   [2] EmitCount           reserved for GPU-side spawn requests 
		//   [3] AliveCountAfterSim  survivors -> becomes next frame's AliveCount
		// 
		// INVARIANT: AliveCount + DeadCount == MAX_PARTICLES, always. If that
		// ever drifts, an atomic is wrong. It is the single best health check
		// in this entire system.
		Ref<StructuredBuffer> mCounters;

		// The INDIRECT ARGS buffer. 64 bytes. Cannot be a StructuredBuffer -
		// it needs DRAWINDIRECT_ARGS + ALLOW_RAW_VIEWS misc flags, and it isn't
		// an array of structs, it's a parameter blob.
		Microsoft::WRL::ComPtr<ID3D11Buffer>              mIndirectArgs;
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> mIndirectArgsUAV;

		// Per-emitter parameters, rewritten by the CPU every frame.
		Ref<StructuredBuffer> mEmitterParams;

		Ref<ConstantBuffer> mEmitCBuffer;
		Buffer              mEmitBuffer;

		// Per-frame simulate constants (just dt, padded to 16 bytes).
		Ref<ConstantBuffer> mSimCBuffer;
		Buffer              mSimBuffer;

		// Which alive list is "current"
		uint32_t mAlivePingPong = 0;

		// Advanced once per frame so that the GPU RNG differs between frames
		uint32_t mFrameSeed = 0;

		DirectX::XMFLOAT3 mPlanetCenter;
		float mGravityStrength;

		AssetHandle mEmitShaderHandle;
		AssetHandle mSimKickoffShaderHandle;
		AssetHandle mSimulateShaderHandle;
		AssetHandle mFinalizeShaderHandle;

		// Guards against a second initialize.
		bool mInitialized = false;
	};

}