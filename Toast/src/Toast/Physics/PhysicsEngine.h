#pragma once

#include "Toast/Scene/Components.h"

#include <../vendor/directxtex/include/DirectXTex.h>

#include <algorithm>

#include <DirectXMath.h>

#define MAX_INT_VALUE 65535.0f

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

		static double GetPlanetHeightAtPos(Entity* planet, Vector3& planetSpaceObjectPos)
		{
			TerrainColliderComponent tcc = planet->GetComponent<TerrainColliderComponent>();
			
			PlanetComponent pc = planet->GetComponent<PlanetComponent>();

			size_t width = std::get<0>(tcc.Collider->TerrainData).width;
			size_t height = std::get<0>(tcc.Collider->TerrainData).height;

			float theta = atan2((float)planetSpaceObjectPos.z, (float)planetSpaceObjectPos.x);
			float phi = asin((float)planetSpaceObjectPos.y);

			// Using floats from now on to better match up with the GPU
			// Get texCoord in -1..1 range
			DirectX::XMFLOAT2 texCoord = DirectX::XMFLOAT2(theta / DirectX::XM_PI, phi / DirectX::XM_PIDIV2);

			// texCoord in 0..1 range, to match the ones on the GPU
			texCoord.x = (texCoord.x * 0.5f + 0.5f) * (float)(width - 1);
			texCoord.y = (texCoord.y * 0.5f + 0.5f) * (float)(height - 1);
			//TOAST_CORE_INFO("texCoord: %f, %f", texCoord.x, texCoord.y);

			uint32_t x1 = (uint32_t)(texCoord.x);
			uint32_t y1 = (uint32_t)(texCoord.y);

			uint32_t x2 = x1 == (width - 1) ? 0 : x1 + 1;
			uint32_t y2 = y1 == (height - 1) ? 0 : y1 + 1;

			size_t rowPitch = std::get<1>(tcc.Collider->TerrainData)->GetImage(0, 0, 0)->rowPitch;
			const uint16_t* terrainData = reinterpret_cast<const uint16_t*>(std::get<1>(tcc.Collider->TerrainData)->GetPixels());
			double Q11 = static_cast<double>(terrainData[y1 * (rowPitch / 2) + x1]) / MAX_INT_VALUE;
			double Q12 = static_cast<double>(terrainData[y2 * (rowPitch / 2) + x1]) / MAX_INT_VALUE;
			double Q21 = static_cast<double>(terrainData[y1 * (rowPitch / 2) + x2]) / MAX_INT_VALUE;
			double Q22 = static_cast<double>(terrainData[y2 * (rowPitch / 2) + x2]) / MAX_INT_VALUE;

			double terrainDataValue = Math::BilinearInterpolation(texCoord, Q11, Q12, Q21, Q22);
			//TOAST_CORE_CRITICAL("planetSpaceObjectPos: %lf, %lf, %lf", planetSpaceObjectPos.x, planetSpaceObjectPos.y, planetSpaceObjectPos.z);

			double planetPtAtPos = (terrainDataValue * ((double)pc.PlanetData.maxAltitude - (double)pc.PlanetData.minAltitude) + (double)pc.PlanetData.minAltitude) + (double)pc.PlanetData.radius;
			//TOAST_CORE_CRITICAL("Planet height: %lf", planetPtAtPos);
			return planetPtAtPos;
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
			if (rbc.AngularVelocity.Magnitude() > 0.0f && collider != nullptr)
			{
				Vector3 CoMPositionWorldSpace = Matrix(tc.GetTransform()) * rbc.CenterOfMass;
				Vector3 CoMToPos = translation - CoMPositionWorldSpace;

				Matrix orientation = Matrix::FromQuaternion(Quaternion::FromRollPitchYaw(Math::DegreesToRadians(tc.RotationEulerAngles.x), Math::DegreesToRadians(tc.RotationEulerAngles.y), Math::DegreesToRadians(tc.RotationEulerAngles.z))) * Matrix::FromQuaternion({ tc.RotationQuaternion });
				Matrix inertiaTensor = orientation * collider->GetInertiaTensor();
				Vector3 alpha = Matrix::Inverse(inertiaTensor) * Vector3::Cross(rbc.AngularVelocity, inertiaTensor * rbc.AngularVelocity);
				rbc.AngularVelocity = rbc.AngularVelocity + alpha * dt;

				// Update orientation due to AngularVelocity
				Vector3 dAngle = rbc.AngularVelocity * dt;
				Quaternion dq = Quaternion::FromAxisAngle(dAngle, dAngle.Magnitude());
				Quaternion finalQuaternion = Quaternion::Normalize(Quaternion(tc.RotationQuaternion) * dq);
				tc.RotationQuaternion = { (float)finalQuaternion.x, (float)finalQuaternion.y, (float)finalQuaternion.z, (float)finalQuaternion.w };

				// Update position due to CoM translation
				Vector3 finalPos = CoMPositionWorldSpace + Vector3::Rotate(CoMToPos, dq);
				tc.Translation = { (float)finalPos.x, (float)finalPos.y, (float)finalPos.z };
			}
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
			PlanetComponent planetPC = planet->GetComponent<PlanetComponent>();

			Vector3 posPlanet = { planet->GetComponent<TransformComponent>().Translation };
			Vector3 posObject = { object->GetComponent<TransformComponent>().Translation };

			Matrix planetTransform = { planetTC.GetTransform() };
			Matrix objectTransform = { objectTC.GetTransform() };

			//Matrix identityMatrix = Matrix::Identity();
			//TOAST_CORE_INFO("Identity matrix");
			//identityMatrix.ToString();
			//TOAST_CORE_INFO("inverse Identity matrix");
			//Matrix::Inverse(identityMatrix).ToString();

			//TOAST_CORE_INFO("Original planet matrix");
			//planetTransform.ToString();
			//TOAST_CORE_INFO("Original planet matrix DXMath:");
			//DirectX::XMVECTOR row = planetTC.GetTransform().r[0];
			//TOAST_CORE_INFO("			%f, %f, %f, %f", DirectX::XMVectorGetX(row), DirectX::XMVectorGetY(row), DirectX::XMVectorGetZ(row), DirectX::XMVectorGetW(row));
			//row = planetTC.GetTransform().r[1];
			//TOAST_CORE_INFO("			%f, %f, %f, %f", DirectX::XMVectorGetX(row), DirectX::XMVectorGetY(row), DirectX::XMVectorGetZ(row), DirectX::XMVectorGetW(row));
			//row = planetTC.GetTransform().r[2];
			//TOAST_CORE_INFO("			%f, %f, %f, %f", DirectX::XMVectorGetX(row), DirectX::XMVectorGetY(row), DirectX::XMVectorGetZ(row), DirectX::XMVectorGetW(row));
			//row = planetTC.GetTransform().r[3];
			//TOAST_CORE_INFO("			%f, %f, %f, %f", DirectX::XMVectorGetX(row), DirectX::XMVectorGetY(row), DirectX::XMVectorGetZ(row), DirectX::XMVectorGetW(row));

			//TOAST_CORE_INFO("Original object matrix");
			//objectTransform.ToString();
			//TOAST_CORE_INFO("Original object matrix DXMath:");
			//row = objectTC.GetTransform().r[0];
			//TOAST_CORE_INFO("			%f, %f, %f, %f", DirectX::XMVectorGetX(row), DirectX::XMVectorGetY(row), DirectX::XMVectorGetZ(row), DirectX::XMVectorGetW(row));
			//row = objectTC.GetTransform().r[1];
			//TOAST_CORE_INFO("			%f, %f, %f, %f", DirectX::XMVectorGetX(row), DirectX::XMVectorGetY(row), DirectX::XMVectorGetZ(row), DirectX::XMVectorGetW(row));
			//row = objectTC.GetTransform().r[2];
			//TOAST_CORE_INFO("			%f, %f, %f, %f", DirectX::XMVectorGetX(row), DirectX::XMVectorGetY(row), DirectX::XMVectorGetZ(row), DirectX::XMVectorGetW(row));
			//row = objectTC.GetTransform().r[3];
			//TOAST_CORE_INFO("			%f, %f, %f, %f", DirectX::XMVectorGetX(row), DirectX::XMVectorGetY(row), DirectX::XMVectorGetZ(row), DirectX::XMVectorGetW(row));

			//DirectX::XMMATRIX inverseMatrixTestDXMath = DirectX::XMMatrixInverse(nullptr, planetTC.GetTransform());
			//TOAST_CORE_INFO("Inverse planetTransform:");
			//Matrix::Inverse(planetTransform).ToString();
			//TOAST_CORE_INFO("Inverse planetTransform DXMath:");
			//row = inverseMatrixTestDXMath.r[0];
			//TOAST_CORE_INFO("			%f, %f, %f, %f", DirectX::XMVectorGetX(row), DirectX::XMVectorGetY(row), DirectX::XMVectorGetZ(row), DirectX::XMVectorGetW(row));
			//row = inverseMatrixTestDXMath.r[1];
			//TOAST_CORE_INFO("			%f, %f, %f, %f", DirectX::XMVectorGetX(row), DirectX::XMVectorGetY(row), DirectX::XMVectorGetZ(row), DirectX::XMVectorGetW(row));
			//row = inverseMatrixTestDXMath.r[2];
			//TOAST_CORE_INFO("			%f, %f, %f, %f", DirectX::XMVectorGetX(row), DirectX::XMVectorGetY(row), DirectX::XMVectorGetZ(row), DirectX::XMVectorGetW(row));
			//row = inverseMatrixTestDXMath.r[3];
			//TOAST_CORE_INFO("			%f, %f, %f, %f", DirectX::XMVectorGetX(row), DirectX::XMVectorGetY(row), DirectX::XMVectorGetZ(row), DirectX::XMVectorGetW(row));
			////TOAST_CORE_CRITICAL("posObject: %lf, %lf, %lf, %lf", posObject.x, posObject.y, posObject.z, posObject.w);
			Vector3 planetSpaceObjectPos = Matrix::Inverse(planetTransform) * posObject;

			//DirectX::XMVECTOR planetSpaceObjectPosDXMath = DirectX::XMVector3Transform(DirectX::XMVectorSet(posObject.x, posObject.y, posObject.z, posObject.w), inverseMatrixTestDXMath);

			//TOAST_CORE_CRITICAL("planetSpaceObjectPos: %lf, %lf, %lf, %lf", planetSpaceObjectPos.x, planetSpaceObjectPos.y, planetSpaceObjectPos.z, planetSpaceObjectPos.w);
			//TOAST_CORE_CRITICAL("planetSpaceObjectPosDXMath: %f, %f, %f, %f", DirectX::XMVectorGetX(planetSpaceObjectPosDXMath), DirectX::XMVectorGetY(planetSpaceObjectPosDXMath), DirectX::XMVectorGetZ(planetSpaceObjectPosDXMath), DirectX::XMVectorGetW(planetSpaceObjectPosDXMath));

			double planetHeight = GetPlanetHeightAtPos(planet, Vector3::Normalize(planetSpaceObjectPos));
			//TOAST_CORE_CRITICAL("planetHeight: %lf", planetHeight);
			Vector3 terrainPointWorldPos = posPlanet + Vector3::Normalize(posObject - posPlanet) * planetHeight;
			//TOAST_CORE_INFO("Starship transform");
			//objectTransform.ToString();
			Vector3 terrainPointObjectSpacePos = Matrix::Inverse(objectTransform) * terrainPointWorldPos;
			//TOAST_CORE_INFO("objectTransform:");
			//objectTransform.ToString();

			DirectX::XMMATRIX inverseMatrixDXMath = DirectX::XMMatrixInverse(nullptr, objectTC.GetTransform());
			DirectX::XMVECTOR terrainPointWorldPosDXMath = DirectX::XMVectorSet(terrainPointWorldPos.x, terrainPointWorldPos.y, terrainPointWorldPos.z, terrainPointWorldPos.w);
			DirectX::XMVECTOR terrainPointObjectSpacePosDXMath = DirectX::XMVector3Transform(terrainPointWorldPosDXMath, inverseMatrixDXMath);

			if (hasSphereCollider)
			{
				//TOAST_CORE_INFO("Inverse Starship transform");
				//Matrix::Inverse(objectTransform).ToString();

				//TOAST_CORE_INFO("Inverse Starship transform DXMath:");
				//DirectX::XMVECTOR row = inverseMatrixDXMath.r[0];
				//TOAST_CORE_INFO("			%f, %f, %f, %f", DirectX::XMVectorGetX(row), DirectX::XMVectorGetY(row), DirectX::XMVectorGetZ(row), DirectX::XMVectorGetW(row));
				//row = inverseMatrixDXMath.r[1];
				//TOAST_CORE_INFO("			%f, %f, %f, %f", DirectX::XMVectorGetX(row), DirectX::XMVectorGetY(row), DirectX::XMVectorGetZ(row), DirectX::XMVectorGetW(row));
				//row = inverseMatrixDXMath.r[2];
				//TOAST_CORE_INFO("			%f, %f, %f, %f", DirectX::XMVectorGetX(row), DirectX::XMVectorGetY(row), DirectX::XMVectorGetZ(row), DirectX::XMVectorGetW(row));
				//row = inverseMatrixDXMath.r[3];
				//TOAST_CORE_INFO("			%f, %f, %f, %f", DirectX::XMVectorGetX(row), DirectX::XMVectorGetY(row), DirectX::XMVectorGetZ(row), DirectX::XMVectorGetW(row));
				//TOAST_CORE_CRITICAL("posObject: %lf, %lf, %lf, %lf", posObject.x, posObject.y, posObject.z, posObject.w);

				//TOAST_CORE_INFO("Terrain position in world space: %lf, %lf, %lf, %lf", terrainPointWorldPos.x, terrainPointWorldPos.y, terrainPointWorldPos.z, terrainPointWorldPos.w);
				//TOAST_CORE_INFO("Terrain position in world space DXMath: %f, %f, %f, %f", DirectX::XMVectorGetX(terrainPointWorldPosDXMath), DirectX::XMVectorGetY(terrainPointWorldPosDXMath), DirectX::XMVectorGetZ(terrainPointWorldPosDXMath), DirectX::XMVectorGetW(terrainPointWorldPosDXMath));
				//TOAST_CORE_INFO("Starship position in worldSpace: %lf, %lf, %lf, %lf", posObject.x, posObject.y, posObject.z, posObject.w);
				//TOAST_CORE_INFO("Terrain position in object space: %lf, %lf, %lf, %lf", terrainPointObjectSpacePos.x, terrainPointObjectSpacePos.y, terrainPointObjectSpacePos.z, terrainPointObjectSpacePos.w);
				//TOAST_CORE_INFO("Terrain position in object space DXMath: %f, %f, %f, %f", DirectX::XMVectorGetX(terrainPointObjectSpacePosDXMath), DirectX::XMVectorGetY(terrainPointObjectSpacePosDXMath), DirectX::XMVectorGetZ(terrainPointObjectSpacePosDXMath), DirectX::XMVectorGetW(terrainPointObjectSpacePosDXMath));
				double distance = terrainPointObjectSpacePos.Magnitude() - object->GetComponent<SphereColliderComponent>().Collider->mRadius;
				//double distance = (terrainPointWorldPos - posObject).Magnitude() - object->GetComponent<SphereColliderComponent>().Collider->mRadius;
				//double distance = (posObject - posPlanet).Magnitude() - planetHeight - object->GetComponent<SphereColliderComponent>().Collider->mRadius;

				////TOAST_CORE_INFO("Vector planet position: %f, %f, %f", DirectX::XMVectorGetX(posPlanet), DirectX::XMVectorGetY(posPlanet), DirectX::XMVectorGetZ(posPlanet));
				////TOAST_CORE_INFO("Vector object position: %f, %f, %f", DirectX::XMVectorGetX(posObject), DirectX::XMVectorGetY(posObject), DirectX::XMVectorGetZ(posObject));
				//TOAST_CORE_INFO("Vector length between planet center and object center: %lf", (posObject - posPlanet).Magnitude());
				//TOAST_CORE_INFO("planetHeight: %lf", planetHeight);
				////TOAST_CORE_INFO("object->GetComponent<SphereColliderComponent>().Collider->mRadius: %f", object->GetComponent<SphereColliderComponent>().Collider->mRadius);
				TOAST_CORE_INFO("Distance: %lf", distance);

				if (distance < 0.0) 
				{
					TOAST_CORE_CRITICAL("NEW WAY COLLISION!");
					TOAST_CORE_INFO("Starship position: %lf, %lf, %lf", posObject.x, posObject.y, posObject.z);
					return true;
				}

				//bool temps = SpherePlanetDynamic(*planet, *object, posObject, posPlanet, rbcObject.LinearVelocity, Vector3(0.0, 0.0, 0.0), planetHeight, dt, collision.PtOnObjectWorldSpace, collision.PtOnPlanetWorldSpace, collision.TimeOfImpact);
				//TOAST_CORE_CRITICAL("collision.TimeOfImpact: %f", collision.TimeOfImpact);
				//if(temps)
				//{
				//	UpdateBody(*object, dt);
				//	
				//	TOAST_CORE_INFO("COLLISION!");
				//	TOAST_CORE_INFO("Starship position: %f, %f, %f", posObject.x, posObject.y, posObject.z);
				//	collision.Normal = Vector3::Normalize(posObject - posPlanet);
				//	TOAST_CORE_INFO("collision.Normal: %f, %f, %f", collision.Normal.x, collision.Normal.y, collision.Normal.z);
				//	UpdateBody(*object, -dt);

				//	double objectSCCRadius = object->GetComponent<SphereColliderComponent>().Collider->mRadius;
				//	Vector3 ab = posObject - posPlanet;
				//	double r = ab.Magnitude() - (objectSCCRadius + planetHeight);
				//	collision.SeparationDistance = r;
				//	TOAST_CORE_INFO("collision.SeparationDistance: %f", collision.SeparationDistance);
				//	return true;
				//}
			}
			//else if (hasBoxCollider) 
			//{
			//	std::vector<Vertex> colliderPts = object->GetComponent<BoxColliderComponent>().ColliderMesh->GetVertices();
			//	Ref<ShapeBox> collider = object->GetComponent<BoxColliderComponent>().Collider;

			//	DirectX::XMVECTOR pos = { 0.0f, 0.0f, 0.0f }, rot = { 0.0f, 0.0f, 0.0f }, scale = { 0.0f, 0.0f, 0.0f };

			//	auto tc = object->GetComponent<TransformComponent>();
			//	DirectX::XMMatrixDecompose(&scale, &rot, &pos, tc.GetTransform());
			//	scale = DirectX::XMLoadFloat3(&object->GetComponent<BoxColliderComponent>().Collider->mSize);

			//	DirectX::XMMATRIX transform = DirectX::XMMatrixIdentity() * DirectX::XMMatrixScalingFromVector(scale) * DirectX::XMMatrixRotationQuaternion(rot) * DirectX::XMMatrixTranslationFromVector(pos);

			//	auto bounds = collider->GetBounds(colliderPts, transform);

			//	const DirectX::XMVECTOR relativeVelocity = DirectX::XMVectorSubtract(velObject, velPlanet);

			//	const DirectX::XMVECTOR startMinsObject = DirectX::XMLoadFloat3(&bounds.mins);
			//	const DirectX::XMVECTOR startMaxsObject = DirectX::XMLoadFloat3(&bounds.maxs);
			//	const DirectX::XMVECTOR endMinsObject = DirectX::XMVectorAdd(startMinsObject, DirectX::XMVectorScale(relativeVelocity, dt));
			//	const DirectX::XMVECTOR endMaxsObject = DirectX::XMVectorAdd(startMaxsObject, DirectX::XMVectorScale(relativeVelocity, dt));

			//	const DirectX::XMVECTOR startPtObject = posObject;
			//	const DirectX::XMVECTOR endPtObject = DirectX::XMVectorAdd(startPtObject, DirectX::XMVectorScale(relativeVelocity, dt));
			//	const DirectX::XMVECTOR rayDir = DirectX::XMVectorSubtract(endPtObject, startPtObject);

			//	DirectX::XMVECTOR planetPt = DirectX::XMVectorAdd(DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(posObject, posPlanet)), planetHeight), posPlanet);

			//	float endMinsObjectLength = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(endMinsObject, posPlanet)));
			//	float endMaxsObjectLength = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(endMaxsObject, posPlanet)));

			//	// Determine which bound is closer to planet center
			//	float distanceToPlanetOrigoMin = endMinsObjectLength < endMaxsObjectLength ? endMinsObjectLength : endMaxsObjectLength;
			//	float distanceToPlanetOrigoMax = endMinsObjectLength > endMaxsObjectLength ? endMinsObjectLength : endMaxsObjectLength;

			//	float enterTime = (distanceToPlanetOrigoMin - planetHeight) / DirectX::XMVectorGetX(DirectX::XMVector3Length(relativeVelocity));
			//	float exitTime = (distanceToPlanetOrigoMax - planetHeight) / DirectX::XMVectorGetX(DirectX::XMVector3Length(relativeVelocity));

			//	//TOAST_CORE_INFO("distanceToPlanetOrigoMin: %f, planetHeight: %f", distanceToPlanetOrigoMin, planetHeight);

			//	DirectX::XMVECTOR endPtObjecFinals = DirectX::XMVectorAdd(startPtObject, DirectX::XMVectorScale(relativeVelocity, enterTime));

			//	enterTime = enterTime < 0.0f ? 0.0f : enterTime;

			//	collision.TimeOfImpact = enterTime;
			//	collision.Normal = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(posObject, planetPt));
			//	collision.PtOnPlanetWorldSpace = planetPt;
			//	collision.PtOnObjectWorldSpace = planetPt;

			//	// No collision this frame
			//	if(enterTime > dt)
			//		return false;

			//	if (enterTime > exitTime || enterTime > 1.0f || exitTime < 0.0f) 
			//		return false;
			//	else
			//		return true;
			//}

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
				TOAST_CORE_CRITICAL("Collision ds Vector: %lf, %lf, %lf", ds.x, ds.y, ds.z);

				const double tObject = rbcObject->InvMass / (planetInvMass + rbcObject->InvMass);
				TOAST_CORE_CRITICAL("tObject: %lf", tObject);
				Vector3 objectPosition = { collision.Object->GetComponent<TransformComponent>().Translation };
				TOAST_CORE_CRITICAL("Translation BEFORE resolving: %lf, %lf, %lf", collision.Object->GetComponent<TransformComponent>().Translation.x, collision.Object->GetComponent<TransformComponent>().Translation.y, collision.Object->GetComponent<TransformComponent>().Translation.z);
				collision.Object->GetComponent<TransformComponent>().Translation = { (float)(objectPosition + ds * tObject).x, (float)(objectPosition + ds * tObject).y, (float)(objectPosition + ds * tObject).z };
				TOAST_CORE_CRITICAL("Translation AFTER resolving: %f, %f, %f", collision.Object->GetComponent<TransformComponent>().Translation.x, collision.Object->GetComponent<TransformComponent>().Translation.y, collision.Object->GetComponent<TransformComponent>().Translation.z);
			}

			return;
		}

		static Vector3 Gravity(Entity& planet, Entity object, double ts)
		{
			Vector3 impulseGravity = { 0.0, 0.0, 0.0 };

			Vector3 planetPos = { planet.GetComponent<TransformComponent>().Translation.x, planet.GetComponent<TransformComponent>().Translation.y, planet.GetComponent<TransformComponent>().Translation.z };
			Vector3 objectPos = { object.GetComponent<TransformComponent>().Translation.x, object.GetComponent<TransformComponent>().Translation.y, object.GetComponent<TransformComponent>().Translation.z };

			auto pc = planet.GetComponent<PlanetComponent>();
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

			auto pc = planet.GetComponent<PlanetComponent>();
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
									//ResolveTerrainCollision(terrainCollision);
									objectEntity.GetComponent<RigidBodyComponent>().LinearVelocity = { 0.0f, 0.0f, 0.0f };
								}

								if (hasBoxCollider)
								{
									//ResolveTerrainCollision(terrainCollision);
									//TOAST_CORE_INFO("COLLISION!!");
									//objectEntity.GetComponent<RigidBodyComponent>().LinearVelocity = { 0.0f, 0.0f, 0.0f };
								}

							}	
						}

						UpdateBody(objectEntity, dt);
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