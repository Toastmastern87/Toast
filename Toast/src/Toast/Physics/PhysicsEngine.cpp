#include "tpch.h"
#include "PhysicsEngine.h"

#include "Toast/Renderer/MeshFactory.h"
#include "Toast/Renderer/RendererDebug.h"

#include "Toast/Utils/PerlinNoise.h"

#include "Toast/Renderer/TerrainSampler.h"

namespace Toast {

	// lodFine = LodFromCellSize(CellSize) on GPU.
	// On CPU you need to define what LOD you want to use for physics queries.
	static float AccumulateHeightDetails(const std::vector<HeightDetail>& details, float px, float py, float pz, int lod)
	{
		float sum = 0.0f;

		for (const auto& d : details)
		{
			if (lod <= d.GPUSettings.LODActivation)
			{
				sum += FractalPerlin3D(d.Perm, px, py, pz, d.GPUSettings.Octaves,	d.GPUSettings.Frequency, d.GPUSettings.Amplitude);
			}
		}

		return sum;
	}

	PhysicsEngine::PhysicsEngine()
	{
		mScene = nullptr;
	}

	void PhysicsEngine::Initialize(Scene* scene)
	{
		mScene = scene;

		mGuideMesh = MeshFactory::CreateCube(1.0f, { 1.0, 0.0, 0.0 });
	}

	void PhysicsEngine::Update(double ts)
	{
		float targetFrameTime = 1.0f / (float)mSettings.FPSTarget;
		float subStepDeltaTime = targetFrameTime / mSettings.StepsPerUpdate;

		mSettings.ElapsedTime += ts;

		while (mSettings.ElapsedTime >= targetFrameTime)
		{
			auto view = mScene->mRegistry.view<RigidBodyComponent, TransformComponent>();

			for (auto entity : view)
			{
				Entity e = { entity, mScene };

				for (int i = 0; i < mSettings.StepsPerUpdate; ++i)
				{
					ApplyGravity(e, subStepDeltaTime);

					IntegrateLinear(e, subStepDeltaTime);
					IntegrateAngular(e, subStepDeltaTime);

					TerrainContactManifold manifold;
					if (CheckTerrainCollision(e, manifold))
						ResolveTerrainCollision(manifold, subStepDeltaTime);
				}
			}

			mSettings.ElapsedTime -= targetFrameTime;
		}
	}

	double PhysicsEngine::GetAltitude(Entity& entity, bool ignoreWorldTranslation)
	{
		Vector3 worldTranslation = mScene->GetMainCamera()->GetWorldTranslation();

		TransformComponent& tc = entity.GetComponent<TransformComponent>();

		double radialDist;
		Vector3 normal;

		if (ignoreWorldTranslation)
			return GetAltitudeAtWorldPos(Vector3(tc.Translation), radialDist, normal);
		else
			return GetAltitudeAtWorldPos(Vector3(tc.Translation) + worldTranslation, radialDist, normal);
	}

	double PhysicsEngine::GetAltitudeAtWorldPos(const Vector3& worldPos, double& outRadialDist, Vector3& outGroundNormal)
	{
		Planet& planet = *mScene->GetPlanet();
		TerrainData& terrain = planet.GetTerrainData();
		Vector3 worldTranslation = mScene->GetMainCamera()->GetWorldTranslation();

		// Planet center and true radial distance
		Vector3 planetCenterCR = Vector3(planet.GetTranslation()) + worldTranslation;

		Vector3 pLocal = worldPos - planetCenterCR;

		double r = pLocal.Length();
		if (r > 0.0)
			outGroundNormal = pLocal / r;   // normalized radial normal
		else
			outGroundNormal = Vector3(0.0, 1.0, 0.0); // fallback, shouldn't really happen

		outRadialDist = r;
		
		Vector3 nWS = Vector3::Normalize(pLocal);

		Vector3 basisLonEast = planet.GetBasisLonEast();   // same as BasisLonEast in PlanetFrame
		Vector3 basisSpinUp = planet.GetBasisSpinUp();    // same as BasisSpinUp
		Vector3 basisLonNorth = planet.GetBasisLonNorth();  // same as BasisLonNorth

		double vx = Vector3::Dot(nWS, basisLonEast);
		double vy = Vector3::Dot(nWS, basisSpinUp);
		double vz = Vector3::Dot(nWS, basisLonNorth);

		Vector3 vPlanet = Vector3(vx, vy, vz);

		double height = SampleHeightFromDir(planet.GetTerrainCubeData(), vPlanet);

		float px = (float)(vPlanet.x * planet.GetRadius());
		float py = (float)(vPlanet.y * planet.GetRadius());
		float pz = (float)(vPlanet.z * planet.GetRadius());

		uint32_t lod = planet.GetLODForWorldPos(worldPos);
		float heightDetails = AccumulateHeightDetails(planet.GetHeightDetails(), px, py, pz, lod);

		height += (double)heightDetails;

		double altitude = pLocal.Length() - (planet.GetRadius() + height);

		return altitude;
	}

