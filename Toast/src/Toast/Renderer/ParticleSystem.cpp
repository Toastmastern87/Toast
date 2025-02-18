#include "tpch.h"
#include "ParticleSystem.h"

#include "Toast/Renderer/Renderer.h"

namespace Toast{

	bool ParticleSystem::Initialize()
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		bufferDesc.ByteWidth = sizeof(Particle) * 1000;
		bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bufferDesc.StructureByteStride = sizeof(Particle);

		device->CreateBuffer(&bufferDesc, nullptr, &mBuffer);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN; // Structured buffers don’t have a format
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.NumElements = 1000;

		device->CreateShaderResourceView(mBuffer.Get(), &srvDesc, &mSRV);

		uint16_t indices[] = { 0, 1, 2, 2, 1, 3 };

		D3D11_BUFFER_DESC indexBufferDesc = {};
		indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		indexBufferDesc.ByteWidth = sizeof(indices);
		indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

		D3D11_SUBRESOURCE_DATA indexData = { indices, 0, 0 };
		device->CreateBuffer(&indexBufferDesc, &indexData, &mIndexBuffer);

		return true;
	}

	void ParticleSystem::OnUpdate(float dt, ParticlesComponent& particles, DirectX::XMFLOAT3 spawnPos, float spawnSize)
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		particles.ElapsedTime += dt;

		while (particles.ElapsedTime >= particles.SpawnDelay)
		{
			particles.ElapsedTime -= particles.SpawnDelay;

			// Create a new particle and add it to the list
			if (particles.Particles.size() < mMaxNrOfParticles && particles.Emitting) {

				DirectX::XMFLOAT3 velocity = { 0.0f, 0.0f, 0.0f };
				if (particles.SpawnFunction == EmitFunction::CONE)
				{
					Vector3 velocityVec = RandomVelocityInCone(particles.Velocity, particles.ConeAngleDegrees);
					velocity = { (float)velocityVec.x, (float)velocityVec.y, (float)velocityVec.z };
				}
				else
					velocity = { (float)particles.Velocity.x, (float)particles.Velocity.y, (float)particles.Velocity.z };

				Particle newParticle;
				newParticle.position = spawnPos;
				newParticle.velocity = velocity;
				newParticle.age = 0.0f;
				newParticle.lifetime = particles.MaxLifeTime;
				newParticle.size = spawnSize;
				particles.Particles.push_back(newParticle);
			}
		}

		auto it = particles.Particles.begin();
		while (it != particles.Particles.end()) {
			it->age += dt;

			if (it->age >= it->lifetime) {
				it = particles.Particles.erase(it);  // Remove dead particle
			}
			else {
				++it;
			}
		}

		if (!particles.Particles.empty()) {
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			deviceContext->Map(mBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);

			Particle* instances = (Particle*)mappedResource.pData;

			for (size_t i = 0; i < particles.Particles.size(); ++i) {
				instances[i] = {
					particles.Particles[i].position,
					particles.Particles[i].velocity,
					particles.Particles[i].age,
					particles.Particles[i].lifetime,
					particles.Particles[i].size
				};
			}

			deviceContext->Unmap(mBuffer.Get(), 0);
		}
	}

	void ParticleSystem::InvalidateBuffer(size_t newSize)
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		bufferDesc.ByteWidth = sizeof(Particle) * newSize;
		bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bufferDesc.StructureByteStride = sizeof(Particle);

		device->CreateBuffer(&bufferDesc, nullptr, &mBuffer);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN; // Structured buffers don’t have a format
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.NumElements = newSize;

		device->CreateShaderResourceView(mBuffer.Get(), &srvDesc, &mSRV);

		mMaxNrOfParticles = newSize;
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

}