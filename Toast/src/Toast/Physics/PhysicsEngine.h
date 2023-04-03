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

			DirectX::XMStoreFloat3(&rbc.LinearVelocity, (DirectX::XMLoadFloat3(&rbc.LinearVelocity) + impulse * rbc.InvMass));
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

		static bool TerrainCollisionCheck(Entity* planet, Entity* object, DirectX::XMVECTOR objectWorldPos, TerrainCollision& collision)
		{
			collision.Planet = planet;
			collision.Object = object;

			TerrainColliderComponent tcc = planet->GetComponent<TerrainColliderComponent>();
			PlanetComponent pc = planet->GetComponent<PlanetComponent>();
			TransformComponent planetTC = planet->GetComponent<TransformComponent>();
			TransformComponent objectTC = object->GetComponent<TransformComponent>();
			SphereColliderComponent scc = object->GetComponent<SphereColliderComponent>();

			DirectX::XMVECTOR planetOrigo = DirectX::XMVectorSet(planetTC.Translation.x, planetTC.Translation.y, planetTC.Translation.z, 1.0f);
			collision.Normal = DirectX::XMVector3Normalize(objectWorldPos - planetOrigo);

			//DirectX::XMVECTOR planetSpaceObjectPosNorm = DirectX::XMVector3Normalize(objectWorldPos);
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

			collision.PtOnPlanetWorldSpace = planetOrigo + collision.Normal * heightAtPos;
			collision.PtOnObjectWorldSpace = objectWorldPos - collision.Normal * scc.Radius;

			// HeightAtPos is the radius of the planet at contact location
			const float radiusPlanetObject = scc.Radius + heightAtPos; 

			const DirectX::XMVECTOR planetObjectVector = DirectX::XMLoadFloat3(&planetTC.Translation) - DirectX::XMLoadFloat3(&objectTC.Translation);
			const float lengthSqr = DirectX::XMVectorGetX(DirectX::XMVector3Length(planetObjectVector) * DirectX::XMVector3Length(planetObjectVector));

			collision.separationDistance = DirectX::XMVectorGetX(DirectX::XMVector3Length(collision.PtOnPlanetWorldSpace) - DirectX::XMVector3Length(collision.PtOnObjectWorldSpace));

			//TOAST_CORE_INFO("planetOrigo: %f, %f, %f", XMVectorGetX(planetOrigo), XMVectorGetY(planetOrigo), XMVectorGetZ(planetOrigo));
			//TOAST_CORE_INFO("collision.Normal: %f, %f, %f", XMVectorGetX(collision.Normal), XMVectorGetY(collision.Normal), XMVectorGetZ(collision.Normal));
			//TOAST_CORE_INFO("collision.PtOnObjectWorldSpace: %f, %f, %f", XMVectorGetX(collision.PtOnObjectWorldSpace), XMVectorGetY(collision.PtOnObjectWorldSpace), XMVectorGetZ(collision.PtOnObjectWorldSpace));
			//TOAST_CORE_INFO("collision.PtOnPlanetWorldSpace: %f, %f, %f", XMVectorGetX(collision.PtOnPlanetWorldSpace), XMVectorGetY(collision.PtOnPlanetWorldSpace), XMVectorGetZ(collision.PtOnPlanetWorldSpace));
			//TOAST_CORE_INFO("terrainDataValue: %f", terrainDataValue);
			//TOAST_CORE_INFO("heightAtPos: %f", heightAtPos);
			//TOAST_CORE_INFO("collision.separationDistance: %f", collision.separationDistance);

			if (lengthSqr <= (radiusPlanetObject * radiusPlanetObject))
				return true;
			else
				return false;
		}

		static void ResolveTerrainCollision(TerrainCollision& collision)
		{
			if (collision.Object->HasComponent<RigidBodyComponent>())
			{
				if (collision.Planet->HasComponent<RigidBodyComponent>())
					collision.Planet->GetComponent<RigidBodyComponent>().LinearVelocity = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);

				collision.Object->GetComponent<RigidBodyComponent>().LinearVelocity = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
			}
			else
				TOAST_CORE_WARN("Object colliding with the planet is lacking a RigidBodyComponent");

			DirectX::XMVECTOR resolveVector = collision.Normal * collision.separationDistance;
			DirectX::XMVECTOR objectTranslation = DirectX::XMLoadFloat3(&collision.Object->GetComponent<TransformComponent>().Translation);

			DirectX::XMFLOAT3 resolvedTranslation;
			DirectX::XMStoreFloat3(&resolvedTranslation, (objectTranslation - resolveVector));

			collision.Object->GetComponent<TransformComponent>().Translation = resolvedTranslation;

			return;
		}
	}
}