	double PhysicsEngine::GetAltitudeBoxCollider(Entity& entity)
	{
		Vector3 worldTranslation = mScene->GetMainCamera()->GetWorldTranslation();

		BoxColliderComponent bcc;
		TransformComponent tc;

		tc = entity.GetComponent<TransformComponent>();
		bcc = entity.GetComponent<BoxColliderComponent>();
		ShapeBox* box = bcc.Collider.get();

		Planet& planet = *mScene->GetPlanet();
		Vector3 planetCenterCR = Vector3(planet.GetTranslation()) + worldTranslation;

		Vector3 lowestPointWorld = Vector3(DBL_MAX, DBL_MAX, DBL_MAX);
		for (const Vector3& cornerLocal : box->mPoints)
		{
			Vector3 cornerWorld = Matrix::TransformPointRowVector(cornerLocal, Matrix(tc.GetTransformWithoutScale()));
			cornerWorld = cornerWorld + worldTranslation;

			Vector3 d = cornerWorld - planetCenterCR;
			if(d.LengthSquared() < (lowestPointWorld - planetCenterCR).LengthSquared())
				lowestPointWorld = cornerWorld;
		}

		double radialDist;
		Vector3 groundNormal;
		return GetAltitudeAtWorldPos(lowestPointWorld, radialDist, groundNormal);
	}

	double PhysicsEngine::GetAltitudeSphereCollider(Entity& entity)
	{
		SphereColliderComponent scc;

		scc = entity.GetComponent<SphereColliderComponent>();

		return 0.0; // TODO
	}

	void PhysicsEngine::ApplyLinearImpulse(RigidBodyComponent& rbc, Vector3 impulse)
	{
		if (rbc.InvMass == 0.0)
			return;

		rbc.LinearVelocity += (impulse * rbc.InvMass);
	}

	void PhysicsEngine::ApplyImpulseAngular(RigidBodyComponent& rbc, Matrix invInertiaWorld, Vector3 impulse)
	{
		if (rbc.InvMass == 0.0)
			return;

		rbc.AngularVelocity += Matrix::MulMat3(invInertiaWorld, impulse);

		if (rbc.AngularVelocity.Length() > mSettings.MaxAngularVelocity)
			rbc.AngularVelocity = Vector3::Normalize(rbc.AngularVelocity) * mSettings.MaxAngularVelocity;
	}

	void PhysicsEngine::ApplyGravity(Entity& entity, double ts)
	{
		Vector3 worldTranslation = mScene->GetMainCamera()->GetWorldTranslation();
		double gravityConstant = (double)mScene->GetPlanet()->GetGravityConstant();
		Vector3 planetPos = Vector3(mScene->GetPlanet()->GetTranslation()) + worldTranslation;

		RigidBodyComponent& rbc = entity.GetComponent<RigidBodyComponent>();
		TransformComponent& tc = entity.GetComponent<TransformComponent>();

		Vector3 objectPos = Vector3(tc.Translation) + worldTranslation;

		Vector3 gravityImpulse = Vector3::Normalize(planetPos - objectPos) * gravityConstant * (1.0 / rbc.InvMass) * ts;

		ApplyLinearImpulse(rbc, gravityImpulse);
	}

	void PhysicsEngine::IntegrateLinear(Entity& entity, double ts)
	{
		RigidBodyComponent& rbc = entity.GetComponent<RigidBodyComponent>();
		TransformComponent& tc = entity.GetComponent<TransformComponent>();

		// Skip static entities
		if (rbc.InvMass == 0.0) 
			return;

		Vector3 deltaPos = rbc.LinearVelocity * (float)ts;

		tc.Translation = { tc.Translation.x + (float)deltaPos.x, tc.Translation.y + (float)deltaPos.y, tc.Translation.z + (float)deltaPos.z };
	}

