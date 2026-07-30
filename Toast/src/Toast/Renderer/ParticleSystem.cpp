#include "tpch.h"
#include "ParticleSystem.h"

#include "Toast/Debug/FrameProfiler.h"

#include "Toast/Renderer/Renderer.h"

namespace Toast{

	bool ParticleSystem::Init()
	{
		if (mInitialized)
		{
			TOAST_CORE_WARN("ParticleSystem::Initialize called twice - ignoring.");
			return true;
		}

		const uint32_t N = MAX_PARTICLES;

		// The Particle Pool
		mParticleBuffer = CreateRef<StructuredBuffer>(
			sizeof(GPUParticle),     // stride: 80 bytes per element
			N,                       // count:  262,144 elements
			D3D11_USAGE_DEFAULT,
			true,                    // createUAV
			false);                  // append: NO

		// Alive lists that are used in a ping-pong way
		mAliveListA = CreateRef<StructuredBuffer>(sizeof(uint32_t), N, D3D11_USAGE_DEFAULT, true, false);
		mAliveListB = CreateRef<StructuredBuffer>(sizeof(uint32_t), N, D3D11_USAGE_DEFAULT, true, false);

		// The dead list holding the free slot indices that can be used
		mDeadList = CreateRef<StructuredBuffer>(sizeof(uint32_t), N, D3D11_USAGE_DEFAULT, true, false);

		// The counters that keeps track of the number in each of the lists above
		mCounters = CreateRef<StructuredBuffer>(sizeof(uint32_t), 4, D3D11_USAGE_DEFAULT, true, false);

		// The indirect args buffer
		{
			RendererAPI* API = RenderCommand::sRendererAPI.get();
			TOAST_CORE_ASSERT(API, "ParticleSystem::Initialize: no RendererAPI! Initialize() must be called AFTER the renderer API exists.");
			if (!API) return false;

			ID3D11Device* device = API->GetDevice();
			TOAST_CORE_ASSERT(device, "ParticleSystem::Initialize: no D3D11 device!");
			if (!device) return false;

			D3D11_BUFFER_DESC bd = {};
			bd.ByteWidth = 64;                        // multiple of 4, required for raw views
			bd.Usage = D3D11_USAGE_DEFAULT;
			bd.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
			bd.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS | D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;

			HRESULT hr = device->CreateBuffer(&bd, nullptr, &mIndirectArgs);
			TOAST_CORE_ASSERT(SUCCEEDED(hr), "ParticleSystem: indirect args buffer failed");
			if (FAILED(hr)) return false;

			D3D11_UNORDERED_ACCESS_VIEW_DESC uavd = {};
			uavd.Format = DXGI_FORMAT_R32_TYPELESS;  // required for raw views
			uavd.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			uavd.Buffer.FirstElement = 0;
			uavd.Buffer.NumElements = 64 / 4;                    // 16 addressable uints
			uavd.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;

			hr = device->CreateUnorderedAccessView(mIndirectArgs.Get(), &uavd, &mIndirectArgsUAV);
			TOAST_CORE_ASSERT(SUCCEEDED(hr), "ParticleSystem: indirect args UAV failed");
			if (FAILED(hr)) return false;
		}

		mEmitterParams = CreateRef<StructuredBuffer>(
			sizeof(EmitterParamsGPU),   // stride: 112 bytes
			MAX_EMITTERS,               // count:  64
			D3D11_USAGE_DYNAMIC,
			false,                      // createUAV: no
			false);                     // append: no

		mEmitCBuffer = ConstantBufferLibrary::Load("ParticleEmit", 16, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_COMPUTE_SHADER, CBufferBindSlot(1)) });
		mEmitBuffer.Allocate(mEmitCBuffer->GetSize());
		mEmitBuffer.ZeroInitialize();

		mSimCBuffer = ConstantBufferLibrary::Load("ParticleSim", 16,
			std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_COMPUTE_SHADER, CBufferBindSlot(2)) });
		mSimBuffer.Allocate(mSimCBuffer->GetSize());
		mSimBuffer.ZeroInitialize();

		// Making sure everything is reseted and ready to be used
		Reset();

		mInitialized = true;

		TOAST_CORE_INFO("GPU particle pool initialized: %d slots, %d MB total VRAM.",	N, (N * (sizeof(GPUParticle) + 3 * sizeof(uint32_t))) / (1024 * 1024));

		return true;
	}

	void ParticleSystem::LoadShaders()
	{
		mEmitShaderHandle = AssetManager::GetEngineShaderHandle("ParticleEmit");
		TOAST_CORE_ASSERT(mEmitShaderHandle, "ParticleEmit shader not found in the asset registry!");

		mSimKickoffShaderHandle = AssetManager::GetEngineShaderHandle("ParticleSimKickoff");
		TOAST_CORE_ASSERT(mSimKickoffShaderHandle, "ParticleSimKickoff shader not found in the asset registry!");

		mSimulateShaderHandle = AssetManager::GetEngineShaderHandle("ParticleSimulate");
		TOAST_CORE_ASSERT(mSimulateShaderHandle, "ParticleSimulate shader not found in the asset registry!");

		mFinalizeShaderHandle = AssetManager::GetEngineShaderHandle("ParticleFinalize");
		TOAST_CORE_ASSERT(mFinalizeShaderHandle, "ParticleFinalize shader not found in the asset registry!");
	}

	void ParticleSystem::Reset()
	{
		TOAST_CORE_ASSERT(mDeadList && mCounters, "ParticleSystem::Reset before Initialize!");

		if (!mDeadList || !mCounters)
			return;

		const uint32_t N = MAX_PARTICLES;

		// Reset the dead list making every slot free again.
		{
			std::vector<uint32_t> deadInit(N);
			for (uint32_t i = 0; i < N; ++i)
				deadInit[i] = i;

			mDeadList->Update(deadInit.data(), deadInit.size() * sizeof(uint32_t));
		}

		// Reset the counters, everything is free
		{
			const uint32_t counterInit[4] = { 0u, N, 0u, 0u };
			mCounters->Update(counterInit, sizeof(counterInit));   // full 16 bytes
		}

		mAlivePingPong = 0;

		TOAST_CORE_INFO("GPU particle pool reset - %d slots free.", N);
	}

	void ParticleSystem::OnUpdate(float dt, const std::vector<EmitterParamsGPU>& emitters, const std::vector<uint32_t>& emitCounts)
	{
		AdvanceFrameSeed();

		// Simulate must run every frame, even with no emitters - existing
		// particles have to keep aging and dying, or they freeze in place the
		// instant the last emitter switches off.
		if (!emitters.empty())
		{
			UpdateEmitterParams(emitters);
			for (uint32_t i = 0; i < (uint32_t)emitCounts.size(); ++i)
				Emit(i, emitCounts[i], dt);
		}

		Simulate(dt);
	}

	void ParticleSystem::UpdateEmitterParams(const std::vector<EmitterParamsGPU>& emitters)
	{
		if (emitters.empty())
			return;

		TOAST_CORE_ASSERT(emitters.size() <= MAX_EMITTERS, "More particle emitters than MAX_EMITTERS!");

		// Update the structured buffer with the emitters data
		mEmitterParams->Update(emitters.data(), emitters.size() * sizeof(EmitterParamsGPU));
	}

	void ParticleSystem::Emit(uint32_t emitterIndex, uint32_t emitCount, float dt)
	{
		if (emitCount == 0)
			return;

		// --- per-dispatch constants ---
		// Map it into the emit constant buffer, then Bind.
		mEmitBuffer.Write((uint8_t*)&emitCount, 4, 0);
		mEmitBuffer.Write((uint8_t*)&emitterIndex, 4, 4);
		mEmitBuffer.Write((uint8_t*)&mFrameSeed, 4, 8);
		mEmitBuffer.Write((uint8_t*)&dt, 4, 12);
		mEmitCBuffer->Map(mEmitBuffer);
		mEmitCBuffer->Bind();

		// --- bind the pool (order must match the register(uN) declarations) ---
		mParticleBuffer->BindUAV(0);
		mDeadList->BindUAV(1);
		GetCurrentAliveList()->BindUAV(2);
		mCounters->BindUAV(3);

		RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 0, mEmitterParams->GetSRV());
		auto shader = AssetManager::GetAsset<Shader>(mEmitShaderHandle);
		if (shader)
			shader->Bind();

		// Round UP so a partial group still runs; the shader's early-out
        // handles the spare threads.
        const uint32_t groups = (emitCount + PARTICLE_THREADGROUP_SIZE - 1) / PARTICLE_THREADGROUP_SIZE;
		RenderCommand::DispatchCompute(groups, 1, 1);

		// Unbind everything that we used in the emit compute shader
		mParticleBuffer->UnbindUAV(0);
		mDeadList->UnbindUAV(1);
		GetCurrentAliveList()->UnbindUAV(2);
		mCounters->UnbindUAV(3);

		// Unbind resources
		RenderCommand::ClearShaderResources();
	}

	void ParticleSystem::Simulate(float dt)
	{
		// No TOAST_PROFILE here: Scene::OnUpdate runs outside the renderer's
		// profiled frame, so opening a scope here produces query warnings.
		// Step 5 moves this into the renderer and it gets timing for free.

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* ctx = API->GetDeviceContext();

		// per-frame simulate constants 
		mSimBuffer.Write((uint8_t*)&dt, 4, 0);
		// bytes 4..15 stay zero (padding to the 16-byte minimum)
		mSimCBuffer->Map(mSimBuffer);
		mSimCBuffer->Bind();

		// DISPATCH 1: KICKOFF (1 thread)
		// Sizes the simulate dispatch, and resets the survivor tally.
		{
			mCounters->BindUAV(0);

			// The indirect args buffer is a raw ComPtr, not a StructuredBuffer,
			// so it has no BindUAV helper. Raw call.
			ID3D11UnorderedAccessView* argsUAV = mIndirectArgsUAV.Get();
			ctx->CSSetUnorderedAccessViews(1, 1, &argsUAV, nullptr);

			auto shader = AssetManager::GetAsset<Shader>(mSimKickoffShaderHandle);
			TOAST_CORE_ASSERT(shader, "ParticleSimKickoff shader missing!");
			if (shader)
				shader->Bind();

			RenderCommand::DispatchCompute(1, 1, 1);

			// Unbind: the counters UAV is about to be rebound at a DIFFERENT
			// slot for the simulate dispatch, and D3D11 will not let the same
			// resource sit at two UAV slots.
			mCounters->UnbindUAV(0);
			ID3D11UnorderedAccessView* nullUAV = nullptr;
			ctx->CSSetUnorderedAccessViews(1, 1, &nullUAV, nullptr);
		}

		// DISPATCH 2: SIMULATE (indirect)
		// Integrate, age, kill, compact survivors into the OTHER alive
		{
			mParticleBuffer->BindUAV(0);          // u0 pool
			GetCurrentAliveList()->BindUAV(1);    // u1 alive IN  (read)
			GetNextAliveList()->BindUAV(2);       // u2 alive OUT (survivors)
			mDeadList->BindUAV(3);                // u3 dead
			mCounters->BindUAV(4);                // u4 counters

			auto shader = AssetManager::GetAsset<Shader>(mSimulateShaderHandle);
			TOAST_CORE_ASSERT(shader, "ParticleSimulate shader missing!");
			if (shader)
				shader->Bind();

			RenderCommand::DispatchComputeIndirect(mIndirectArgs.Get(), ARGS_OFFSET_DISPATCH);

			mParticleBuffer->UnbindUAV(0);
			GetCurrentAliveList()->UnbindUAV(1);
			GetNextAliveList()->UnbindUAV(2);
			mDeadList->UnbindUAV(3);
			mCounters->UnbindUAV(4);
		}

		// DISPATCH 3: FINALIZE (1 thread)
		// Survivors become next frame's alive count.
		{
			mCounters->BindUAV(0);

			ID3D11UnorderedAccessView* argsUAV = mIndirectArgsUAV.Get();
			ctx->CSSetUnorderedAccessViews(1, 1, &argsUAV, nullptr);

			auto shader = AssetManager::GetAsset<Shader>(mFinalizeShaderHandle);
			TOAST_CORE_ASSERT(shader, "ParticleFinalize shader missing!");
			if (shader)
				shader->Bind();

			RenderCommand::DispatchCompute(1, 1, 1);

			mCounters->UnbindUAV(0);

			// Must unbind the argument buffer to be ready to be used by DrawIndexedInstancedIndirect.
			ID3D11UnorderedAccessView* nullUAV = nullptr;
			ctx->CSSetUnorderedAccessViews(1, 1, &nullUAV, nullptr);
		}

		// CPU: flip the ping-pong. The list we just WROTE survivors into
		// becomes the list we READ next frame - and the list Emit appends to.
		mAlivePingPong ^= 1u;
	}

	uint32_t ParticleSystem::ComputeEmitCount(ParticlesComponent& pc, float dt)
	{
		if (!pc.Emitting || pc.SpawnDelay <= 0.0)
			return 0;

		pc.ElapsedTime += dt;

		// How many whole spawn intervals fit in the accumulated time.
		uint32_t count = static_cast<uint32_t>(pc.ElapsedTime / pc.SpawnDelay);

		// Keep the remainder. THIS is what makes the spawn rate independent of
		// framerate - dropping it would make emission slower at low FPS.
		pc.ElapsedTime -= count * pc.SpawnDelay;

		if (count > MAX_EMIT_PER_EMITTER_PER_FRAME)
			count = MAX_EMIT_PER_EMITTER_PER_FRAME;

		return count;
	}

	// TEMP CODE!
	// DEBUG ONLY - stalls the GPU. Logs every `everyNFrames` calls.
	void ParticleSystem::DebugLogCounters(uint32_t everyNFrames)
	{
		static uint32_t counter = 0;
		if (++counter % everyNFrames != 0)
			return;

		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();
		ID3D11DeviceContext* ctx = API->GetDeviceContext();

		D3D11_BUFFER_DESC bd = {};
		mCounters->GetBuffer()->GetDesc(&bd);
		bd.Usage = D3D11_USAGE_STAGING;
		bd.BindFlags = 0;
		bd.MiscFlags = 0;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

		Microsoft::WRL::ComPtr<ID3D11Buffer> staging;
		if (FAILED(device->CreateBuffer(&bd, nullptr, &staging)))
			return;

		ctx->CopyResource(staging.Get(), mCounters->GetBuffer());

		D3D11_MAPPED_SUBRESOURCE m = {};
		if (FAILED(ctx->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &m)))
			return;

		uint32_t c[4];
		memcpy(c, m.pData, sizeof(c));
		ctx->Unmap(staging.Get(), 0);

		TOAST_CORE_INFO("Particles: Alive=%d Dead=%d (sum=%d, should be %d)", c[0], c[1], c[0] + c[1], MAX_PARTICLES);
	}

}