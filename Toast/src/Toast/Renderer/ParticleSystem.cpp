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

	void ParticleSystem::OnUpdate(float dt, ParticlesComponent& particles, DirectX::XMFLOAT3 spawnPos, DirectX::XMFLOAT3 spawnSize, DirectX::XMMATRIX roationQuat, size_t maxNrOfParticles, DirectX::XMFLOAT3 velocity)
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		particles.ElapsedTime += dt;

		while (particles.ElapsedTime >= particles.SpawnDelay)
		{
			particles.ElapsedTime -= particles.SpawnDelay;

			// Create a new particle and add it to the list
			if (particles.Particles.size() < maxNrOfParticles && particles.Emitting) {

				DirectX::XMFLOAT3 finalVelocity = { 0.0f, 0.0f, 0.0f };

				DirectX::XMVECTOR originalVel = DirectX::XMLoadFloat3(&velocity);
				DirectX::XMVECTOR rotatedVel = DirectX::XMVector3Transform(originalVel, roationQuat);

				DirectX::XMFLOAT3 tempVelocity;
				DirectX::XMStoreFloat3(&tempVelocity, rotatedVel);

				DirectX::XMFLOAT3 finalSpawnPos;

				switch (particles.SpawnFunction)
				{
					case EmitFunction::CONE:
					{
						Vector3 velocityVec = RandomVelocityInCone(tempVelocity, particles.ConeAngleDegrees);
						finalVelocity = { (float)velocityVec.x, (float)velocityVec.y, (float)velocityVec.z };

						finalSpawnPos = spawnPos;

						break;
					}
					case EmitFunction::BOX:
					{
						finalSpawnPos = RandomPointInBox(spawnPos, spawnSize, particles.BiasExponent);

						finalVelocity = tempVelocity;

						break;
					}
					default:
					{
						finalVelocity = tempVelocity;

						finalSpawnPos = spawnPos;

						break;
					}
				}
					
				Particle newParticle;
				newParticle.Position = finalSpawnPos;
				newParticle.Velocity = finalVelocity;
				newParticle.StartColor = particles.StartColor;
				newParticle.EndColor = particles.EndColor;
				newParticle.ColorBlendFactor = particles.ColorBlendFactor;
				newParticle.Age = 0.0f;
				newParticle.LifeTime = particles.MaxLifeTime;
				newParticle.Size = particles.Size;
				newParticle.GrowRate = particles.GrowRate;
				newParticle.BurstInitial = particles.BurstInitial;
				newParticle.BurstDecay = particles.BurstDecay;
				particles.Particles.push_back(newParticle);
			}
		}

		auto it = particles.Particles.begin();
		while (it != particles.Particles.end()) {
			it->Age += dt;

			if (it->Age >= it->LifeTime) {
				it = particles.Particles.erase(it);  // Remove dead particle
			}
			else {
				++it;
			}
		}
	}

	Vector3 ParticleSystem::RandomVelocityInCone(const Vector3& baseDir, double coneAngleDegrees)
	{
		double coneAngleRadians = coneAngleDegrees * M_PI / 180.0;

		// Generate two random values in [0, 1]
		double u = static_cast<double>(rand()) / RAND_MAX;
		double v = static_cast<double>(rand()) / RAND_MAX;

		// Compute the offset angles.
		double theta = u * coneAngleRadians; // deviation angle from center
		double phi = v * 2.0 * M_PI;           // azimuthal angle

		// Spherical coordinates to Cartesian (assuming cone aligned with +Z)
		double sinTheta = std::sin(theta);
		double cosTheta = std::cos(theta);
		double x = sinTheta * std::cos(phi);
		double y = sinTheta * std::sin(phi);
		double z = cosTheta;  // along the cone's central axis

		// Normalize the base direction
		Vector3 base = Vector3::Normalize(baseDir);

		// Choose an arbitrary "up" vector; if base is nearly (0,1,0) choose (1,0,0)
		Vector3 up(0.0, 1.0, 0.0);
		if (std::abs(Vector3::Dot(base, up)) > 0.99)
			up = Vector3(1.0, 0.0, 0.0);

		// Compute a right vector and a new up vector for the orthonormal basis.
		Vector3 right = Vector3::Normalize(Vector3::Cross(up, base));
		Vector3 newUp = Vector3::Cross(base, right);

		// Transform the local vector (x, y, z) to world space:
		// worldVec = right * x + newUp * y + base * z.
		Vector3 worldVec = right * x + newUp * y + base * z;

		// Return the normalized world vector.
		return Vector3::Normalize(worldVec);
	}

	DirectX::XMFLOAT3 ParticleSystem::RandomPointInBox(const DirectX::XMFLOAT3& boxCenter, const DirectX::XMFLOAT3& boxSize, float biasExponent)
	{
		float rx = BiasedRandomValue(boxSize.x, biasExponent);
		float ry = BiasedRandomValue(boxSize.y, biasExponent);
		float rz = BiasedRandomValue(boxSize.z, biasExponent);

		DirectX::XMFLOAT3 randomPos;
		randomPos.x = boxCenter.x + rx;
		randomPos.y = boxCenter.y + ry;
		randomPos.z = boxCenter.z + rz;

		return randomPos;
	}

	float ParticleSystem::BiasedRandomValue(float halfExtent, float biasExponent)
	{
		// Get a random value in [0, 1]
		float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
		// Map to [-1, 1]
		float value = r * 2.0f - 1.0f;
		// Apply bias: if biasExponent > 1, the distribution is peaked near zero.
		float biasedValue = (value < 0.0f ? -1.0f : 1.0f) * pow(fabs(value), biasExponent);
		return biasedValue * halfExtent;
	}

	void ParticleSystem::UpdateEmitterParams(const std::vector<EmitterParamsGPU>& emitters)
	{
		if (emitters.empty())
			return;

		TOAST_CORE_ASSERT(emitters.size() <= MAX_EMITTERS, "More particle emitters than MAX_EMITTERS!");

		// Update the structured buffer with the emitters data
		mEmitterParams->Update(emitters.data(), emitters.size() * sizeof(EmitterParamsGPU));
	}

	void ParticleSystem::Emit(uint32_t emitterIndex, uint32_t emitCount)
	{
		if (emitCount == 0)
			return;

		//RendererAPI* API = RenderCommand::sRendererAPI.get();
		//ID3D11DeviceContext* ctx = API->GetDeviceContext();

		// --- per-dispatch constants ---
		// Map it into the emit constant buffer, then Bind.
		mEmitBuffer.Write((uint8_t*)&emitCount, 4, 0);
		mEmitBuffer.Write((uint8_t*)&emitterIndex, 4, 4);
		mEmitBuffer.Write((uint8_t*)&mFrameSeed, 4, 8);
		// bytes 12..15 stay zero - padding to the 16-byte constant buffer minimum.
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
	void ParticleSystem::DebugLogCounters(uint32_t everyNFrames, int32_t OLDnrOfParticles)
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

		TOAST_CORE_INFO("OLD PARTICLE SYSTEM: %d, Particles: Alive=%d Dead=%d (sum=%d, should be %d)", OLDnrOfParticles, c[0], c[1], c[0] + c[1], MAX_PARTICLES);
	}

}