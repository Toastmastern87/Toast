#pragma once

#include "Toast/Scene/Entity.h"

namespace Toast {

	class Scene;

	class PhysicsEngine
	{
	public:
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

		struct PhysicsSettings
		{
			int SlowDown = 1;
			int FPSTarget = 60;
			int StepsPerUpdate = 1;
			float ElapsedTime = 0.0f;
			float MaxAngularVelocity = 30.0f; // radians per second
		};

	public:
		PhysicsEngine();

		void Initialize(Scene* scene);
		void Update(double ts);

		double GetAltitudeSimple(const Vector3& worldPos);
		double GetAltitude(Entity& entity, bool ignoreWorldTranslation = false);
		double GetAltitudeAtWorldPos(const Vector3& worldPos, double& outRadialDist, Vector3& outGroundNormal);
		double GetAltitudeBoxCollider(Entity& entity);
		double GetAltitudeSphereCollider(Entity& entity);

		double GetAirDensity(double altitude);

		void ApplyLinearImpulse(RigidBodyComponent& rbc, Vector3 impulse);
		void ApplyLinearImpulseAtPoint(RigidBodyComponent& rbc, Vector3 impulse, Vector3 worldPoint, Vector3 comWorld);

		DirectX::XMVECTOR GetCameraPlanetSpace() const;

		PhysicsSettings& GetSettings() { return mSettings; }
	private:
		void ApplyImpulseAngular(RigidBodyComponent& rbc, Matrix objectInvInertiaWorld, Vector3 impulse);

		void ApplyGravity(Entity& entity, double ts);
		void ApplyAeroDrag(Entity& entity, double ts);

		void IntegrateLinear(Entity& entity, double ts);
		void IntegrateAngular(Entity& entity, double ts);

		bool CheckTerrainCollision(Entity& entity, TerrainContactManifold& manifold);
		bool FindTerrainContactPoints(Entity& entity, TerrainContactManifold& manifold);
		bool FindTerrainContactPointsSphere(Entity& entity, TerrainContactManifold& manifold);
		bool FindTerrainContactPointsBox(Entity& entity, TerrainContactManifold& manifold);
		void ResolveTerrainCollision(TerrainContactManifold& manifold, double dt);

		void UpdateMassProperties();
		Matrix ComputeSphereInertiaTensor(double mass, double radius);
		Matrix ComputeBoxInertiaTensor(double mass, const Vector3& boxSize, const Vector3& centerOfMass);
	private:
		Scene* mScene;

		PhysicsSettings mSettings;

		Ref<Mesh> mGuideMesh;

		friend class Scene;
	};
}