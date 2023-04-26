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

			float separationDistance;
		};

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

		static bool TerrainCollisionCheck(Entity* planet, Entity* object, TerrainCollision& collision)
		{
			collision.Planet = planet;
			collision.Object = object;

			TerrainColliderComponent tcc = planet->GetComponent<TerrainColliderComponent>();
			PlanetComponent pc = planet->GetComponent<PlanetComponent>();
			TransformComponent planetTC = planet->GetComponent<TransformComponent>();
			TransformComponent objectTC = object->GetComponent<TransformComponent>();
			SphereColliderComponent scc = object->GetComponent<SphereColliderComponent>();

			DirectX::XMVECTOR planetWorldPos = DirectX::XMLoadFloat3(&planetTC.Translation);
			DirectX::XMVECTOR objectWorldPos = DirectX::XMLoadFloat3(&objectTC.Translation);
			const DirectX::XMVECTOR ab = DirectX::XMVectorSubtract(objectWorldPos, planetWorldPos);
			collision.Normal = DirectX::XMVector3Normalize(ab);

			DirectX::XMVECTOR planetSpaceObjectPosNorm = DirectX::XMVector3Normalize(DirectX::XMVector3Transform(objectWorldPos, DirectX::XMMatrixInverse(nullptr, planetTC.GetTransform())));

			size_t width = std::get<0>(tcc.TerrainData).width;
			size_t height = std::get<0>(tcc.TerrainData).height;

			DirectX::XMFLOAT2 texCoord = DirectX::XMFLOAT2((0.5f + (atan2(DirectX::XMVectorGetZ(planetSpaceObjectPosNorm), DirectX::XMVectorGetX(planetSpaceObjectPosNorm)) / (2.0f * DirectX::XM_PI))), (0.5f - (asin(DirectX::XMVectorGetY(planetSpaceObjectPosNorm)) / DirectX::XM_PI)));
			texCoord.x *= (float)(width - 1);
			texCoord.y *= (float)(height - 1);

			// Calculates the +1 tiles to check and take texture edge into account
			float nextXTile = texCoord.x == (float)(width - 1) ? texCoord.x = 0.0f: texCoord.x + 1.0f;
			float nextYTile = texCoord.y == (float)(height - 1) ? texCoord.y = 0.0f : texCoord.y + 1.0f;

			size_t rowPitch = std::get<1>(tcc.TerrainData)->GetImage(0, 0, 0)->rowPitch;
			const uint16_t* terrainData = reinterpret_cast<const uint16_t*>(std::get<1>(tcc.TerrainData)->GetPixels());
			float Q11 = (float)terrainData[(int)((rowPitch / 2.0f) * floor(texCoord.y) + floor(nextXTile))] / MAX_INT_VALUE;
			float Q12 = (float)terrainData[(int)((rowPitch / 2.0f) * floor(nextYTile) + floor(nextXTile))] / MAX_INT_VALUE;
			float Q21 = (float)terrainData[(int)((rowPitch / 2.0f) * floor(texCoord.y) + floor(texCoord.x))] / MAX_INT_VALUE;
			float Q22 = (float)terrainData[(int)((rowPitch / 2.0f) * floor(nextYTile) + floor(texCoord.x))] / MAX_INT_VALUE;

			float terrainDataValue = Math::BilinearInterpolation(texCoord, Q11, Q12, Q21, Q22);

			float heightAtPos = (terrainDataValue * (pc.PlanetData.maxAltitude - pc.PlanetData.minAltitude) + pc.PlanetData.minAltitude) + pc.PlanetData.radius;
	
			// HeightAtPos is the radius of the planet at contact location
			collision.PtOnPlanetWorldSpace = DirectX::XMVectorAdd(planetWorldPos, DirectX::XMVectorScale(collision.Normal, heightAtPos));
			collision.PtOnObjectWorldSpace = DirectX::XMVectorSubtract(objectWorldPos, DirectX::XMVectorScale(collision.Normal, scc.Radius));

			collision.separationDistance = DirectX::XMVectorGetX(DirectX::XMVector3Length(collision.PtOnPlanetWorldSpace) - DirectX::XMVector3Length(collision.PtOnObjectWorldSpace));

			//TOAST_CORE_INFO("planetOrigo: %f, %f, %f", XMVectorGetX(planetOrigo), XMVectorGetY(planetOrigo), XMVectorGetZ(planetOrigo));
			//TOAST_CORE_INFO("collision.Normal: %f, %f, %f", XMVectorGetX(collision.Normal), XMVectorGetY(collision.Normal), XMVectorGetZ(collision.Normal));
			//TOAST_CORE_INFO("collision.PtOnObjectWorldSpace: %f, %f, %f", XMVectorGetX(collision.PtOnObjectWorldSpace), XMVectorGetY(collision.PtOnObjectWorldSpace), XMVectorGetZ(collision.PtOnObjectWorldSpace));
			//TOAST_CORE_INFO("collision.PtOnPlanetWorldSpace: %f, %f, %f", XMVectorGetX(collision.PtOnPlanetWorldSpace), XMVectorGetY(collision.PtOnPlanetWorldSpace), XMVectorGetZ(collision.PtOnPlanetWorldSpace));
			//TOAST_CORE_INFO("terrainDataValue: %f", terrainDataValue);
			//TOAST_CORE_INFO("pos: %f", pos);
			//TOAST_CORE_INFO("heightAtPos: %f", heightAtPos);

			// HeightAtPos is the radius of the planet at contact location
			const float radiusPlanetObject = scc.Radius + heightAtPos;
			const float lengthSquare = DirectX::XMVectorGetX(DirectX::XMVector3Length(ab)) * DirectX::XMVectorGetX(DirectX::XMVector3Length(ab));
			if (lengthSquare <= (radiusPlanetObject * radiusPlanetObject))
				return true;
			else
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

			const float tPlanet = planetInvMass / (planetInvMass + rbcObject->InvMass);
			const float tObject = rbcObject->InvMass / (planetInvMass + rbcObject->InvMass);

			const DirectX::XMVECTOR ds = collision.PtOnObjectWorldSpace - collision.PtOnPlanetWorldSpace;

			DirectX::XMVECTOR planetPosition = DirectX::XMLoadFloat3(&collision.Planet->GetComponent<TransformComponent>().Translation);
			DirectX::XMVECTOR objectPosition = DirectX::XMLoadFloat3(&collision.Object->GetComponent<TransformComponent>().Translation);

			DirectX::XMStoreFloat3(&collision.Planet->GetComponent<TransformComponent>().Translation, DirectX::XMVectorAdd(planetPosition, (ds * tPlanet)));
			DirectX::XMStoreFloat3(&collision.Object->GetComponent<TransformComponent>().Translation, DirectX::XMVectorSubtract(objectPosition, (ds * tObject)));

			return;
		}

		static DirectX::XMVECTOR Gravity(Entity& planet, Entity object, Scene* scene, Timestep ts)
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
			impulseGravity = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&ptc.Translation) - DirectX::XMLoadFloat3(&tc.Translation)) * (pc.PlanetData.gravAcc / 1000.0f) * mass * ts.GetSeconds() * scene->GetTimeScale();

			return	impulseGravity;
		}

		static void Update(entt::registry* registry, Scene* scene, Timestep ts)
		{
			auto view = registry->view<TransformComponent, RigidBodyComponent>();

			auto planetView = registry->view<PlanetComponent>();
			if (planetView.size() > 0)
			{
				SphereColliderComponent scc;
				Entity planetEntity = { planetView[0], scene };
				for (auto entity : view)
				{
					Entity objectEntity = { entity, scene };
					auto [tc, rbc] = view.get<TransformComponent, RigidBodyComponent>(entity);

					DirectX::XMVECTOR impulseGravity = Gravity(planetEntity, objectEntity, scene, ts);
					//ApplyImpulseLinear(rbc, impulseGravity);
					//TOAST_CORE_INFO("Linear Velocity: %f, %f, %f", rbc.LinearVelocity.x, rbc.LinearVelocity.y, rbc.LinearVelocity.z);

					if (objectEntity.HasComponent<SphereColliderComponent>())
					{
						scc = objectEntity.GetComponent<SphereColliderComponent>();
						// Gravity
						ApplyImpulse(tc, rbc, scc, DirectX::XMLoadFloat3(&tc.Translation), impulseGravity);
						TerrainCollision terrainCollision;

						if (TerrainCollisionCheck(&planetEntity, &objectEntity, terrainCollision))
							//TOAST_CORE_INFO("TERRAIN COLLISION!");
							ResolveTerrainCollision(terrainCollision);

						// Update position due to LinearVelocity
						DirectX::XMVECTOR translation = DirectX::XMLoadFloat3(&tc.Translation);
						DirectX::XMVECTOR deltaGravityTranslation = DirectX::XMLoadFloat3(&rbc.LinearVelocity) * (ts.GetSeconds() * scene->GetTimeScale());
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
								DirectX::XMStoreFloat3(&rbc.AngularVelocity, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&rbc.AngularVelocity), DirectX::XMVectorScale(alpha, ts)));

								// Update orientation due to AngularVelocity
								DirectX::XMVECTOR dAngle = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&rbc.AngularVelocity), ts.GetSeconds());
							DirectX::XMVECTOR dq = DirectX::XMQuaternionRotationAxis(dAngle, DirectX::XMVectorGetX(DirectX::XMVector3Length(dAngle)));
							DirectX::XMStoreFloat4(&tc.RotationQuaternion, DirectX::XMVector4Normalize(DirectX::XMQuaternionMultiply(DirectX::XMLoadFloat4(&tc.RotationQuaternion), dq)));

							// Update position due to CoM translation
							DirectX::XMStoreFloat3(&tc.Translation, DirectX::XMVectorAdd(CoMPositionWorldSpace, DirectX::XMVector3Rotate(CoMToPos, dq)));
						}
					}
				}
			}

		}
	}
}