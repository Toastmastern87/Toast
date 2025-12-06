#pragma once

#include "Toast/Renderer/PlanetSystem.h"

#include "Toast/Scene/Entity.h"

namespace Toast {

	class Scene;

	class PhysicsEngineUpdated
	{
		struct TerrainContactPoint
		{
			Vector3 Position;    // world space
			Vector3 Normal;      // world space
			double Penetration; // > 0 inside terrain
		};

		struct TerrainContactManifold
		{
			Entity Entity;
			std::vector<TerrainContactPoint> Points;
		};

	public:
		PhysicsEngineUpdated();

		void Initialize(Scene* scene);
		void Update(double ts);

		double GetAltitude(Entity& entity);
		double GetAltitudeAtWorldPos(const Vector3& worldPos, double& outRadialDist, Vector3& outGroundNormal);
	private:
		void ApplyLinearImpulse(RigidBodyComponent& rbc, Vector3 impulse);

		void ApplyGravity(Entity& entity, double ts);

		void IntegrateLinear(Entity& entity, double ts);

		bool CheckTerrainCollision(Entity& entity, TerrainContactManifold& manifold);
		bool FindTerrainContactPoints(Entity& entity, TerrainContactManifold& manifold);
		bool FindTerrainContactPointsSphere(Entity& entity, TerrainContactManifold& manifold);
		bool FindTerrainContactPointsBox(Entity& entity, TerrainContactManifold& manifold);
		void ResolveTerrainCollision(const TerrainContactManifold& manifold, double dt);
	private:
		Scene* mScene;

		Ref<Mesh> mGuideMesh;
	};
}