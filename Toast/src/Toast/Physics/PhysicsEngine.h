#pragma once

#include "Toast/Scene/Components.h"

#include <../vendor/directxtex/include/DirectXTex.h>

#include <math.h>
#include <DirectXMath.h>

#define MAX_INT_VALUE 65535.0f

namespace Toast {

	namespace PhysicsEngine {

		struct TerrainCollision
		{
			Entity* Planet;
			Entity* Object;

			DirectX::XMVECTOR Normal;

			DirectX::XMVECTOR PtOnPlanetWorldSpace;
			DirectX::XMVECTOR PtOnObjectWorldSpace;

			float SeparationDistance;
			float TimeOfImpact;
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

		static bool RaySphere(const DirectX::XMVECTOR& rayStart, const DirectX::XMVECTOR& rayDir, const DirectX::XMVECTOR& sphereCenter, const float sphereRadius, float& t1, float& t2)
		{
			const DirectX::XMVECTOR m = DirectX::XMVectorSubtract(sphereCenter, rayStart);
			const float a = DirectX::XMVectorGetX(DirectX::XMVector3Dot(rayDir, rayDir));
			const float b = DirectX::XMVectorGetX(DirectX::XMVector3Dot(m, rayDir));
			const float c = DirectX::XMVectorGetX(DirectX::XMVector3Dot(m, m)) - sphereRadius * sphereRadius;

			const float delta = b * b - a * c;

			const float invA = 1.0f / a;

			// No real solution exists
			if (delta < 0.0f)
				return false;

			const float deltaRoot = sqrtf(delta);
			t1 = invA * (b - deltaRoot);
			t2 = invA * (b + deltaRoot);

			return true;
		}

		static bool SpherePlanetDynamic(Entity& planet, Entity& object, DirectX::XMVECTOR& posObject, DirectX::XMVECTOR& posPlanet, DirectX::XMVECTOR& velObject, DirectX::XMVECTOR& velPlanet, const float planetHeight, const float dt, DirectX::XMVECTOR& ptOnPlanet, DirectX::XMVECTOR& ptOnObject, float& timeOfImpact)
		{
			RigidBodyComponent rbcObject = object.GetComponent<RigidBodyComponent>();
			SphereColliderComponent sccObject = object.GetComponent<SphereColliderComponent>();

			const DirectX::XMVECTOR relativeVelocity = DirectX::XMVectorSubtract(velObject, velPlanet);

			const DirectX::XMVECTOR startPtObject = posObject;
			const DirectX::XMVECTOR endPtObject = DirectX::XMVectorAdd(startPtObject, DirectX::XMVectorScale(relativeVelocity, dt));
			const DirectX::XMVECTOR rayDir = DirectX::XMVectorSubtract(endPtObject, startPtObject);

			float t0 = 0.0f;
			float t1 = 0.0f;
			float length = DirectX::XMVectorGetX(DirectX::XMVector3Length(rayDir));
			float lengthSqr = length * length;
			if (lengthSqr < (0.001f * 0.001f))
			{
				// Ray is to short, just check if already intersecting
				DirectX::XMVECTOR ab = posPlanet - posObject;
				float abLength = DirectX::XMVectorGetX(DirectX::XMVector3Length(ab));
				float abLengthSqr = abLength * abLength;
				float radius = sccObject.Radius + planetHeight + 0.001f;
				if (abLengthSqr > (radius * radius))
					return false;
			}
			else if (!RaySphere(posObject, rayDir, posPlanet, sccObject.Radius + planetHeight, t0, t1))
				return false;

			// Change from [0,1] range to [0,dt] range
			t0 *= dt;
			t1 *= dt;

			// If the collision is only the past, then there's no future collision this frame
			if (t1 < 0.0f)
				return false;

			// Get the earliest positive time of impact
			timeOfImpact = t0 < 0.0f ? 0.0f : t0;

			// If the earliest collision is to far in the future, then there's no collision this frame
			if (timeOfImpact > dt)
				return false;

			// Get the points on the respective points of collision and return true
			DirectX::XMVECTOR newPosObject = DirectX::XMVectorAdd(posObject, DirectX::XMVectorScale(velObject, timeOfImpact));
			DirectX::XMVECTOR newPosPlanet = DirectX::XMVectorAdd(posPlanet, DirectX::XMVectorScale(velPlanet, timeOfImpact));
			DirectX::XMVECTOR ab = DirectX::XMVectorSubtract(newPosPlanet, newPosObject);
			ab = DirectX::XMVector3Normalize(ab);
			ptOnObject = DirectX::XMVectorAdd(newPosObject, DirectX::XMVectorScale(ab, sccObject.Radius));
			ptOnPlanet = DirectX::XMVectorSubtract(newPosPlanet, DirectX::XMVectorScale(ab, planetHeight));
		}

