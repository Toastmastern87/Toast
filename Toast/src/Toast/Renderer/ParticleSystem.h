#pragma once

#include "Toast/Scene/Components.h"

#include "Toast/Core/Math/Vector.h"

namespace Toast {

	struct Particle {
		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT3 Velocity;
		DirectX::XMFLOAT3 StartColor;
		DirectX::XMFLOAT3 EndColor;
		float Age;
		float Lifetime;
		float Size;
		float GrowRate;
	};

	enum class EmitFunction 
	{
		NONE = 0,
		CONE = 1
	};

	class ParticleSystem
	{
	public:
		ParticleSystem() = default;

		bool Initialize();
		void OnUpdate(float dt, ParticlesComponent& particles, DirectX::XMFLOAT3 spawnPos, float spawnSize);

		void InvalidateBuffer(size_t newSize);

		Vector3 RandomVelocityInCone(const Vector3& baseDir, double coneAngleDegrees);

		Microsoft::WRL::ComPtr<ID3D11Buffer> GetIndexBuffer() { return mIndexBuffer; }
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetSRV() { return mSRV; }
		size_t GetCurrentMaxNrOfParticles() { return mMaxNrOfParticles; }
	private:
		size_t mMaxNrOfParticles = 1000;

		Microsoft::WRL::ComPtr<ID3D11Buffer> mBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> mIndexBuffer;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mSRV;
	};

}