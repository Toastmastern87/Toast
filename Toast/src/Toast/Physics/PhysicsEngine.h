#pragma once

#include "Toast/Scene/Components.h"

#include "Toast/Renderer/RendererDebug.h"
#include "Toast/Renderer/PlanetSystem.h"

#include <../vendor/directxtex/include/DirectXTex.h>

#define NOMINMAX
#include <algorithm>

#include <DirectXMath.h>

#define MAX_INT_VALUE	65535.0
#define M_PI			3.14159265358979323846
#define M_PIDIV2		(3.14159265358979323846 / 2.0)

namespace Toast {

	namespace PhysicsEngine {
		struct ContactPoint 
		{
			Vector3 PtOnPlanetWorldSpace;
			Vector3 PtOnObjectWorldSpace;

			ContactPoint() = default;
			ContactPoint(const Vector3& ptOnPlanet, const Vector3& ptOnObject) : PtOnPlanetWorldSpace(ptOnPlanet), PtOnObjectWorldSpace(ptOnObject) {}
		};

		struct TerrainCollision
		{
			Entity* Planet;
			Entity* Object;
			
			Vector3 Normal;
			double Depth;

			std::vector<ContactPoint> ContactPoints;
		};

		static Vector3 ClosestPointOnTriangle(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& p) {
			// Compute vectors
			Vector3 ab = b - a;
			Vector3 ac = c - a;
			Vector3 ap = p - a;

			// Compute dot products
			double d1 = Vector3::Dot(ab, ap);
			double d2 = Vector3::Dot(ac, ap);

			// Check if P in vertex region outside A
			if (d1 <= 0.0 && d2 <= 0.0) return a;

			// Check if P in vertex region outside B
			Vector3 bp = p - b;
			double d3 = Vector3::Dot(ab, bp);
			double d4 = Vector3::Dot(ac, bp);
			if (d3 >= 0.0 && d4 <= d3) return b;

			// Check if P in edge region of AB
			double vc = d1 * d4 - d3 * d2;
			if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0) {
				double v = d1 / (d1 - d3);
				return a + ab * v;
			}

			// Check if P in vertex region outside C
			Vector3 cp = p - c;
			double d5 = Vector3::Dot(ab, cp);
			double d6 = Vector3::Dot(ac, cp);
			if (d6 >= 0.0 && d5 <= d6) return c;

			// Check if P in edge region of AC
			double vb = d5 * d2 - d1 * d6;
			if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0) {
				double w = d2 / (d2 - d6);
				return a + ac * w;
			}

			// Check if P in edge region of BC
			double va = d3 * d6 - d5 * d4;
			if (va <= 0.0 && (d4 - d3) >= 0.0 && (d5 - d6) >= 0.0) {
				double w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
				return b + (c - b) * w;
			}