	void PhysicsEngine::IntegrateAngular(Entity& entity, double ts)
	{
		auto& rbc = entity.GetComponent<RigidBodyComponent>();
		auto& tc = entity.GetComponent<TransformComponent>();

		// Static bodies don’t rotate
		if (rbc.InvMass == 0.0)
			return;

		Vector3 omega = rbc.AngularVelocity; // rad/s, world space
		double wLen = omega.Length();

		if (wLen < 1e-6)
			return;

		double maxW = mSettings.MaxAngularVelocity;
		if (wLen > maxW)
		{
			omega = omega * (maxW / wLen);
			wLen = maxW;
			rbc.AngularVelocity = omega;
		}

		double angle = wLen * ts;      // radians
		if (angle < 1e-6)
			return;

		Vector3 axis = omega / wLen;

		// Δq representing this small rotation (w, x, y, z)
		Quaternion deltaQ = Quaternion::FromAxisAngle(axis, angle);

		// Current orientation
		Quaternion q = tc.RotationQuaternion;

		// If angular velocity is in WORLD space, we left-multiply: new = Δq * q
		q = Quaternion::Normalize(deltaQ * q);

		tc.RotationQuaternion = { (float)q.x, (float)q.y, (float)q.z, (float)q.w };
	}

	bool PhysicsEngine::CheckTerrainCollision(Entity& entity, TerrainContactManifold& manifold)
	{
		Vector3 worldTranslation = mScene->GetMainCamera()->GetWorldTranslation();

		SphereColliderComponent* scc = nullptr;
		BoxColliderComponent* bcc = nullptr;

		auto& rbc = entity.GetComponent<RigidBodyComponent>();

		if(entity.HasComponent<SphereColliderComponent>())
			scc = &entity.GetComponent<SphereColliderComponent>();
		else if (entity.HasComponent<BoxColliderComponent>())
			bcc = &entity.GetComponent<BoxColliderComponent>();
		else
			return false;

		if (rbc.InvMass == 0.0)
			return false;

		auto& tc = entity.GetComponent<TransformComponent>();
		Vector3 centerWS = Vector3(tc.Translation) + worldTranslation;

		Vector3 groundNormal;
		double radialDist;
		double centerAlt = GetAltitudeAtWorldPos(centerWS, radialDist, groundNormal);

		double bottomAlt = 0.0;
		if(scc)
		{
			double radius = scc->Collider->mRadius;
			bottomAlt = centerAlt - radius;
		}
		else if(bcc)
		{
			// Simple, robust: approximate box by bounding sphere for the quick check
			// radius = half-diagonal of the box in local space
			const Bounds& b = bcc->Collider->GetBounds(); // add a getter if you don’t have one
			Vector3 half = (b.maxs - b.mins) * 0.5;
			double radius = std::sqrt(half.x * half.x + half.y * half.y + half.z * half.z);
			bottomAlt = centerAlt - radius;
		}

		if (bottomAlt > 0.0)
			return false;

		manifold.Entity = entity;
		manifold.Points.clear();

		return FindTerrainContactPoints(entity, manifold);
	}

	bool PhysicsEngine::FindTerrainContactPoints(Entity& entity, TerrainContactManifold& manifold)
	{
		if (entity.HasComponent<SphereColliderComponent>())
			return FindTerrainContactPointsSphere(entity, manifold);
		else if (entity.HasComponent<BoxColliderComponent>())
			return FindTerrainContactPointsBox(entity, manifold);
	}

