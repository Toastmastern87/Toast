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

			float pixelOneSecondRowValue = (float)terrainData[(int)(heightMap->GetImage(0, 0, 0)->rowPitch / 2.0f) * 50] / MAX_INT_VALUE;
			TOAST_CORE_INFO("First pixel second row value: %f", pixelOneSecondRowValue);

			return std::make_tuple(heightMapMetadata, heightMap);
		}

		static bool TerrainCollisionCheck(Entity* planet, Entity* object, DirectX::XMVECTOR objectWorldPos, TerrainCollision& collision)
		{
			collision.Planet = planet;
			collision.Object = object;

			TerrainColliderComponent tcc = planet->GetComponent<TerrainColliderComponent>();
			PlanetComponent pc = planet->GetComponent<PlanetComponent>();
			SphereColliderComponent scc = object->GetComponent<SphereColliderComponent>();

			//Planet always at origo
			DirectX::XMVECTOR planetOrigo = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
			collision.Normal = DirectX::XMVector3Normalize(objectWorldPos - planetOrigo);

			DirectX::XMVECTOR worldPosNorm = DirectX::XMVector3Normalize(objectWorldPos);

			size_t width = std::get<0>(tcc.TerrainData).width;
			size_t height = std::get<0>(tcc.TerrainData).height;
	
			DirectX::XMFLOAT2 texCoord = DirectX::XMFLOAT2((0.5f + (atan2(DirectX::XMVectorGetZ(worldPosNorm), DirectX::XMVectorGetX(worldPosNorm)) / (2.0f * DirectX::XM_PI))), (0.5f - (asin(DirectX::XMVectorGetY(worldPosNorm)) / DirectX::XM_PI)));
			texCoord.x *= (float)width;
			texCoord.y *= (float)height;

			size_t rowPitch = std::get<1>(tcc.TerrainData)->GetImage(0, 0, 0)->rowPitch;
			const uint16_t* terrainData = reinterpret_cast<const uint16_t*>(std::get<1>(tcc.TerrainData)->GetPixels());
			float Q11 = (float)terrainData[(int)((rowPitch / 2.0f) * floor(texCoord.y + 1.0f) + floor(texCoord.x))] / MAX_INT_VALUE;
			float Q12 = (float)terrainData[(int)((rowPitch / 2.0f) * floor(texCoord.y) + floor(texCoord.x))] / MAX_INT_VALUE;
			float Q21 = (float)terrainData[(int)((rowPitch / 2.0f) * floor(texCoord.y + 1.0f) + floor(texCoord.x + 1.0f))] / MAX_INT_VALUE;
			float Q22 = (float)terrainData[(int)((rowPitch / 2.0f) * floor(texCoord.y) + floor(texCoord.x + 1.0f))] / MAX_INT_VALUE;

			float terrainDataValue = Math::BilinearInterpolation(texCoord, Q11, Q12, Q21, Q22);

			float heightAtPos = (terrainDataValue * (pc.PlanetData.maxAltitude - pc.PlanetData.minAltitude) + pc.PlanetData.minAltitude) + pc.PlanetData.radius;

			collision.PtOnPlanetWorldSpace = planetOrigo + collision.Normal * heightAtPos;
			collision.PtOnObjectWorldSpace = objectWorldPos - collision.Normal * scc.Radius;

			collision.separationDistance = DirectX::XMVectorGetX(DirectX::XMVector3Length(collision.PtOnObjectWorldSpace)) - DirectX::XMVectorGetX(DirectX::XMVector3Length(collision.PtOnPlanetWorldSpace));
			

			if (collision.separationDistance <= 0.0f)
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

			DirectX::XMVECTOR resolveVector = -collision.Normal * collision.separationDistance;

			collision.Object->GetComponent<TransformComponent>().Transform = DirectX::XMMatrixMultiply(collision.Object->GetComponent<TransformComponent>().Transform, XMMatrixTranslationFromVector(resolveVector));	

			return;
		}
	}
}