#pragma once

#include "Toast/Scene/Components.h"

#include <../vendor/directxtex/include/DirectXTex.h>

#define NOMINMAX
#include <algorithm>

#include <DirectXMath.h>

#define MAX_INT_VALUE	65535.0
#define M_PI			3.14159265358979323846
#define M_PIDIV2		(3.14159265358979323846 / 2.0)

namespace Toast {

	namespace PhysicsEngine {

		struct TerrainCollision
		{
			Entity* Planet;
			Entity* Object;

			Vector3 Normal;

			Vector3 PtOnPlanetWorldSpace;
			Vector3 PtOnObjectWorldSpace;

			double SeparationDistance;
			double TimeOfImpact;
		};

		static int CompareContacts(const void* p1, const void* p2) 
		{
			TerrainCollision a = *(TerrainCollision*)p1;
			TerrainCollision b = *(TerrainCollision*)p2;

			if (a.TimeOfImpact < b.TimeOfImpact)
				return -1;
			
			if (a.TimeOfImpact == b.TimeOfImpact)
				return 0;

			return 1;

		}

		static bool RaySphere(const Vector3& rayStart, const Vector3& rayDir, const Vector3& sphereCenter, const double sphereRadius, double& t1, double& t2)
		{
			const Vector3 m = sphereCenter - rayStart;
			const double a = Vector3::Dot(rayDir, rayDir);
			const double b = Vector3::Dot(m, rayDir);
			const double c = Vector3::Dot(m, m) - sphereRadius * sphereRadius;

			const double delta = b * b - a * c;

			const double invA = 1.0 / a;

			// No real solution exists
			if (delta < 0.0)
				return false;

			const double deltaRoot = sqrt(delta);
			t1 = invA * (b - deltaRoot);
			t2 = invA * (b + deltaRoot);

			return true;
		}

		static bool SpherePlanetDynamic(Entity& planet, Entity& object, Vector3& posObject, Vector3& posPlanet, Vector3& velObject, Vector3& velPlanet, const double planetHeight, const double dt, Vector3& ptOnPlanet, Vector3& ptOnObject, double& timeOfImpact)
		{
			RigidBodyComponent rbcObject = object.GetComponent<RigidBodyComponent>();
			SphereColliderComponent sccObject = object.GetComponent<SphereColliderComponent>();

			const Vector3 relativeVelocity = velObject - velPlanet;

			const Vector3 startPtObject = posObject;
			const Vector3 endPtObject = startPtObject + relativeVelocity * dt;
			const Vector3 rayDir = endPtObject - startPtObject;

			double t0 = 0.0;
			double t1 = 0.0;
			double length = rayDir.Magnitude();
			double lengthSqr = length * length;
			if (lengthSqr < (0.001f * 0.001f))
			{
				// Ray is to short, just check if already intersecting
				Vector3 ab = posPlanet - posObject;
				double abLength = ab.Magnitude();
				double abLengthSqr = abLength * abLength;
				double radius = (double)sccObject.Collider->mRadius + planetHeight;// +0.001;
				if (abLengthSqr > (radius * radius))
					return false;
			}
			else if (!RaySphere(posObject, rayDir, posPlanet, sccObject.Collider->mRadius + planetHeight, t0, t1))
				return false;

			// Change from [0,1] range to [0,dt] range
			t0 *= dt;
			t1 *= dt;

			// If the collision is only the past, then there's no future collision this frame
			if (t1 < 0.0)
				return false;

			// Get the earliest positive time of impact
			timeOfImpact = t0 < 0.0 ? 0.0 : t0;

			// If the earliest collision is to far in the future, then there's no collision this frame
			if (timeOfImpact > dt)
				return false;

			// Get the points on the respective points of collision and return true
			Vector3 newPosObject = posObject + velObject * timeOfImpact;
			Vector3 newPosPlanet = posPlanet + velPlanet * timeOfImpact;
			Vector3 ab = newPosPlanet - newPosObject;
			ab = Vector3::Normalize(ab);
			ptOnObject = newPosObject + ab * sccObject.Collider->mRadius;
			ptOnPlanet = newPosPlanet - ab * planetHeight;

			return true;
		}