			// P inside face region. Compute Q through its barycentric coordinates (u,v,w)
			double denom = 1.0 / (va + vb + vc);
			double v = vb * denom;
			double w = vc * denom;
			return a + ab * v + ac * w;
		}

		static std::pair<double, double> ProjectShapeOntoAxis(const std::vector<Vector3>& vertices, const Vector3& axis) {
			double minProjection = DBL_MAX;
			double maxProjection = -DBL_MAX;

			for (int i = 0; i < vertices.size(); i++)
			{
				double projection = Vector3::Dot(vertices.at(i), axis);

				minProjection = (std::min)(minProjection, projection);
				maxProjection = (std::max)(maxProjection, projection);
			}

			return { minProjection, maxProjection };
		}

		static std::tuple<bool, double, Vector3> OverlapOnAxis(const std::vector<Vector3>& obbVertices, const std::vector<Vector3>& triangleVertices, const Vector3& axis, double minPenetration) {
			auto [obbMin, obbMax] = ProjectShapeOntoAxis(obbVertices, axis);
			auto [triMin, triMax] = ProjectShapeOntoAxis(triangleVertices, axis);

			if (obbMin >= triMax || triMin >= obbMax)
				return { false, minPenetration, axis };

			double penetration = (std::min)(triMax - obbMin, obbMax - triMin);

			if (penetration < minPenetration)
				minPenetration = penetration;

			return { true, minPenetration, axis };
		}

		static void FindContactPointsOBB(const Vector3 planetPos, const std::vector<Vector3>& colliderPts, const std::vector<Vector3>& terrainPts, TerrainCollision& collision)
		{
			Vector3 p1 = terrainPts[0];
			Vector3 p2 = terrainPts[1];
			Vector3 p3 = terrainPts[2];

			// Calculate vectors lying on the plane
			Vector3 v1 = { p2.x - p1.x, p2.y - p1.y, p2.z - p1.z };
			Vector3 v2 = { p3.x - p1.x, p3.y - p1.y, p3.z - p1.z };

			// Calculate the normal vector to the plane
			Vector3 planeNormal = Vector3::Cross(v1, v2);

			// Plane coefficients
			double a = planeNormal.x;
			double b = planeNormal.y;
			double c = planeNormal.z;
			double d = -Vector3::Dot(planeNormal, p1);

			std::vector<Vector3> pointsBelowPlane;
			for (const auto& p : colliderPts) 
			{
				double f = a * p.x + b * p.y + c * p.z + d;
				if (f > 0.0) 
				{
					// Calculate t
					double denominator = a * collision.Normal.x + b * collision.Normal.y + c * collision.Normal.z;
					if (denominator != 0)
					{ // Avoid division by zero
						double t = -f / denominator;
						// Calculate the intersection point
						Vector3 ptOnPlanet = p + collision.Normal * t;
						ContactPoint newContact = { ptOnPlanet, p };

						bool addNewContact = true;

						for (const auto& pt : collision.ContactPoints)
						{
							double distance = (pt.PtOnObjectWorldSpace - newContact.PtOnObjectWorldSpace).Magnitude();

							if (distance < 0.1)
							{
								addNewContact = false;
								break;
							}
						}

						if (addNewContact)
							collision.ContactPoints.emplace_back(newContact);
					}
				}
			}
		}

		static bool BoxPlanetCollisionCheck(TerrainCollision& collision, const Vector3 triangleNormal, const Vector3 A, const Vector3 B, const Vector3 C )
		{
			auto& planetPos = collision.Planet->GetComponent<TransformComponent>().Translation;
			std::vector<Vector3> axes, colliderPts, terrainPts;
			auto& rotEuler = collision.Object->GetComponent<TransformComponent>().RotationEulerAngles;
			auto& rotQuat = collision.Object->GetComponent<TransformComponent>().RotationQuaternion;
			auto& scale = collision.Object->GetComponent<BoxColliderComponent>().Collider->mSize;
			auto& objectPos = collision.Object->GetComponent<TransformComponent>().Translation;
			std::vector<Vertex> colliderVertices = collision.Object->GetComponent<BoxColliderComponent>().ColliderMesh->GetVertices();

			Matrix objTransform = DirectX::XMMatrixIdentity() * DirectX::XMMatrixScaling(scale.x, scale.y, scale.z)
				* (DirectX::XMMatrixRotationQuaternion(DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(rotEuler.x), DirectX::XMConvertToRadians(rotEuler.y), DirectX::XMConvertToRadians(rotEuler.z)))) * DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&rotQuat))
				* DirectX::XMMatrixTranslation(objectPos.x, objectPos.y, objectPos.z);

			for (auto& vertex : colliderVertices)
			{
				colliderPts.emplace_back(vertex.Position);
				colliderPts.at(colliderPts.size() - 1) = objTransform * colliderPts.at(colliderPts.size() - 1);
			}

			terrainPts.emplace_back(A);
			terrainPts.emplace_back(B);
			terrainPts.emplace_back(C);

			Matrix colliderRot = { DirectX::XMMatrixIdentity() * (DirectX::XMMatrixRotationQuaternion(DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(rotEuler.x), DirectX::XMConvertToRadians(rotEuler.y), DirectX::XMConvertToRadians(rotEuler.z)))) * DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&collision.Object->GetComponent<TransformComponent>().RotationQuaternion)) };
			Ref<ShapeBox> collider = collision.Object->GetComponent<BoxColliderComponent>().Collider;

			Vector3 obbAxes[3] = {
				colliderRot * Vector3(1.0, 0.0, 0.0),  // Local X-axis
				colliderRot * Vector3(0.0, 1.0, 0.0),  // Local Y-axis
				colliderRot * Vector3(0.0, 0.0, 1.0)   // Local Z-axis
			};

			axes.emplace_back(triangleNormal);
			axes.insert(axes.end(), std::begin(obbAxes), std::end(obbAxes));

			Vector3 triEdges[3] = {
				B - A,
				C - B,
				A - C
			};

			for (const auto& obbAxis : obbAxes) {
				for (const auto& triEdge : triEdges) {
					Vector3 crossProduct = Vector3::Cross(obbAxis, triEdge);
					crossProduct = Vector3::Normalize(crossProduct);
					axes.emplace_back(crossProduct);
				}
			}

			double minPenetration = DBL_MAX;
			for (auto& axis : axes)
			{
				auto [overlap, penetration, normal] = OverlapOnAxis(colliderPts, terrainPts, axis, minPenetration);
				if (!overlap)
					return false; // No overlap found, shapes do not collide

				if (penetration < minPenetration) 
					minPenetration = penetration;
				
			}

			collision.Depth = minPenetration;
			collision.Normal = Vector3::Normalize(Vector3(objectPos) - Vector3(planetPos));

			FindContactPointsOBB(planetPos, colliderPts, terrainPts, collision);

			return true;
		}

		static bool SphereTerrainCollisionCheck(Vector3 sphereCenter, double radius, const double dt, TerrainCollision& collision, const std::vector<Vertex>& terrainVertices, const std::vector<uint32_t>& terrainIndices)
		{
			collision.Depth = -DBL_MAX;
			collision.Normal = Vector3(0.0, 0.0, 0.0);
			bool collisionDetected = false;
			double minDistanceSquared = DBL_MAX;

			for (size_t i = 0; i < terrainIndices.size() - 2; i += 3)
			{
				Vector3 va = terrainVertices[terrainIndices[i]].Position;
				Vector3 vb = terrainVertices[terrainIndices[i + 1]].Position;
				Vector3 vc = terrainVertices[terrainIndices[i + 2]].Position;

				Vector3 closestPoint = ClosestPointOnTriangle(va, vb, vc, sphereCenter);

				Vector3 diff = closestPoint - sphereCenter;
				double distanceSqr = diff.MagnitudeSqrt();

				if (distanceSqr < minDistanceSquared)
				{
					minDistanceSquared = distanceSqr;

					double distance = sqrt(distanceSqr);
					collision.Depth = radius - distance;

					if (distance > 0)
						collision.Normal = diff / distance;
					else
					{
						Vector3 faceNormal = Vector3::Cross(vb - va, vc - va);
						if (faceNormal.MagnitudeSqrt() > 0)
							collision.Normal = Vector3::Normalize(faceNormal);
						else
							collision.Normal = Vector3(0.0, 1.0, 0.0);
					}

					Vector3 sphereContactPoint = sphereCenter - collision.Normal * (radius - collision.Depth);

					collision.ContactPoints.clear();
					collision.ContactPoints.emplace_back();
					collision.ContactPoints.back().PtOnPlanetWorldSpace = closestPoint;
					collision.ContactPoints.back().PtOnObjectWorldSpace = sphereContactPoint;

					collisionDetected = collision.Depth >= 0;
				}
			}

			return collisionDetected;
		}

		static void ApplyLinearImpulse(RigidBodyComponent& rbc, Vector3 impulse)
		{
			if (rbc.InvMass == 0.0)
				return;

			rbc.LinearVelocity += (impulse * rbc.InvMass);
		}

		static void ApplyImpulseAngular(RigidBodyComponent& rbc, Matrix objectInvInertiaWorld, Vector3 impulse)
		{
			if (rbc.InvMass == 0.0)
				return;

			rbc.AngularVelocity += objectInvInertiaWorld * impulse;

			const double maxAngularSpeed = 30.0;

			if (rbc.AngularVelocity.Magnitude() > maxAngularSpeed)
				rbc.AngularVelocity = Vector3::Normalize(rbc.AngularVelocity) * maxAngularSpeed;
		}
		
		static TerrainData LoadTerrainData(const char* path, const double maxAltitude, const double minAltitude)
		{
			HRESULT result;

			std::wstring w;
			std::copy(path, path + strlen(path), back_inserter(w));
			const WCHAR* pathWChar = w.c_str();

			DirectX::TexMetadata heightMapMetadata;
			DirectX::ScratchImage* heightMap = new DirectX::ScratchImage();

			result = DirectX::LoadFromWICFile(pathWChar, DirectX::WIC_FLAGS_NONE, &heightMapMetadata, *heightMap);

			TOAST_CORE_ASSERT(SUCCEEDED(result), "Unable to load height map!");

			TOAST_CORE_INFO("Terrain data loaded width: %d, height: %d, format: %d", heightMapMetadata.width, heightMapMetadata.height, heightMapMetadata.format);

			const uint16_t* rawTerrainData = reinterpret_cast<const uint16_t*>(heightMap->GetPixels());

			size_t totalPixels = heightMapMetadata.width * heightMapMetadata.height;
			TerrainData terrainDataUpdated;
			terrainDataUpdated.HeightData.reserve(totalPixels);

			for (size_t i = 0; i < totalPixels; ++i) 
				terrainDataUpdated.HeightData.emplace_back(((static_cast<double>(rawTerrainData[i]) / MAX_INT_VALUE) * (maxAltitude - minAltitude)) + minAltitude);

			terrainDataUpdated.RowPitch = heightMap->GetImage(0, 0, 0)->rowPitch;
			terrainDataUpdated.Width = heightMapMetadata.width;
			terrainDataUpdated.Height = heightMapMetadata.height;

			return terrainDataUpdated;
		}

		static double GetObjectDistanceToPlanet(Entity* planet, Vector3& worldSpaceObjectPos, Vector3& triangleNormal, Vector3& A, Vector3& B, Vector3& C)
		{	
			PlanetComponent& pc = planet->GetComponent<PlanetComponent>();
			Vector3 worldSpacePlanetPos = planet->GetComponent<TransformComponent>().Translation;

			std::vector<uint32_t> indices = pc.RenderMesh->GetIndices();
			std::vector<Vertex> vertices = pc.RenderMesh->GetVertices();

			double distance;

			if (indices.size() > 0)
			{
				for (int i = 0; i < indices.size() - 2; i += 3)
				{
					distance = Math::PointToPlaneDistance(worldSpaceObjectPos, worldSpacePlanetPos, vertices[indices[i]].Position, vertices[indices[i + 1]].Position, vertices[indices[i + 2]].Position);

					if (distance != -100.0f) 
					{
						Vector3 v1(vertices[indices[i + 1]].Position);
						Vector3 v2(vertices[indices[i]].Position);
						Vector3 v3(vertices[indices[i + 2]].Position);

						A = v1;
						B = v2;
						C = v3;

						triangleNormal = Vector3::Normalize(Vector3::Cross(v1 - v2, v3 - v1));
						return distance;
					}
				}
			}

			return distance;
		}

		static void UpdateBody(Entity& body, float dt)
		{
			TransformComponent& tc = body.GetComponent<TransformComponent>();
			RigidBodyComponent& rbc = body.GetComponent<RigidBodyComponent>();

			// Update position due to LinearVelocity
			Vector3 translation = { tc.Translation };
			Vector3 deltaTranslation = rbc.LinearVelocity * dt;
			translation += deltaTranslation;
			tc.Translation = { (float)translation.x , (float)translation.y, (float)translation.z };

			// Update rotation due to AngularVelocity
			Quaternion updatedQuaternion = tc.RotationQuaternion;

			updatedQuaternion = Quaternion::Normalize(updatedQuaternion + (Quaternion(rbc.AngularVelocity * dt * 0.5, 0.0) * Quaternion(tc.RotationQuaternion)));// *Quaternion(tc.RotationQuaternion));


			tc.RotationQuaternion = { (float)updatedQuaternion.x, (float)updatedQuaternion.y, (float)updatedQuaternion.z, (float)updatedQuaternion.w };
		}

		static bool TerrainCollisionCheck(Entity* planet, Entity* object, TerrainCollision& collision, float dt)
		{
			collision.Planet = planet;
			collision.Object = object;

			RigidBodyComponent rbcObject = object->GetComponent<RigidBodyComponent>();
			TransformComponent objectTC = object->GetComponent<TransformComponent>();
			TransformComponent planetTC = planet->GetComponent<TransformComponent>();
			PlanetComponent& planetPC = planet->GetComponent<PlanetComponent>();

			Vector3 posPlanet = { planet->GetComponent<TransformComponent>().Translation };
			Vector3 posObject = { object->GetComponent<TransformComponent>().Translation };

			Matrix planetTransform = { planetTC.GetTransform() };
			Matrix objectTransform = { objectTC.GetTransform() };

			Vector3 triangleNormal, A, B, C;
			double objectDistance = GetObjectDistanceToPlanet(planet, posObject, triangleNormal, A, B, C);

			if (object->HasComponent<SphereColliderComponent>())
			{
				double sphereRadius = object->GetComponent<SphereColliderComponent>().Collider->mRadius;

				bool collisionDetected = SphereTerrainCollisionCheck(posObject, sphereRadius, dt, collision, planetPC.RenderMesh->GetVertices(), planetPC.RenderMesh->GetIndices());

				if (collisionDetected) 
					return true;
			}
			else if (object->HasComponent<BoxColliderComponent>())
			{
				bool collisionDetected = BoxPlanetCollisionCheck(collision, triangleNormal, A, B, C);

				if (collisionDetected)
					return true;
			}

			return false;
		}

		static void ResolveTerrainCollision(TerrainCollision& collision)
		{
			RigidBodyComponent* rbcObject;
			RigidBodyComponent rbcPlanet;

			bool objectHasRigidBody = collision.Object->HasComponent<RigidBodyComponent>();
			bool planetHasRigidBody = collision.Planet->HasComponent<RigidBodyComponent>();

			if (objectHasRigidBody)
				rbcObject = &collision.Object->GetComponent<RigidBodyComponent>();

			if (planetHasRigidBody)
				rbcPlanet = collision.Planet->GetComponent<RigidBodyComponent>();

			Ref<Shape> collider;
			if (collision.Object->HasComponent<SphereColliderComponent>())
				collider = collision.Object->GetComponent<SphereColliderComponent>().Collider;
			else if (collision.Object->HasComponent<BoxColliderComponent>())
				collider = collision.Object->GetComponent<BoxColliderComponent>().Collider;

			Vector3 objectCoMWorld = Matrix(collision.Object->GetComponent<TransformComponent>().GetTransform()) * rbcObject->CenterOfMass;

			Matrix objectInvInertiaWorld = collider->GetInvInertiaTensor() * collision.Object->GetComponent<TransformComponent>().GetRotation() * Matrix(collision.Object->GetComponent<TransformComponent>().GetRotation()).Transpose();

			// Elasticity
			const double elasticityObject = objectHasRigidBody ? rbcObject->Elasticity : 0.0;
			const double elasticityPlanet = planetHasRigidBody ? rbcPlanet.Elasticity : 1.0;
			const double elasticity = elasticityObject * elasticityPlanet;

			Vector3 totalImpulse = { 0, 0, 0 };
			Vector3 totalAngularImpulse = { 0, 0, 0 };

			for (const ContactPoint& contact : collision.ContactPoints)
			{
				const Vector3 rObject = contact.PtOnObjectWorldSpace - objectCoMWorld;

				const Vector3 angularJObject = Vector3::Cross(objectInvInertiaWorld * Vector3::Cross(rObject, collision.Normal), rObject);
				const double angularFactor = Vector3::Dot(angularJObject, collision.Normal);

				const Vector3 velObject = rbcObject->LinearVelocity / collision.ContactPoints.size() + Vector3::Cross(rbcObject->AngularVelocity, rObject);

				// In terrain collision we can set the vab to velObject cause the planet isn't really moving in that sense.
				const Vector3 vab = velObject;
				const double impulseJ = (-(1.0 + elasticity) * Vector3::Dot(vab, collision.Normal)) / (rbcObject->InvMass + angularFactor);
				const Vector3 vectorImpulseJ = collision.Normal * impulseJ;

				totalImpulse += vectorImpulseJ;
				totalAngularImpulse += Vector3::Cross(rObject, vectorImpulseJ);
			}

			totalImpulse /= collision.ContactPoints.size();
			totalAngularImpulse /= collision.ContactPoints.size();

			if (collision.Object->HasComponent<SphereColliderComponent>() && totalImpulse.Magnitude() < 5000.0)
			{
				rbcObject->IsStatic = true;
			}
			else if (collision.Object->HasComponent<BoxColliderComponent>() && totalImpulse.Magnitude() < 1500.0 && collision.ContactPoints.size() >= 3)
			{
				rbcObject->IsStatic = true;
			}
			else 
			{
				ApplyLinearImpulse(*rbcObject, totalImpulse);
				ApplyImpulseAngular(*rbcObject, objectInvInertiaWorld, totalAngularImpulse);
			}

			Vector3 objectPos = { collision.Object->GetComponent<TransformComponent>().Translation };

			Vector3 updatedPos = objectPos - collision.Normal * collision.Depth;

			collision.Object->GetComponent<TransformComponent>().Translation = { (float)updatedPos.x, (float)updatedPos.y, (float)updatedPos.z };

			// Friction
			//const double frictionObject = objectHasRigidBody ? rbcObject->Friction : 0.0;
			//const double frictionPlanet = planetHasRigidBody ? rbcPlanet.Friction : 1.0;
			//const double friction = frictionObject * frictionPlanet;

			//const Vector3 velNorm = normal * Vector3::Dot(normal, vab);

			//const Vector3 velTang = vab - velNorm;

			//Vector3 relativeVelTang = velTang;
			//relativeVelTang = Vector3::Normalize(relativeVelTang);

			//const Vector3 inertiaObject = Vector3::Cross(invWorldInertiaObject * Vector3::Cross(ra, relativeVelTang), ra);
			//const double invInertia = Vector3::Dot(inertiaObject, relativeVelTang);

			//const double reducedMass = 1.0 / (rbcObject->InvMass + planetInvMass + invInertia);
			//const Vector3 impulseFriction = velTang * (reducedMass * friction);

			//ApplyImpulse(collision.Object->GetComponent<TransformComponent>(), *rbcObject, collider, { collision.Object->GetComponent<TransformComponent>().Translation }, impulseFriction * 1.0);

			return;
		}

		static Vector3 Gravity(Entity& planet, Entity object, double ts)
		{
			Vector3 impulseGravity = { 0.0, 0.0, 0.0 };

			Vector3 planetPos = { planet.GetComponent<TransformComponent>().Translation };
			Vector3 objectPos = { object.GetComponent<TransformComponent>().Translation };

			auto& pc = planet.GetComponent<PlanetComponent>();

			// Calculate linear velocity due to gravity
			double mass = 1.0 / object.GetComponent<RigidBodyComponent>().InvMass;
			impulseGravity = Vector3::Normalize(planetPos - objectPos) * (double)pc.PlanetData.gravAcc * mass * ts;

			return impulseGravity;
		}

		static bool BroadPhaseCheck(Entity& planet, Entity& object, float dt)
		{
			Vector3 planetPos = planet.GetComponent<TransformComponent>().Translation;
			Vector3 objectPos = object.GetComponent<TransformComponent>().Translation;
			Vector3 objectLinearVel = object.GetComponent<RigidBodyComponent>().LinearVelocity;

			Bounds objectBounds;
			Ref<Shape> collider;
			if (object.HasComponent<SphereColliderComponent>())
				collider = object.GetComponent<SphereColliderComponent>().Collider;

			if (object.HasComponent<BoxColliderComponent>())
				collider = object.GetComponent<BoxColliderComponent>().Collider;

			objectBounds = collider->GetBounds();

			objectBounds = objectBounds * Matrix(object.GetComponent<TransformComponent>().GetTransformWithoutScale());

			auto& pc = planet.GetComponent<PlanetComponent>();
			auto& tcc = planet.GetComponent<TerrainColliderComponent>();

			Bounds planetBounds = tcc.Collider->GetBounds() + planetPos;

			objectBounds.Expand({ objectPos.x + objectLinearVel.x * dt, objectPos.y + objectLinearVel.y * dt, objectPos.z + objectLinearVel.z * dt });

			bool intersects = planetBounds.Intersects(objectBounds);

			return intersects;
		}

		static void Update(entt::registry* registry, Scene* scene, double dt, double slowmotion)
		{
			dt = dt / slowmotion;

			//RendererDebug::SubmitLine(Vector3(-10.0, 10.0, 10.0), Vector3(10.0, 10.0, 10.0), DirectX::XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f));

			auto view = registry->view<TransformComponent, RigidBodyComponent>();

			auto planetView = registry->view<PlanetComponent>();
			if (planetView.size() > 0)
			{
				Entity planetEntity = { planetView[0], scene };
				for (auto entity : view)
				{
					Entity objectEntity = { entity, scene };
					auto [tc, rbc] = view.get<TransformComponent, RigidBodyComponent>(entity);

					Ref<Shape> collider;
					if(objectEntity.HasComponent<SphereColliderComponent>())
						collider = objectEntity.GetComponent<SphereColliderComponent>().Collider;
					if(objectEntity.HasComponent<BoxColliderComponent>())
						collider = objectEntity.GetComponent<BoxColliderComponent>().Collider;

					if (collider != nullptr && !rbc.IsStatic)
					{
						// Update inertia tensor if needed.
						if (collider->GetIsDirty()) 
						{
							Vector3 objectPos = tc.Translation;
							Matrix transform = tc.GetTransform();

							collider->CalculateInertiaTensor(1.0 / rbc.InvMass);

							collider->SetIsDirty(false);
						}

						// Gravity
						if (!rbc.IsStatic) 
						{
							Vector3 impulseGravity = Gravity(planetEntity, objectEntity, dt);
							ApplyLinearImpulse(rbc, impulseGravity);
						}

						// Broad Phase
						if (BroadPhaseCheck(planetEntity, objectEntity, dt))
						{
							// Narrow Phase
							TerrainCollision terrainCollision;
							if (TerrainCollisionCheck(&planetEntity, &objectEntity, terrainCollision, dt))
							{
								bool hasSphereCollider = objectEntity.HasComponent<SphereColliderComponent>();
								bool hasBoxCollider = objectEntity.HasComponent<BoxColliderComponent>();

								if (hasSphereCollider)
									ResolveTerrainCollision(terrainCollision);

								if (hasBoxCollider)
									ResolveTerrainCollision(terrainCollision);
							}
						}

						UpdateBody(objectEntity, dt);;
					}
				}

				// This will be used later for when collision is added between entities. Right now Toast Physics only work with
				// Collision with terrain.
				int numContacts = 0;
				const int maxContacts = view.size() * view.size();
			}

		}
	}
}