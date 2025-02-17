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

	void ParticleSystem::OnUpdate(float dt, ParticlesComponent& particles, DirectX::XMFLOAT3 spawnPos)
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		particles.ElapsedTime += dt;

		if (particles.ElapsedTime >= particles.SpawnDelay)
		{
			particles.ElapsedTime -= particles.SpawnDelay;

			// Create a new particle and add it to the list
			if (particles.Particles.size() < mMaxNrOfParticles && particles.Emitting) {
				Particle newParticle;
				newParticle.position = spawnPos;
				newParticle.velocity = { (float)particles.StartVelocity.x, (float)particles.StartVelocity.y, (float)particles.StartVelocity.z };
				newParticle.age = 0.0f;
				newParticle.lifetime = particles.MaxLifeTime;
				newParticle.size = 1.0f;  // Default size
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

}