	bool PhysicsEngine::FindTerrainContactPointsBox(Entity& entity, TerrainContactManifold& manifold)
	{
		Vector3 worldTranslation = mScene->GetMainCamera()->GetWorldTranslation();

		auto& tc = entity.GetComponent<TransformComponent>();
		auto& bcc = entity.GetComponent<BoxColliderComponent>();
		Planet& planet = *mScene->GetPlanet();

		ShapeBox* box = bcc.Collider.get();

		Vector3 planetCenter = Vector3(planet.GetTranslation()) + worldTranslation;

		bool hasContact = false;
		std::vector<TerrainContactPoint> contacts;

		for (const Vector3& cornerLocal : box->mPoints)
		{
			double radialDist;
			Vector3 groundNormal;

			Vector3 cornerWorld = Matrix::TransformPointRowVector(cornerLocal, Matrix(tc.GetTransformWithoutScale())); 
			cornerWorld = cornerWorld + worldTranslation;

			//DirectX::XMMATRIX transform = DirectX::XMMatrixRotationQuaternion(DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(tc.RotationEulerAngles.x), DirectX::XMConvertToRadians(tc.RotationEulerAngles.y), DirectX::XMConvertToRadians(tc.RotationEulerAngles.z))) * DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&tc.RotationQuaternion)) * DirectX::XMMatrixTranslation(cornerWorld.x, cornerWorld.y, cornerWorld.z);

			//RendererDebug::SubmitDebugMesh(mGuideMesh, transform);

			double alt = GetAltitudeAtWorldPos(cornerWorld, radialDist, groundNormal);

			if (alt <= 0.0)
			{
				TerrainContactPoint collisionPoint;
				collisionPoint.Normal = groundNormal;
				collisionPoint.Penetration = -alt; // corner altitude below terrain

				// terrain surface radius at this corner
				double surfaceRadius = radialDist - alt;

				collisionPoint.Position = planetCenter + groundNormal * surfaceRadius;

				contacts.push_back(collisionPoint);
				hasContact = true;
			}
		}

		if (!hasContact)
			return false;

		// For now, keep all corner contacts
		manifold.Points.insert(manifold.Points.end(), contacts.begin(), contacts.end());
		return true;
	}

	bool PhysicsEngine::FindTerrainContactPointsSphere(Entity& entity, TerrainContactManifold& manifold)
	{
		Vector3 worldTranslation = mScene->GetMainCamera()->GetWorldTranslation();

		auto& tc = entity.GetComponent<TransformComponent>();
		auto& scc = entity.GetComponent<SphereColliderComponent>();
		Planet& planet = *mScene->GetPlanet();

		double radius = scc.Collider->mRadius;

		double radialDist;
		Vector3 groundNormal;

		double centerAlt = GetAltitudeAtWorldPos(Vector3(tc.Translation) + worldTranslation, radialDist, groundNormal);

		double bottomAlt = centerAlt - radius;

		//If above terrain, no collision
		if (bottomAlt > 0.0)
			return false;

		TerrainContactPoint collisionPoint;
		collisionPoint.Normal = groundNormal;

		collisionPoint.Penetration = -bottomAlt;

		Vector3 planetCenter = Vector3(planet.GetTranslation()) + worldTranslation;

		double surfaceRadius = radialDist - centerAlt;

		collisionPoint.Position = planetCenter + groundNormal * surfaceRadius;

		manifold.Points.push_back(collisionPoint);
		return true;
	}

