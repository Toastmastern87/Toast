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

			const float elasticityObject = objectHasRigidBody ? rbcObject->Elasticity : 0.0f;
			const float elasticityPlanet = planetHasRigidBody ? rbcPlanet.Elasticity : 1.0f;
			const float elasticity = elasticityObject * elasticityPlanet;

			const DirectX::XMVECTOR vab = planetVelocity - DirectX::XMLoadFloat3(&rbcObject->LinearVelocity);
			const float impulseJ = -(1.0f + elasticity) * DirectX::XMVectorGetX(DirectX::XMVector3Dot(vab, collision.Normal)) / (rbcObject->InvMass + planetInvMass);

			if (planetHasRigidBody)
				ApplyImpulseLinear(rbcPlanet, DirectX::XMVectorMultiply(collision.Normal, { impulseJ * 1.0f, impulseJ * 1.0f, impulseJ * 1.0f }));

			ApplyImpulseLinear(*rbcObject, DirectX::XMVectorMultiply(collision.Normal, { impulseJ * -1.0f, impulseJ * -1.0f, impulseJ * -1.0f }));
		
			const float tPlanet = planetInvMass / (planetInvMass + rbcObject->InvMass);
			const float tObject = rbcObject->InvMass / (planetInvMass + rbcObject->InvMass);

			const DirectX::XMVECTOR ds = collision.PtOnObjectWorldSpace - collision.PtOnPlanetWorldSpace;

			DirectX::XMVECTOR planetPosition = DirectX::XMLoadFloat3(&collision.Planet->GetComponent<TransformComponent>().Translation);
			DirectX::XMVECTOR objectPosition = DirectX::XMLoadFloat3(&collision.Object->GetComponent<TransformComponent>().Translation);

			DirectX::XMStoreFloat3(&collision.Planet->GetComponent<TransformComponent>().Translation, DirectX::XMVectorAdd(planetPosition, (ds * tPlanet)));
			DirectX::XMStoreFloat3(&collision.Object->GetComponent<TransformComponent>().Translation, DirectX::XMVectorSubtract(objectPosition, (ds * tObject)));

			return;
		}
	}
}