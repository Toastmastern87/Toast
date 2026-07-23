#include "tpch.h"
#include "ParticleSystem.h"

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

		// Making sure everything is reseted and ready to be used
		Reset();

		mInitialized = true;

		TOAST_CORE_INFO("GPU particle pool initialized: %d slots, %d MB total VRAM.",	N, (N * (sizeof(GPUParticle) + 3 * sizeof(uint32_t))) / (1024 * 1024));

		return true;
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

	// TEMP CODE!
	void ParticleSystem::DebugValidatePool()
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();
		ID3D11DeviceContext* ctx = API->GetDeviceContext();

		auto ReadBack = [&](ID3D11Buffer* src, uint32_t elementCount, std::vector<uint32_t>& out)
			{
				D3D11_BUFFER_DESC bd = {};
				src->GetDesc(&bd);

				// Staging copy: same layout, but CPU-readable and not bindable.
				D3D11_BUFFER_DESC sd = bd;
				sd.Usage = D3D11_USAGE_STAGING;
				sd.BindFlags = 0;
				sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

				Microsoft::WRL::ComPtr<ID3D11Buffer> staging;
				if (FAILED(device->CreateBuffer(&sd, nullptr, &staging)))
					return false;

				ctx->CopyResource(staging.Get(), src);

				D3D11_MAPPED_SUBRESOURCE m = {};
				if (FAILED(ctx->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &m)))
					return false;

				out.resize(elementCount);
				memcpy(out.data(), m.pData, elementCount * sizeof(uint32_t));
				ctx->Unmap(staging.Get(), 0);
				return true;
			};

		std::vector<uint32_t> counters;
		if (ReadBack(mCounters->GetBuffer(), 4, counters))
		{
			TOAST_CORE_INFO("Counters: Alive=%d Dead=%d Emit=%d AliveAfter=%d",
				counters[0], counters[1], counters[2], counters[3]);
			TOAST_CORE_ASSERT(counters[0] == 0, "AliveCount should be 0");
			TOAST_CORE_ASSERT(counters[1] == MAX_PARTICLES, "DeadCount should be MAX_PARTICLES");
			TOAST_CORE_ASSERT(counters[0] + counters[1] == MAX_PARTICLES,
				"INVARIANT BROKEN: Alive + Dead must equal MAX_PARTICLES");
		}

		std::vector<uint32_t> dead;
		if (ReadBack(mDeadList->GetBuffer(), 8, dead))   // just the first 8
		{
			TOAST_CORE_INFO("DeadList[0..7]: %d %d %d %d %d %d %d %d",
				dead[0], dead[1], dead[2], dead[3],
				dead[4], dead[5], dead[6], dead[7]);
			for (uint32_t i = 0; i < 8; ++i)
				TOAST_CORE_ASSERT(dead[i] == i, "DeadList must start as 0,1,2,...");
		}
	}

}