	void PhysicsEngine::ResolveTerrainCollision(TerrainContactManifold& manifold, double dt)
	{
		Vector3 worldTranslation = mScene->GetMainCamera()->GetWorldTranslation();

		Entity& entity = manifold.Entity;
		auto& rbc = entity.GetComponent<RigidBodyComponent>();
		auto& tc = entity.GetComponent<TransformComponent>();

		if (rbc.InvMass == 0.0 || manifold.Points.empty())
			return;

		const TerrainContactPoint* deepest = nullptr;
		double maxPen = 0.0;

		Vector3 normal(0.0, 0.0, 0.0);

		for (const auto& cp : manifold.Points)
		{
			// Weighted by penetration so more "important" contacts dominate
			normal += cp.Normal * cp.Penetration;

			if (!deepest || cp.Penetration > maxPen)
			{
				maxPen = cp.Penetration;
				deepest = &cp;
			}
		}

		if (maxPen <= 0.0 || !deepest)
			return;

		normal = Vector3::Normalize(normal);

		const double slop = 0.0001;  // very small tolerance
		const double percent = 0.8;     // solve 80% this frame

		double correctionMag = std::max(maxPen - slop, 0.0) * percent;

		Vector3 pos = Vector3(tc.Translation);
		pos += normal * correctionMag;
		tc.Translation = { (float)pos.x, (float)pos.y, (float)pos.z };

		Vector3 linearVelocity = rbc.LinearVelocity;
		Vector3 angularVelocity = rbc.AngularVelocity;

		Vector3 contactPos = deepest->Position;

		Matrix worldNoScale = tc.GetTransformWithoutScale();
		Vector3 CoMWorld = Matrix::TransformPointRowVector(rbc.CenterOfMass, worldNoScale);
		Vector3 CoMPS = CoMWorld + worldTranslation;

		Vector3 r = contactPos - CoMPS;

		Vector3 vRel = linearVelocity + Vector3::Cross(angularVelocity, r);

		double vRelN = Vector3::Dot(vRel, normal);

		double elasticity = rbc.Elasticity;

		const double bounceThreshold = 0.1; // in your velocity units
		if (std::abs(vRelN) < bounceThreshold)
			elasticity = 0.0;

		double invMass = rbc.InvMass;

		Vector3 rn = Vector3::Cross(r, normal);

		Ref<Shape> collider; 
		if (entity.HasComponent<SphereColliderComponent>())
			collider = entity.GetComponent<SphereColliderComponent>().Collider;
		else if (entity.HasComponent<BoxColliderComponent>())
			collider = entity.GetComponent<BoxColliderComponent>().Collider;

		Matrix rotationMatrix = Matrix(entity.GetComponent<TransformComponent>().GetRotation());
		Matrix invInertiaWorld = rotationMatrix * collider->GetInvInertiaTensor() * rotationMatrix.Transpose();

		Vector3 invIrn = Matrix::MulMat3(invInertiaWorld, rn);
		double angularTerm = Vector3::Dot(Vector3::Cross(invIrn, r), normal);

		double denom = invMass + angularTerm;

		if (denom < 1e-8)
			return; // avoid divide by zero / super heavy body

		double j = -(1.0 + elasticity) * vRelN / denom;

		Vector3 impulse = j * normal;

		ApplyLinearImpulse(rbc, impulse);
		 
		Vector3 torqueImpulse = Vector3::Cross(r, impulse);

		ApplyImpulseAngular(rbc, invInertiaWorld, torqueImpulse);

		// Friction, Coulomb

		// Recompute relative velocity AFTER normal impulse (important!)
		Vector3 vRel2 = rbc.LinearVelocity + Vector3::Cross(rbc.AngularVelocity, r);

		// Tangential component (remove normal component)
		double vRelN2 = Vector3::Dot(vRel2, normal);
		Vector3 vT = vRel2 - normal * vRelN2;

		double vTlen = vT.Length();
		if (vTlen > 1e-6)
		{
			Vector3 t = vT / vTlen; // tangent direction opposing slip will be handled by sign in jt

			Vector3 rt = Vector3::Cross(r, t);
			Vector3 invI_rt = Matrix::MulMat3(invInertiaWorld, rt);
			double angularTermT = Vector3::Dot(Vector3::Cross(invI_rt, r), t);

			double denomT = rbc.InvMass + angularTermT;
			if (denomT > 1e-8)
			{
				// Desired friction impulse to cancel tangential velocity
				// (negative sign because we want to oppose current tangential motion)
				double jt = -Vector3::Dot(vRel2, t) / denomT;

				// Coulomb limit based on normal impulse magnitude j (from your normal solve)
				double muS = rbc.StaticFriction;
				double muD = rbc.DynamicFriction;

				// If required jt is within static cone => static friction (stick)
				// otherwise dynamic friction (slide)
				double jtMaxStatic = muS * j;

				Vector3 frictionImpulse;
				if (std::abs(jt) <= jtMaxStatic)
				{
					// static: fully cancel tangential motion (within limit)
					frictionImpulse = jt * t;
				}
				else
				{
					// dynamic: clamp to muD * j in opposite direction of slip
					double jtClamped = -muD * j * (Vector3::Dot(vRel2, t) > 0.0 ? 1.0 : -1.0);
					frictionImpulse = jtClamped * t;
				}

				ApplyLinearImpulse(rbc, frictionImpulse);

				Vector3 frictionTorque = Vector3::Cross(r, frictionImpulse);
				ApplyImpulseAngular(rbc, invInertiaWorld, frictionTorque);
			}
		}
	}

}