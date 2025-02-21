#include "tpch.h"
#include "ParticleSystem.h"

#include "Toast/Renderer/Renderer.h"

namespace Toast{

	bool ParticleSystem::Initialize()
	{
		return true;
	}

	void ParticleSystem::OnUpdate(float dt, ParticlesComponent& particles, DirectX::XMFLOAT3 spawnPos, DirectX::XMFLOAT3 spawnSize, DirectX::XMMATRIX roationQuat, size_t maxNrOfParticles)
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

		particles.ElapsedTime += dt;

		while (particles.ElapsedTime >= particles.SpawnDelay)
		{
			particles.ElapsedTime -= particles.SpawnDelay;

			// Create a new particle and add it to the list
			if (particles.Particles.size() < maxNrOfParticles && particles.Emitting) {

				DirectX::XMFLOAT3 velocity = { 0.0f, 0.0f, 0.0f };

				DirectX::XMVECTOR originalVel = DirectX::XMLoadFloat3(&particles.Velocity);
				DirectX::XMVECTOR rotatedVel = DirectX::XMVector3Transform(originalVel, roationQuat);

				DirectX::XMFLOAT3 tempVelocity;
				DirectX::XMStoreFloat3(&tempVelocity, rotatedVel);

				DirectX::XMFLOAT3 finalSpawnPos;

				switch (particles.SpawnFunction)
				{
					case EmitFunction::CONE:
					{
						Vector3 velocityVec = RandomVelocityInCone(tempVelocity, particles.ConeAngleDegrees);
						velocity = { (float)velocityVec.x, (float)velocityVec.y, (float)velocityVec.z };

						finalSpawnPos = spawnPos;

						break;
					}
					case EmitFunction::BOX:
					{
						finalSpawnPos = RandomPointInBox(spawnPos, spawnSize, particles.BiasExponent);

						velocity = tempVelocity;

						break;
					}
					default:
					{
						velocity = tempVelocity;

						finalSpawnPos = spawnPos;

						break;
					}
				}
					
				Particle newParticle;
				newParticle.Position = finalSpawnPos;
				newParticle.Velocity = velocity;
				newParticle.StartColor = particles.StartColor;
				newParticle.EndColor = particles.EndColor;
				newParticle.ColorBlendFactor = particles.ColorBlendFactor;
				newParticle.Age = 0.0f;
				newParticle.Lifetime = particles.MaxLifeTime;
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

			if (it->Age >= it->Lifetime) {
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

}