		static void ApplyImpulseLinear(RigidBodyComponent& rbc, Vector3 impulse)
		{
			if (rbc.InvMass == 0.0)
				return;

			rbc.LinearVelocity = rbc.LinearVelocity + (impulse * rbc.InvMass);
		}

		static void ApplyImpulseAngular(TransformComponent& tc, RigidBodyComponent& rbc, const Ref<Shape>& shape, Vector3 impulse)
		{
			if (rbc.InvMass == 0.0)
				return;

			Matrix invInertiaTensorWorldSpace = Matrix::Inverse(shape->GetInertiaTensor()) * rbc.InvMass;
			invInertiaTensorWorldSpace = invInertiaTensorWorldSpace * Matrix::FromQuaternion(Quaternion::FromRollPitchYaw(Math::DegreesToRadians(tc.RotationEulerAngles.x), Math::DegreesToRadians(tc.RotationEulerAngles.y), Math::DegreesToRadians(tc.RotationEulerAngles.z))) * Matrix::FromQuaternion({ tc.RotationQuaternion });

			rbc.AngularVelocity = rbc.AngularVelocity + invInertiaTensorWorldSpace * impulse;

			const double maxAngularSpeed = 30.0;

			if (rbc.AngularVelocity.Magnitude() > maxAngularSpeed)
				rbc.AngularVelocity = Vector3::Normalize(rbc.AngularVelocity) * maxAngularSpeed;
		}
		
		static void ApplyImpulse(TransformComponent& tc, RigidBodyComponent& rbc, const Ref<Shape>& shape, Vector3 impulsePoint, Vector3 impulse)
		{
			if (rbc.InvMass == 0.0)
				return;
			
			ApplyImpulseLinear(rbc, impulse);

			Matrix transform = { tc.GetTransform() };
			Vector3 CoM = { rbc.CenterOfMass };

			Vector3 position = transform * CoM;
			Vector3 r = impulsePoint - position;
			Vector3 dL = Vector3::Cross(r, impulse);
			//ApplyImpulseAngular(tc, rbc, shape, dL);
		}

		static TerrainData LoadTerrainData(const char* path, const double maxAltitude, const double minAltitude)
		{
			HRESULT result;

			std::wstring w;
			std::copy(path, path + strlen(path), back_inserter(w));
			const WCHAR* pathWChar = w.c_str();

			DirectX::TexMetadata heightMapMetadata;
			DirectX::ScratchImage* heightMap = new DirectX::ScratchImage();

			result = DirectX::LoadFromWICFile(pathWChar, WIC_FLAGS_NONE, &heightMapMetadata, *heightMap);

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

		static double GetObjectDistanceToPlanet(Entity* planet, Vector3& worldSpaceObjectPos)
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
						return distance;
				}
			}