		static void ApplyImpulseLinear(RigidBodyComponent& rbc, DirectX::XMVECTOR impulse)
		{
			if (rbc.InvMass == 0.0f)
				return;

			DirectX::XMStoreFloat3(&rbc.LinearVelocity, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&rbc.LinearVelocity), DirectX::XMVectorScale(impulse, rbc.InvMass)));
		}

		static void ApplyImpulseAngular(TransformComponent& tc, RigidBodyComponent& rbc, SphereColliderComponent& scc, DirectX::XMVECTOR impulse)
		{
			if (rbc.InvMass == 0.0f)
				return;

			DirectX::XMMATRIX invInertiaTensorWorldSpace = DirectX::XMMatrixInverse(nullptr, DirectX::XMLoadFloat3x3(&scc.InertiaTensor)) * rbc.InvMass;
			invInertiaTensorWorldSpace = DirectX::XMMatrixMultiply(invInertiaTensorWorldSpace, DirectX::XMMatrixRotationQuaternion(DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(tc.RotationEulerAngles.x), DirectX::XMConvertToRadians(tc.RotationEulerAngles.y), DirectX::XMConvertToRadians(tc.RotationEulerAngles.z)))) * DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&tc.RotationQuaternion));

			DirectX::XMStoreFloat3(&rbc.AngularVelocity, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&rbc.AngularVelocity),  DirectX::XMVector3Transform(impulse, invInertiaTensorWorldSpace)));

			const float maxtAngularSpeed = 30.0f;

			if (Math::GetVectorLength(rbc.AngularVelocity) > maxtAngularSpeed)
			{
				DirectX::XMVECTOR angularVelocity = DirectX::XMLoadFloat3(&rbc.AngularVelocity);
				DirectX::XMVector3Normalize(angularVelocity);
				DirectX::XMStoreFloat3(&rbc.AngularVelocity, DirectX::XMVectorScale(angularVelocity, maxtAngularSpeed));
			}
		}

		static void ApplyImpulse(TransformComponent& tc, RigidBodyComponent& rbc, SphereColliderComponent& scc, DirectX::XMVECTOR impulsePoint, DirectX::XMVECTOR impulse)
		{
			if (rbc.InvMass == 0.0f)
				return;

			ApplyImpulseLinear(rbc, impulse);

			DirectX::XMVECTOR position = DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&rbc.CenterOfMass), tc.GetTransform());
			DirectX::XMVECTOR r = DirectX::XMVectorSubtract(impulsePoint, position);
			DirectX::XMVECTOR dL = DirectX::XMVector3Cross(r, impulse);
			ApplyImpulseAngular(tc, rbc, scc, dL);
		}

		static std::tuple<DirectX::TexMetadata, DirectX::ScratchImage*> LoadTerrainData(const char* path)
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

			const uint16_t* terrainData = reinterpret_cast<const uint16_t*>(heightMap->GetPixels());

			//float pixelOneSecondRowValue = (float)terrainData[(int)(heightMap->GetImage(0, 0, 0)->rowPitch / 2.0f) * 50] / MAX_INT_VALUE;

			return std::make_tuple(heightMapMetadata, heightMap);
		}

		static float GetPlanetHeightAtPos(Entity* planet, DirectX::XMVECTOR& pos)
		{
			TerrainColliderComponent tcc = planet->GetComponent<TerrainColliderComponent>();
			TransformComponent planetTC = planet->GetComponent<TransformComponent>();
			PlanetComponent pc = planet->GetComponent<PlanetComponent>();

			DirectX::XMVECTOR planetSpaceObjectPosNorm = DirectX::XMVector3Normalize(DirectX::XMVector3Transform(pos, DirectX::XMMatrixInverse(nullptr, planetTC.GetTransform())));

			size_t width = std::get<0>(tcc.TerrainData).width;
			size_t height = std::get<0>(tcc.TerrainData).height;

			DirectX::XMFLOAT2 texCoord = DirectX::XMFLOAT2((0.5f + (atan2(DirectX::XMVectorGetZ(planetSpaceObjectPosNorm), DirectX::XMVectorGetX(planetSpaceObjectPosNorm)) / (2.0f * DirectX::XM_PI))), (0.5f - (asin(DirectX::XMVectorGetY(planetSpaceObjectPosNorm)) / DirectX::XM_PI)));
			texCoord.x *= (float)(width - 1);
			texCoord.y *= (float)(height - 1);

			// Calculates the +1 tiles to check and take texture edge into account
			float nextXTile = texCoord.x == (float)(width - 1) ? texCoord.x = 0.0f : texCoord.x + 1.0f;
			float nextYTile = texCoord.y == (float)(height - 1) ? texCoord.y = 0.0f : texCoord.y + 1.0f;

			size_t rowPitch = std::get<1>(tcc.TerrainData)->GetImage(0, 0, 0)->rowPitch;
			const uint16_t* terrainData = reinterpret_cast<const uint16_t*>(std::get<1>(tcc.TerrainData)->GetPixels());
			float Q11 = (float)terrainData[(int)((rowPitch / 2.0f) * floor(texCoord.y) + floor(nextXTile))] / MAX_INT_VALUE;
			float Q12 = (float)terrainData[(int)((rowPitch / 2.0f) * floor(nextYTile) + floor(nextXTile))] / MAX_INT_VALUE;
			float Q21 = (float)terrainData[(int)((rowPitch / 2.0f) * floor(texCoord.y) + floor(texCoord.x))] / MAX_INT_VALUE;
			float Q22 = (float)terrainData[(int)((rowPitch / 2.0f) * floor(nextYTile) + floor(texCoord.x))] / MAX_INT_VALUE;

			float terrainDataValue = Math::BilinearInterpolation(texCoord, Q11, Q12, Q21, Q22);

			float heightAtPos = (terrainDataValue * (pc.PlanetData.maxAltitude - pc.PlanetData.minAltitude) + pc.PlanetData.minAltitude) + pc.PlanetData.radius;

			return heightAtPos;
		}

		static void UpdateBody(Entity& body, float dt)
		{
			TransformComponent& tc = body.GetComponent<TransformComponent>();
			RigidBodyComponent& rbc = body.GetComponent<RigidBodyComponent>();
			SphereColliderComponent& scc = body.GetComponent<SphereColliderComponent>();

			// Update position due to LinearVelocity
			DirectX::XMVECTOR translation = DirectX::XMLoadFloat3(&tc.Translation);
			DirectX::XMVECTOR deltaGravityTranslation = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&rbc.LinearVelocity), dt);
			translation += deltaGravityTranslation;
			DirectX::XMStoreFloat3(&tc.Translation, translation);

			// Update rotation due to AngularVelocity
			if (DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&rbc.AngularVelocity))) > 0.0f)
			{
				DirectX::XMVECTOR CoMPositionWorldSpace = DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&rbc.CenterOfMass), tc.GetTransform());
				DirectX::XMVECTOR CoMToPos = DirectX::XMVectorSubtract(translation, CoMPositionWorldSpace);

				DirectX::XMMATRIX orientation = DirectX::XMMatrixMultiply(DirectX::XMMatrixRotationQuaternion(DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(tc.RotationEulerAngles.x), DirectX::XMConvertToRadians(tc.RotationEulerAngles.y), DirectX::XMConvertToRadians(tc.RotationEulerAngles.z))), DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&tc.RotationQuaternion)));
				DirectX::XMMATRIX inertiaTensor = DirectX::XMMatrixMultiply(orientation, DirectX::XMLoadFloat3x3(&scc.InertiaTensor));
				DirectX::XMVECTOR alpha = DirectX::XMVector3Transform(DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&rbc.AngularVelocity), DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&rbc.AngularVelocity), inertiaTensor)), DirectX::XMMatrixInverse(nullptr, inertiaTensor));
				DirectX::XMStoreFloat3(&rbc.AngularVelocity, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&rbc.AngularVelocity), DirectX::XMVectorScale(alpha, dt)));

				// Update orientation due to AngularVelocity
				DirectX::XMVECTOR dAngle = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&rbc.AngularVelocity), dt);
				DirectX::XMVECTOR dq = DirectX::XMQuaternionRotationAxis(dAngle, DirectX::XMVectorGetX(DirectX::XMVector3Length(dAngle)));
				DirectX::XMStoreFloat4(&tc.RotationQuaternion, DirectX::XMVector4Normalize(DirectX::XMQuaternionMultiply(DirectX::XMLoadFloat4(&tc.RotationQuaternion), dq)));

				// Update position due to CoM translation
				DirectX::XMStoreFloat3(&tc.Translation, DirectX::XMVectorAdd(CoMPositionWorldSpace, DirectX::XMVector3Rotate(CoMToPos, dq)));
			}
		}

		static bool TerrainCollisionCheck(Entity* planet, Entity* object, TerrainCollision& collision, float dt)
		{
			collision.Planet = planet;
			collision.Object = object;

			RigidBodyComponent rbcObject = object->GetComponent<RigidBodyComponent>();

			TransformComponent objectTC = object->GetComponent<TransformComponent>();
			float objectSCCRadius = object->GetComponent<SphereColliderComponent>().Radius;

			float planetHeight = GetPlanetHeightAtPos(planet, DirectX::XMLoadFloat3(&objectTC.Translation));

			DirectX::XMVECTOR posPlanet = DirectX::XMLoadFloat3(&planet->GetComponent<TransformComponent>().Translation);
			DirectX::XMVECTOR posObject = DirectX::XMLoadFloat3(&object->GetComponent<TransformComponent>().Translation);

			DirectX::XMVECTOR velObject = DirectX::XMLoadFloat3(&rbcObject.LinearVelocity);
			DirectX::XMVECTOR velPlanet = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);

			if (SpherePlanetDynamic(*planet, *object, posObject, posPlanet, velObject, velPlanet, planetHeight, dt, collision.PtOnObjectWorldSpace, collision.PtOnPlanetWorldSpace, collision.TimeOfImpact))
			{
				UpdateBody(*object, dt);

				collision.Normal = DirectX::XMVectorSubtract(posObject, posPlanet);
				collision.Normal = DirectX::XMVector3Normalize(collision.Normal);

				UpdateBody(*object, -dt);

				DirectX::XMVECTOR ab = DirectX::XMVectorSubtract(posObject, posPlanet);
				float r = DirectX::XMVectorGetX(DirectX::XMVector3Length(ab)) - (objectSCCRadius + planetHeight);
				collision.SeparationDistance = r;
				return true;
			}

			return false;
		}

		static void ResolveTerrainCollision(TerrainCollision& collision)
		{
			RigidBodyComponent* rbcObject;
			RigidBodyComponent rbcPlanet;
			const DirectX::XMVECTOR planetVelocity = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
			const float planetInvMass = 0.0f;

			bool objectHasRigidBody = collision.Object->HasComponent<RigidBodyComponent>();
			bool planetHasRigidBody = collision.Planet->HasComponent<RigidBodyComponent>();

			if (objectHasRigidBody)
				rbcObject = &collision.Object->GetComponent<RigidBodyComponent>();

			if (planetHasRigidBody)
				rbcPlanet = collision.Planet->GetComponent<RigidBodyComponent>();

			// Elasticity
			const float elasticityObject = objectHasRigidBody ? rbcObject->Elasticity : 0.0f;
			const float elasticityPlanet = planetHasRigidBody ? rbcPlanet.Elasticity : 1.0f;
			const float elasticity = elasticityObject * elasticityPlanet;

			const DirectX::XMMATRIX invWorldInertiaObject = DirectX::XMMatrixInverse(nullptr, DirectX::XMLoadFloat3x3(&collision.Object->GetComponent<SphereColliderComponent>().InertiaTensor));

			const DirectX::XMVECTOR normal = collision.Normal;
			
			const DirectX::XMVECTOR ra = DirectX::XMVectorSubtract (collision.PtOnObjectWorldSpace, DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&rbcObject->CenterOfMass), collision.Object->GetComponent<TransformComponent>().GetTransform()));

			const DirectX::XMVECTOR angularJObject = DirectX::XMVector3Cross(DirectX::XMVector3Transform(DirectX::XMVector3Cross(ra, normal), invWorldInertiaObject), ra);
			const float angularFactor = DirectX::XMVectorGetX(DirectX::XMVector3Dot(angularJObject, normal));

			const DirectX::XMVECTOR velObject = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&rbcObject->LinearVelocity), DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&rbcObject->AngularVelocity), ra));

			const DirectX::XMVECTOR vab = DirectX::XMVectorSubtract(planetVelocity, velObject);
			const float impulseJ = -(1.0f + elasticity) * DirectX::XMVectorGetX(DirectX::XMVector3Dot(vab, collision.Normal)) / (rbcObject->InvMass + planetInvMass + angularFactor);
			const DirectX::XMVECTOR vectorImpulseJ = DirectX::XMVectorScale(normal, impulseJ);

			if (planetHasRigidBody)
				ApplyImpulse(collision.Planet->GetComponent<TransformComponent>(), *rbcObject, collision.Object->GetComponent<SphereColliderComponent>(), DirectX::XMLoadFloat3(&collision.Planet->GetComponent<TransformComponent>().Translation), DirectX::XMVectorScale(vectorImpulseJ, 1.0f));

			ApplyImpulse(collision.Object->GetComponent<TransformComponent>(), *rbcObject, collision.Object->GetComponent<SphereColliderComponent>(), DirectX::XMLoadFloat3(&collision.Object->GetComponent<TransformComponent>().Translation), DirectX::XMVectorScale(vectorImpulseJ, -1.0f));
		
			// Friction
			const float frictionObject = objectHasRigidBody ? rbcObject->Friction : 0.0f;
			const float frictionPlanet = planetHasRigidBody ? rbcPlanet.Friction : 1.0f;
			const float friction = frictionObject * frictionPlanet;

			const DirectX::XMVECTOR velNorm = DirectX::XMVectorMultiply(normal, DirectX::XMVector3Dot(normal, vab));

			const DirectX::XMVECTOR velTang = DirectX::XMVectorSubtract(vab, velNorm);

			DirectX::XMVECTOR relativeVelTang = velTang;
			relativeVelTang = DirectX::XMVector3Normalize(relativeVelTang);

			const DirectX::XMVECTOR inertiaObject = DirectX::XMVector3Cross(DirectX::XMVector3Transform(DirectX::XMVector3Cross(ra, relativeVelTang), invWorldInertiaObject), ra);
			const float invInertia = DirectX::XMVectorGetX(DirectX::XMVector3Dot(inertiaObject, relativeVelTang));

			const float reducedMass = 1.0f / (rbcObject->InvMass + planetInvMass + invInertia);
			const DirectX::XMVECTOR impulseFriction = DirectX::XMVectorScale(velTang, (reducedMass * friction));

			ApplyImpulse(collision.Object->GetComponent<TransformComponent>(), *rbcObject, collision.Object->GetComponent<SphereColliderComponent>(), DirectX::XMLoadFloat3(&collision.Object->GetComponent<TransformComponent>().Translation), impulseFriction * 1.0f);

			if (0.0f == collision.TimeOfImpact) 
			{
				const DirectX::XMVECTOR ds = collision.PtOnObjectWorldSpace - collision.PtOnPlanetWorldSpace;

				const float tPlanet = planetInvMass / (planetInvMass + rbcObject->InvMass);
				const float tObject = rbcObject->InvMass / (planetInvMass + rbcObject->InvMass);

				DirectX::XMVECTOR planetPosition = DirectX::XMLoadFloat3(&collision.Planet->GetComponent<TransformComponent>().Translation);
				DirectX::XMVECTOR objectPosition = DirectX::XMLoadFloat3(&collision.Object->GetComponent<TransformComponent>().Translation);

				DirectX::XMStoreFloat3(&collision.Planet->GetComponent<TransformComponent>().Translation, DirectX::XMVectorAdd(planetPosition, (ds * tPlanet)));
				DirectX::XMStoreFloat3(&collision.Object->GetComponent<TransformComponent>().Translation, DirectX::XMVectorSubtract(objectPosition, (ds * tObject)));
			}

			return;
		}

		static DirectX::XMVECTOR Gravity(Entity& planet, Entity object, float ts)
		{
			DirectX::XMVECTOR impulseGravity = { 0.0f, 0.0f, 0.0f, 0.0f };

			//Only one planet can be handled at a time per scene
			auto pc = planet.GetComponent<PlanetComponent>();
			auto ptc = planet.GetComponent<TransformComponent>();
			auto tcc = planet.GetComponent<TerrainColliderComponent>();

			auto tc = object.GetComponent<TransformComponent>();
			auto rbc = object.GetComponent<RigidBodyComponent>();

			bool terrainCollision = false;

			// Calculate linear velocity due to gravity
			float mass = 1.0f / rbc.InvMass;
			impulseGravity = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&ptc.Translation) - DirectX::XMLoadFloat3(&tc.Translation)) * (pc.PlanetData.gravAcc / 1000.0f) * mass * ts;

			return impulseGravity;
		}

		static bool BroadPhaseCheck(Entity& planet, Entity& object, float dt)
		{ 
			DirectX::XMFLOAT3 planetPos = planet.GetComponent<TransformComponent>().Translation;
			DirectX::XMFLOAT3 objectPos = object.GetComponent<TransformComponent>().Translation; 
			DirectX::XMFLOAT4 objectRot = object.GetComponent<TransformComponent>().RotationQuaternion;
			DirectX::XMFLOAT3 objectLinearVel = object.GetComponent<RigidBodyComponent>().LinearVelocity;
			auto pc = planet.GetComponent<PlanetComponent>();

			auto planetBounds = Bounds::GetPlanetBounds(planetPos, pc.PlanetData.maxAltitude, pc.PlanetData.radius); 
			auto objectBounds = Bounds::GetBounds(objectPos, objectRot, object.GetComponent<SphereColliderComponent>().Radius);

			DirectX::XMFLOAT3 planetMins = std::get<0>(planetBounds);
			DirectX::XMFLOAT3 planetMaxs = std::get<1>(planetBounds); 

			DirectX::XMFLOAT3 objectMins = std::get<0>(objectBounds);
			DirectX::XMFLOAT3 objectMaxs = std::get<1>(objectBounds);

			DirectX::XMFLOAT3 expandVector = { objectLinearVel.x * dt,  objectLinearVel.y * dt, objectLinearVel.z * dt };

			objectBounds = Bounds::Expand(objectBounds, expandVector);

			objectMins = std::get<0>(objectBounds);
			objectMaxs = std::get<1>(objectBounds);

			bool intersects = Bounds::Intersects(objectBounds, planetBounds);

			return intersects;
		}

		static void Update(entt::registry* registry, Scene* scene, float dt)
		{
			auto view = registry->view<TransformComponent, RigidBodyComponent, SphereColliderComponent>();

			auto planetView = registry->view<PlanetComponent>();
			if (planetView.size() > 0)
			{
				Entity planetEntity = { planetView[0], scene };
				for (auto entity : view)
				{
					Entity objectEntity = { entity, scene };
					auto [tc, rbc, scc] = view.get<TransformComponent, RigidBodyComponent, SphereColliderComponent>(entity);

					// Gravity
					DirectX::XMVECTOR impulseGravity = Gravity(planetEntity, objectEntity, dt);
					ApplyImpulse(tc, rbc, scc, DirectX::XMLoadFloat3(&tc.Translation), impulseGravity);
					//ApplyImpulseLinear(rbc, impulseGravity);
					//TOAST_CORE_INFO("Linear Velocity: %f, %f, %f", rbc.LinearVelocity.x, rbc.LinearVelocity.y, rbc.LinearVelocity.z);
					//TOAST_CORE_INFO("Translation outside UpdateBody(): %f, %f, %f", tc.Translation.x, tc.Translation.y, tc.Translation.z);

					// Broad Phase
					if (BroadPhaseCheck(planetEntity, objectEntity, dt))
					{
						// Narrow Phase
						TerrainCollision terrainCollision;
						if (TerrainCollisionCheck(&planetEntity, &objectEntity, terrainCollision, dt))
							ResolveTerrainCollision(terrainCollision);
					}

					UpdateBody(objectEntity, dt);
				}

				// This will be used later for when collision is added between entities. Right now Toast Physics only work with
				// Collision with terrain.
				int numContacts = 0;
				const int maxContacts = view.size() * view.size();
			}

		}
	}
}