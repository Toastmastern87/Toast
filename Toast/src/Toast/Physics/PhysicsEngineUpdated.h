#pragma once

#include "Toast/Renderer/PlanetSystem.h"

#include "Toast/Scene/Entity.h"

namespace Toast {

	class Scene;

	class PhysicsEngineUpdated
	{
	public:
		PhysicsEngineUpdated();

		void Initialize(Scene* scene);
		void Update(double ts);

		TerrainData LoadTerrainData(const std::string& path, const double maxHeight, const double minHeight);

		double GetAltitude(Entity& entity);
	private:
		void ApplyLinearImpulse(RigidBodyComponent& rbc, Vector3 impulse);

		void ApplyGravity(double ts);

		void WorldPosToHeightMapUV(Planet& p, const Vector3& worldPos, const Vector3& worldTranslation, int mapWidth, int mapHeight, float& outU, float& outV, double& outRadialDist);
		double SampleHeightBilinear(const std::vector<double>& heightData, int textureWidth, int textureHeight, float u, float v);
	private:
		Scene* mScene;
	};
}