			return distance;
		}

		static void UpdateBody(Entity& body, float dt)
		{
			Ref<Shape> collider;

			TransformComponent& tc = body.GetComponent<TransformComponent>();
			RigidBodyComponent& rbc = body.GetComponent<RigidBodyComponent>();
			if(body.HasComponent<SphereColliderComponent>())
				collider = body.GetComponent<SphereColliderComponent>().Collider;
			if (body.HasComponent<BoxColliderComponent>())
				collider = body.GetComponent<BoxColliderComponent>().Collider;

			// Update position due to LinearVelocity
			Vector3 translation = { tc.Translation };
			Vector3 deltaGravityTranslation = rbc.LinearVelocity * dt;
			translation += deltaGravityTranslation;
			tc.Translation = { (float)translation.x , (float)translation.y, (float)translation.z };

			// Update rotation due to AngularVelocity
			//if (rbc.AngularVelocity.Magnitude() > 0.0f && collider != nullptr)
			//{
			//	Vector3 CoMPositionWorldSpace = Matrix(tc.GetTransform()) * rbc.CenterOfMass;
			//	Vector3 CoMToPos = translation - CoMPositionWorldSpace;

			//	Matrix orientation = Matrix::FromQuaternion(Quaternion::FromRollPitchYaw(Math::DegreesToRadians(tc.RotationEulerAngles.x), Math::DegreesToRadians(tc.RotationEulerAngles.y), Math::DegreesToRadians(tc.RotationEulerAngles.z))) * Matrix::FromQuaternion({ tc.RotationQuaternion });
			//	Matrix inertiaTensor = orientation * collider->GetInertiaTensor();
			//	Vector3 alpha = Matrix::Inverse(inertiaTensor) * Vector3::Cross(rbc.AngularVelocity, inertiaTensor * rbc.AngularVelocity);
			//	rbc.AngularVelocity = rbc.AngularVelocity + alpha * dt;

			//	// Update orientation due to AngularVelocity
			//	Vector3 dAngle = rbc.AngularVelocity * dt;
			//	Quaternion dq = Quaternion::FromAxisAngle(dAngle, dAngle.Magnitude());
			//	Quaternion finalQuaternion = Quaternion::Normalize(Quaternion(tc.RotationQuaternion) * dq);
			//	tc.RotationQuaternion = { (float)finalQuaternion.x, (float)finalQuaternion.y, (float)finalQuaternion.z, (float)finalQuaternion.w };

			//	// Update position due to CoM translation
			//	Vector3 finalPos = CoMPositionWorldSpace + Vector3::Rotate(CoMToPos, dq);
			//	tc.Translation = { (float)finalPos.x, (float)finalPos.y, (float)finalPos.z };
			//}
		}

		static bool TerrainCollisionCheck(Entity* planet, Entity* object, TerrainCollision& collision, float dt)
		{
			bool hasSphereCollider = object->HasComponent<SphereColliderComponent>();
			bool hasBoxCollider = object->HasComponent<BoxColliderComponent>();

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

			Vector3 planetSpaceObjectPos = Matrix::Inverse(planetTransform) * posObject;

			//planetTransform.ToString();
			//planetSpaceObjectPos.ToString("planetSpaceObjectPos: ");
			
			double objectDistance = GetObjectDistanceToPlanet(planet, posObject);
			double planetHeight = (posObject - posPlanet).Magnitude() - objectDistance;
			//TOAST_CORE_CRITICAL("(posObject - posPlanet).Magnitude(): %lf", (posObject - posPlanet).Magnitude());
			//TOAST_CORE_CRITICAL("planetHeight: %lf", planetHeight);
			//Vector3 terrainPointWorldPos = posPlanet + Vector3::Normalize(posObject - posPlanet) * (planetHeight + planetPC.PlanetData.radius);
			//terrainPointWorldPos.ToString("terrainPointWorldPos: ");
			//Vector3 terrainPointObjectSpacePos = Matrix::Inverse(objectTransform) * terrainPointWorldPos;
			//terrainPointObjectSpacePos.ToString("terrainPointObjectSpacePos: ");

			if (hasSphereCollider)
			{
				double distance = objectDistance - object->GetComponent<SphereColliderComponent>().Collider->mRadius;

				bool collisionDetected = SpherePlanetDynamic(*planet, *object, posObject, posPlanet, rbcObject.LinearVelocity, Vector3(0.0, 0.0, 0.0), planetHeight, dt, collision.PtOnObjectWorldSpace, collision.PtOnPlanetWorldSpace, collision.TimeOfImpact);

				if (collisionDetected)
				{
					//posPlanet.ToString("posPlanet: ");
					//posObject.ToString("posObject: ");
					//TOAST_CORE_CRITICAL("Weird collision planetHeight: %lf, objectDistance: %lf", planetHeight, objectDistance);
					UpdateBody(*object, dt);
					collision.Normal = Vector3::Normalize(posObject - posPlanet);
					collision.SeparationDistance = distance;
					UpdateBody(*object, -dt);

					return true;
				}
			}
			else if (hasBoxCollider) 
			{
				std::vector<Vertex> colliderPts = object->GetComponent<BoxColliderComponent>().ColliderMesh->GetVertices();
				Ref<ShapeBox> collider = object->GetComponent<BoxColliderComponent>().Collider;

				Vector3 velObject = rbcObject.LinearVelocity;
				Vector3 velPlanet = Vector3(0.0, 0.0, 0.0);

				auto bounds = collider->GetBounds(colliderPts, objectTransform);

				const Vector3 relativeVelocity = velObject - velPlanet;

				const Vector3 startMinsObject = bounds.mins;
				const Vector3 startMaxsObject = bounds.maxs;
				const Vector3 endMinsObject = startMinsObject + relativeVelocity * dt;
				const Vector3 endMaxsObject = startMaxsObject + relativeVelocity * dt;

				const Vector3 startPtObject = posObject;
				const Vector3 endPtObject = startPtObject + relativeVelocity * dt;
				const Vector3 rayDir = endPtObject - startPtObject;

			//	DirectX::XMVECTOR planetPt = DirectX::XMVectorAdd(DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(posObject, posPlanet)), planetHeight), posPlanet);

				double endMinsObjectLength = (endMinsObject - posPlanet).Magnitude();
				double endMaxsObjectLength = (endMaxsObject - posPlanet).Magnitude();

				// Determine which bound is closer to planet center
				double distanceToPlanetOrigoMin = endMinsObjectLength < endMaxsObjectLength ? endMinsObjectLength : endMaxsObjectLength;
				double distanceToPlanetOrigoMax = endMinsObjectLength > endMaxsObjectLength ? endMinsObjectLength : endMaxsObjectLength;

				double enterTime = (distanceToPlanetOrigoMin - planetHeight) / relativeVelocity.Magnitude();
				double exitTime = (distanceToPlanetOrigoMax - planetHeight) / relativeVelocity.Magnitude();

			//	//TOAST_CORE_INFO("distanceToPlanetOrigoMin: %f, planetHeight: %f", distanceToPlanetOrigoMin, planetHeight);

				enterTime = enterTime < 0.0f ? 0.0f : enterTime;

				collision.TimeOfImpact = enterTime;
				//collision.Normal = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(posObject, planetPt));
// 				collision.PtOnPlanetWorldSpace = planetPt;
// 				collision.PtOnObjectWorldSpace = planetPt;

				// No collision this frame
				if(enterTime > dt)
					return false;

				if (enterTime > exitTime || enterTime > 1.0f || exitTime < 0.0f) 
					return false;
				else
					return true;
			}

			return false;
		}

		static void ResolveTerrainCollision(TerrainCollision& collision)
		{
			RigidBodyComponent* rbcObject;
			RigidBodyComponent rbcPlanet;
			const Vector3 planetVelocity = { 0.0, 0.0, 0.0 };
			const double planetInvMass = 0.0;

			bool objectHasRigidBody = collision.Object->HasComponent<RigidBodyComponent>();
			bool planetHasRigidBody = collision.Planet->HasComponent<RigidBodyComponent>();

			if (objectHasRigidBody)
				rbcObject = &collision.Object->GetComponent<RigidBodyComponent>();

			if (planetHasRigidBody)
				rbcPlanet = collision.Planet->GetComponent<RigidBodyComponent>();

			Matrix invWorldInertiaObject;
			Ref<Shape> collider;
			if (collision.Object->HasComponent<SphereColliderComponent>())
			{
				collider = collision.Object->GetComponent<SphereColliderComponent>().Collider;
				invWorldInertiaObject = Matrix::Inverse(collider->GetInertiaTensor());
			}
			if (collision.Object->HasComponent<BoxColliderComponent>())
			{
				collider = collision.Object->GetComponent<BoxColliderComponent>().Collider;
				invWorldInertiaObject = Matrix::Inverse(collider->GetInertiaTensor());
			}

			// Elasticity
			const double elasticityObject = objectHasRigidBody ? rbcObject->Elasticity : 0.0;
			const double elasticityPlanet = planetHasRigidBody ? rbcPlanet.Elasticity : 1.0;
			const double elasticity = elasticityObject * elasticityPlanet;

			const Vector3 normal = collision.Normal;
			
			DirectX::XMVECTOR posDx = { 0.0f, 0.0f, 0.0f }, rotDx = { 0.0f, 0.0f, 0.0f }, scaleDx = { 0.0f, 0.0f, 0.0f };
			Vector3 pos, scale;
			Quaternion rot;

			auto tc = collision.Object->GetComponent<TransformComponent>();
			DirectX::XMMatrixDecompose(&scaleDx, &rotDx, &posDx, tc.GetTransform());
			pos = { posDx };
			rot = { rotDx };
			scale = { scaleDx };
			if (collision.Object->HasComponent<BoxColliderComponent>())
				scale = collision.Object->GetComponent<BoxColliderComponent>().Collider->mSize;

			Matrix transform = Matrix::Identity() * Matrix::ScalingFromVector(scale) * Matrix::FromQuaternion(rot) * Matrix::TranslationFromVector(pos);

			const Vector3 ra = collision.PtOnObjectWorldSpace - transform * rbcObject->CenterOfMass;

			const Vector3 angularJObject = Vector3::Cross(invWorldInertiaObject * Vector3::Cross(ra, normal), ra);
			const double angularFactor = Vector3::Dot(angularJObject, normal);

			if (collision.Object->HasComponent<BoxColliderComponent>())
			{
				TOAST_CORE_INFO("Angular factor for the box collider: %lf", angularFactor);
				TOAST_CORE_INFO("collision.PtOnObjectWorldSpace Vector: %lf, %lf, %lf", collision.PtOnObjectWorldSpace.x, collision.PtOnObjectWorldSpace.y, collision.PtOnObjectWorldSpace.z);
				TOAST_CORE_INFO("Center of Mass world space Vector: %lf, %lf, %lf", (transform * rbcObject->CenterOfMass).x, (transform * rbcObject->CenterOfMass).y, (transform * rbcObject->CenterOfMass).z);
				TOAST_CORE_INFO("Collision ra Vector: %lf, %lf, %lf", ra.x, ra.y, ra.z);
				TOAST_CORE_INFO("Collision normal Vector: %lf, %lf, %lf", normal.x, normal.y, normal.z);
				TOAST_CORE_INFO("ra x normal: %lf, %lf, %lf", Vector3::Cross(ra, normal).x, Vector3::Cross(ra, normal).y, Vector3::Cross(ra, normal).z);
				TOAST_CORE_INFO("angularJObject Vector: %lf, %lf, %lf", angularJObject.x, angularJObject.y, angularJObject.z);
			}

			const Vector3 velObject = rbcObject->LinearVelocity + Vector3::Cross(rbcObject->AngularVelocity, ra);

			const Vector3 vab = planetVelocity - velObject;
			const float impulseJ = -(1.0 + elasticity) * Vector3::Dot(vab, collision.Normal) / (rbcObject->InvMass + planetInvMass + angularFactor);
			const Vector3 vectorImpulseJ = normal * impulseJ;

			if (planetHasRigidBody && collider)
				ApplyImpulse(collision.Planet->GetComponent<TransformComponent>(), *rbcObject, collider, { collision.Planet->GetComponent<TransformComponent>().Translation }, vectorImpulseJ * 1.0);

			ApplyImpulse(collision.Object->GetComponent<TransformComponent>(), *rbcObject, collider, { collision.Object->GetComponent<TransformComponent>().Translation }, vectorImpulseJ * -1.0);
		
			// Friction
			const double frictionObject = objectHasRigidBody ? rbcObject->Friction : 0.0;
			const double frictionPlanet = planetHasRigidBody ? rbcPlanet.Friction : 1.0;
			const double friction = frictionObject * frictionPlanet;

			const Vector3 velNorm = normal * Vector3::Dot(normal, vab);

			const Vector3 velTang = vab - velNorm;

			Vector3 relativeVelTang = velTang;
			relativeVelTang = Vector3::Normalize(relativeVelTang);

			const Vector3 inertiaObject = Vector3::Cross(invWorldInertiaObject * Vector3::Cross(ra, relativeVelTang), ra);
			const double invInertia = Vector3::Dot(inertiaObject, relativeVelTang);

			const double reducedMass = 1.0 / (rbcObject->InvMass + planetInvMass + invInertia);
			const Vector3 impulseFriction = velTang * (reducedMass * friction);

			ApplyImpulse(collision.Object->GetComponent<TransformComponent>(), *rbcObject, collider, { collision.Object->GetComponent<TransformComponent>().Translation }, impulseFriction * 1.0);
			
			if (0.0 == collision.TimeOfImpact) 
			{
				const Vector3 ds = collision.PtOnObjectWorldSpace - collision.PtOnPlanetWorldSpace;
				//TOAST_CORE_CRITICAL("Collision ds Vector: %lf, %lf, %lf", ds.x, ds.y, ds.z);

				const double tObject = rbcObject->InvMass / (planetInvMass + rbcObject->InvMass);
				//TOAST_CORE_CRITICAL("tObject: %lf", tObject);
				Vector3 objectPosition = { collision.Object->GetComponent<TransformComponent>().Translation };
				//TOAST_CORE_CRITICAL("Translation BEFORE resolving: %lf, %lf, %lf", collision.Object->GetComponent<TransformComponent>().Translation.x, collision.Object->GetComponent<TransformComponent>().Translation.y, collision.Object->GetComponent<TransformComponent>().Translation.z);
				collision.Object->GetComponent<TransformComponent>().Translation = { (float)(objectPosition + ds * tObject).x, (float)(objectPosition + ds * tObject).y, (float)(objectPosition + ds * tObject).z };
				//TOAST_CORE_CRITICAL("Translation AFTER resolving: %f, %f, %f", collision.Object->GetComponent<TransformComponent>().Translation.x, collision.Object->GetComponent<TransformComponent>().Translation.y, collision.Object->GetComponent<TransformComponent>().Translation.z);
			}

			return;
		}

		static Vector3 Gravity(Entity& planet, Entity object, double ts)
		{
			Vector3 impulseGravity = { 0.0, 0.0, 0.0 };

			Vector3 planetPos = { planet.GetComponent<TransformComponent>().Translation.x, planet.GetComponent<TransformComponent>().Translation.y, planet.GetComponent<TransformComponent>().Translation.z };
			Vector3 objectPos = { object.GetComponent<TransformComponent>().Translation.x, object.GetComponent<TransformComponent>().Translation.y, object.GetComponent<TransformComponent>().Translation.z };

			auto& pc = planet.GetComponent<PlanetComponent>();
			auto ptc = planet.GetComponent<TransformComponent>();
			auto tcc = planet.GetComponent<TerrainColliderComponent>();

			auto tc = object.GetComponent<TransformComponent>();
			auto rbc = object.GetComponent<RigidBodyComponent>();

			// Calculate linear velocity due to gravity
			double mass = 1.0 / (double)rbc.InvMass;
			impulseGravity = Vector3::Normalize(planetPos - objectPos) * (double)pc.PlanetData.gravAcc * mass * ts;

			return impulseGravity;
		}

		static bool BroadPhaseCheck(Entity& planet, Entity& object, float dt)
		{
			Vector3 planetPos = planet.GetComponent<TransformComponent>().Translation;
			Vector3 objectPos = object.GetComponent<TransformComponent>().Translation;
			Quaternion objectRot = object.GetComponent<TransformComponent>().RotationQuaternion;
			Vector3 objectLinearVel = object.GetComponent<RigidBodyComponent>().LinearVelocity;

			Bounds objectBounds;
			if (object.HasComponent<SphereColliderComponent>())
			{
				Ref<ShapeSphere> collider = object.GetComponent<SphereColliderComponent>().Collider;
				objectBounds = collider->GetBounds(objectPos, objectRot);
			}

			if (object.HasComponent<BoxColliderComponent>())
			{
				std::vector<Vertex> colliderPts = object.GetComponent<BoxColliderComponent>().ColliderMesh->GetVertices();
				Ref<ShapeBox> collider = object.GetComponent<BoxColliderComponent>().Collider;
				Matrix objectTransform = { object.GetComponent<TransformComponent>().GetTransform() };
				objectBounds = collider->GetBounds(colliderPts, objectTransform);
			}

			auto& pc = planet.GetComponent<PlanetComponent>();
			auto tcc = planet.GetComponent<TerrainColliderComponent>();

			Bounds planetBounds = tcc.Collider->GetBounds({ planetPos.x + pc.PlanetData.maxAltitude + pc.PlanetData.radius, planetPos.y + pc.PlanetData.maxAltitude + pc.PlanetData.radius, planetPos.z + pc.PlanetData.maxAltitude + pc.PlanetData.radius }, { 0.0f, 0.0f, 0.0f, 0.0f });

			objectBounds.Expand({ objectPos.x + objectLinearVel.x * dt, objectPos.y + objectLinearVel.y * dt, objectPos.z + objectLinearVel.z * dt });

			bool intersects = planetBounds.Intersects(objectBounds);

			return intersects;
		}

		static void Update(entt::registry* registry, Scene* scene, double dt)
		{
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

					if (collider != nullptr)
					{
						// Gravity
						Vector3 impulseGravity = Gravity(planetEntity, objectEntity, dt);
						ApplyImpulse(tc, rbc, collider, { tc.Translation }, impulseGravity);
						//ApplyImpulseLinear(rbc, impulseGravity);
						//TOAST_CORE_INFO("Linear Velocity: %f, %f, %f", rbc.LinearVelocity.x, rbc.LinearVelocity.y, rbc.LinearVelocity.z);
						//TOAST_CORE_INFO("Translation outside UpdateBody(): %f, %f, %f", tc.Translation.x, tc.Translation.y, tc.Translation.z);

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
								{
									//objectEntity.GetComponent<RigidBodyComponent>().LinearVelocity = { 0.0f, 0.0f, 0.0f };
									ResolveTerrainCollision(terrainCollision);
									//TOAST_CORE_CRITICAL("Object translation after resolving the collision: %f, %f, %f", tc.Translation.x, tc.Translation.y, tc.Translation.z);
								}

								if (hasBoxCollider)
								{
									//ResolveTerrainCollision(terrainCollision);
									TOAST_CORE_INFO("COLLISION!!");
									objectEntity.GetComponent<RigidBodyComponent>().LinearVelocity = { 0.0f, 0.0f, 0.0f };
									//objectEntity.GetComponent<RigidBodyComponent>().LinearVelocity = { 0.0f, 0.0f, 0.0f };
								}

							}	
						}

						UpdateBody(objectEntity, dt);

						//TOAST_CORE_CRITICAL("Object translation after Update resolving the collision: %f, %f, %f", tc.Translation.x, tc.Translation.y, tc.Translation.z);
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