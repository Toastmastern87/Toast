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

	namespace PhysicsEngine 
	{

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

		struct Ray {
			Vector3 Origin;
			Vector3 Direction; // should be normalized
		};

		// Test ray against bounding box
		static bool RayIntersectsBounds(const Ray& ray, const Bounds& bounds)
		{
			// Unpack for clarity
			const Vector3& O = ray.Origin;
			const Vector3& D = ray.Direction;

			// For each axis (x, y, z), compute intersection t-values
			// When direction is 0 along an axis, handle separately.
			double tmin = -DBL_MAX;
			double tmax = DBL_MAX;

			// Lambda to handle one dimension
			auto checkAxis = [&](double start, double dir, double minVal, double maxVal) {
				// If dir is zero, ray is parallel to that axis
				if (fabs(dir) < 1e-12) {
					// If origin not in slab range, no hit
					if (start < minVal || start > maxVal) {
						return false;
					}
					return true; // No update to t needed since parallel
				}
				else {
					double t1 = (minVal - start) / dir;
					double t2 = (maxVal - start) / dir;

					if (t1 > t2) std::swap(t1, t2);

					// Update global tmin/tmax
					if (t1 > tmin) tmin = t1;
					if (t2 < tmax) tmax = t2;
					if (tmin > tmax) return false;
				}
				return true;
				};

			// Check X-axis
			if (!checkAxis(O.x, D.x, bounds.mins.x, bounds.maxs.x)) return false;
			// Check Y-axis
			if (!checkAxis(O.y, D.y, bounds.mins.y, bounds.maxs.y)) return false;
			// Check Z-axis
			if (!checkAxis(O.z, D.z, bounds.mins.z, bounds.maxs.z)) return false;

			// If we get here, tmin <= tmax, so there's an intersection
			// Intersection occurs at t >= 0 (forward in ray direction)
			return tmax >= 0.0;
		}

		// Test ray against a single triangle
		static bool RayIntersectsTriangle(const Vector3& origin, const Vector3& dir,
			const Vector3& A, const Vector3& B, const Vector3& C,
			double& tOut)
		{
			const double EPSILON = 1e-12;
			Vector3 AB = B - A;
			Vector3 AC = C - A;
			Vector3 pvec = Vector3::Cross(dir, AC);
			double det = Vector3::Dot(AB, pvec);

			// If det is near zero, the ray lies in plane of triangle or is parallel
			if (fabs(det) < EPSILON) return false;

			double invDet = 1.0 / det;

			Vector3 tvec = origin - A;
			double u = Vector3::Dot(tvec, pvec) * invDet;
			if (u < 0.0 || u > 1.0) return false;

			Vector3 qvec = Vector3::Cross(tvec, AB);
			double v = Vector3::Dot(dir, qvec) * invDet;
			if (v < 0.0 || u + v > 1.0) return false;

			double t = Vector3::Dot(AC, qvec) * invDet;
			if (t < EPSILON) return false; // Intersection behind the ray origin or too close

			tOut = t;
			return true;
		}

		// Recursively ray cast through the node hierarchy
		static bool RaycastPlanetNode(const Ray& ray, Ref<PlanetNode>& node, double& closestT, Vector3& hitPoint) {
			// Broad phase: check bounding box intersection
			if (!RayIntersectsBounds(ray, node->NodeBounds))
				return false;

			bool hitFound = false;

			if (!node->ChildNodes.empty()) {
				// Not a leaf, go deeper
				for (auto& child : node->ChildNodes) {
					double tChild;
					Vector3 hpChild;
					if (RaycastPlanetNode(ray, child, tChild, hpChild)) {
						if (!hitFound || tChild < closestT) {
							hitFound = true;
							closestT = tChild;
							hitPoint = hpChild;
						}
					}
				}
			}
			else {
				// Leaf node: test ray against node's triangle
				Vector3 Apos = node->A.Position;
				Vector3 Bpos = node->B.Position;
				Vector3 Cpos = node->C.Position;

				double t;
				if (RayIntersectsTriangle(ray.Origin, ray.Direction, Apos, Bpos, Cpos, t)) {
					Vector3 currentHit = ray.Origin + ray.Direction * t;
					if (!hitFound || t < closestT) {
						hitFound = true;
						closestT = t;
						hitPoint = currentHit;
					}
				}
			}

			return hitFound;
		}

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

		static std::pair<double, double> ProjectShapeOntoAxis(const std::vector<Vector3>& vertices, const Vector3& axis) 
		{
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

		static std::tuple<bool, double> OverlapOnAxis(const std::vector<Vector3>& obbVertices, const std::vector<Vector3>& triangleVertices, const Vector3& axis) {
			auto [obbMin, obbMax] = ProjectShapeOntoAxis(obbVertices, axis);
			auto [triMin, triMax] = ProjectShapeOntoAxis(triangleVertices, axis);

			if (obbMin >= triMax || triMin >= obbMax)
				return { false, 0.0 };

			double penetrationDepth = (std::min)(triMax - obbMin, obbMax - triMin);

			return { true, penetrationDepth };
		}

		static void FindContactPointsOBB(const std::vector<Vector3>& colliderPts, const std::vector<Vector3>& terrainPts, TerrainCollision& collision)
		{
			TOAST_PROFILE_FUNCTION();

			Vector3 p1 = terrainPts[0];
			Vector3 p2 = terrainPts[1];
			Vector3 p3 = terrainPts[2];

			// Calculate vectors lying on the plane
			Vector3 v1 = p2 - p1;
			Vector3 v2 = p3 - p1;

			// Calculate the normal vector to the plane
			Vector3 planeNormal = Vector3::Normalize(Vector3::Cross(v1, v2));

			if (Vector3::Dot(planeNormal, collision.Normal) < 0)
				planeNormal = -planeNormal;

			// Plane coefficients
			double d = -Vector3::Dot(planeNormal, p1);

			for (const auto& p : colliderPts) 
			{
				double f = Vector3::Dot(planeNormal, p) + d;

				if (f < 0.0)
				{
					double denominator = Vector3::Dot(planeNormal, collision.Normal);
					if (std::abs(denominator) > 1e-6)
					{ 
						double t = -f / denominator;
						if (t >= 0.0)
						{
							// Calculate the intersection point
							Vector3 ptOnPlanet = p + collision.Normal * t;
							ContactPoint newContact = { ptOnPlanet, p };

							bool addNewContact = true;

							for (const auto& pt : collision.ContactPoints)
							{
								double distance = (pt.PtOnObjectWorldSpace - newContact.PtOnObjectWorldSpace).Length();

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
		}

		static bool BoxPlanetCollisionCheck(TerrainCollision& collision, const std::vector<Vector3>& terrainColliderVertices)
		{
			TOAST_PROFILE_FUNCTION();

			collision.Depth = DBL_MAX;
			collision.Normal = Vector3(0.0, 0.0, 0.0);

			bool collisionDetected = false;

			auto& planetPos = collision.Planet->GetComponent<TransformComponent>().Translation;
			std::vector<Vector3> axes, objectColliderPts, terrainPts;
			auto& rotEuler = collision.Object->GetComponent<TransformComponent>().RotationEulerAngles;
			auto& rotQuat = collision.Object->GetComponent<TransformComponent>().RotationQuaternion;
			auto& scale = collision.Object->GetComponent<BoxColliderComponent>().Collider->mSize;
			auto& objectPos = collision.Object->GetComponent<TransformComponent>().Translation;
			std::vector<Vertex> objectColliderVertices = collision.Object->GetComponent<BoxColliderComponent>().ColliderMesh->GetVertices();

			Matrix objTransform = DirectX::XMMatrixIdentity() * DirectX::XMMatrixScaling(scale.x, scale.y, scale.z)
				* (DirectX::XMMatrixRotationQuaternion(DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(rotEuler.x), DirectX::XMConvertToRadians(rotEuler.y), DirectX::XMConvertToRadians(rotEuler.z)))) * DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&rotQuat))
				* DirectX::XMMatrixTranslation(objectPos.x, objectPos.y, objectPos.z);

			for (auto& vertex : objectColliderVertices)
			{
				objectColliderPts.emplace_back(vertex.Position);
				objectColliderPts.at(objectColliderPts.size() - 1) = objTransform * objectColliderPts.at(objectColliderPts.size() - 1);
			}

			Matrix colliderRot = { DirectX::XMMatrixIdentity() * (DirectX::XMMatrixRotationQuaternion(DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(rotEuler.x), DirectX::XMConvertToRadians(rotEuler.y), DirectX::XMConvertToRadians(rotEuler.z)))) * DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&collision.Object->GetComponent<TransformComponent>().RotationQuaternion)) };

			Vector3 obbAxes[3] = {
				colliderRot * Vector3(1.0, 0.0, 0.0),  // Local X-axis
				colliderRot * Vector3(0.0, 1.0, 0.0),  // Local Y-axis
				colliderRot * Vector3(0.0, 0.0, 1.0)   // Local Z-axis
			};

			for (size_t i = 0; i < terrainColliderVertices.size() - 2; i += 3)
			{
				terrainPts.clear();
				axes.clear();

				terrainPts.emplace_back(terrainColliderVertices[i]);
				terrainPts.emplace_back(terrainColliderVertices[i + 1]);
				terrainPts.emplace_back(terrainColliderVertices[i + 2]);


				Vector3 triangleNormal = Vector3::Normalize(Vector3::Cross(terrainPts[1] - terrainPts[0], terrainPts[2] - terrainPts[0]));

				if (triangleNormal.LengthSqrt() == 0)
					continue;

				axes.emplace_back(triangleNormal);
				axes.insert(axes.end(), std::begin(obbAxes), std::end(obbAxes));

				Vector3 triEdges[3] = {
					terrainPts.at(1) - terrainPts.at(0),
					terrainPts.at(2) - terrainPts.at(1),
					terrainPts.at(0) - terrainPts.at(2)
				};

				for (const auto& obbAxis : obbAxes)
				{
					for (const auto& triEdge : triEdges)
					{
						Vector3 crossProduct = Vector3::Cross(obbAxis, triEdge);
						if (crossProduct.LengthSqrt() < 1e-6)
							continue;

						crossProduct = Vector3::Normalize(crossProduct);
						axes.emplace_back(crossProduct);
					}
				}

				bool separatingAxisFound = false;
				double minPenetration = DBL_MAX;
				Vector3 collisionNormal;

				for (auto& axis : axes)
				{
					if (axis.LengthSqrt() < 1e-6)
						continue;

					auto [overlap, penetration] = OverlapOnAxis(objectColliderPts, terrainPts, axis);
					if (!overlap)
					{
						separatingAxisFound = true;
						break;
					}

					if (penetration < minPenetration)
					{
						minPenetration = penetration;
						collisionNormal = axis;
					}
				}

				if (!separatingAxisFound)
				{
					// Collision detected with this triangle
					if (minPenetration < collision.Depth)
					{
						collision.Depth = minPenetration;
						collision.Normal = collisionNormal;
						collisionDetected = true;
					}

					// Ensure collision.Normal points from the box into the terrain
					Vector3 objectToTerrain = terrainPts[0] - objectPos;
					if (Vector3::Dot(collision.Normal, objectToTerrain) > 0)
					{
						// Invert the collision normal
						collision.Normal = -collision.Normal;
					}

					break;
				}
			}

			if (collisionDetected)
			{
				FindContactPointsOBB(objectColliderPts, terrainPts, collision);

				return true;
			}
			else
				return false;
		}

		static bool SphereTerrainCollisionCheck(Vector3 sphereCenter, double radius, const double dt, TerrainCollision& collision, const std::vector<Vector3>& colliderVertices)
		{
			TOAST_PROFILE_FUNCTION();

			// Initialize collision depth to a very large negative number to ensure
			// the first positive collision depth found will replace it.
			collision.Depth = -DBL_MAX;
			collision.Normal = Vector3(0.0, 0.0, 0.0);
			bool collisionDetected = false;

			for (size_t i = 0; i + 2 < colliderVertices.size(); i += 3)
			{
				const Vector3& va = colliderVertices[i];
				const Vector3& vb = colliderVertices[i + 1];
				const Vector3& vc = colliderVertices[i + 2];

				Vector3 closestPoint = ClosestPointOnTriangle(va, vb, vc, sphereCenter);

				Vector3 diff = closestPoint - sphereCenter;
				double distance = diff.Length();
				double penetrationDepth = radius - distance;

				// If this triangle gives a greater penetration depth (i.e., it's a tighter collision), update.
				if (penetrationDepth > collision.Depth)
				{
					collision.Depth = penetrationDepth;
					collision.Normal = (penetrationDepth >= 0.0) ? Vector3::Normalize(sphereCenter - closestPoint) : Vector3(0.0, 0.0, 0.0);

					Vector3 sphereContactPoint = sphereCenter - collision.Normal * (radius - collision.Depth);

					collision.ContactPoints.clear();
					collision.ContactPoints.emplace_back();
					collision.ContactPoints.back().PtOnPlanetWorldSpace = closestPoint;
					collision.ContactPoints.back().PtOnObjectWorldSpace = sphereContactPoint;

					collisionDetected = (collision.Depth >= 0.0);
				}
			}

			return collisionDetected;
		}

		static void ApplyLinearImpulse(RigidBodyComponent& rbc, Vector3 impulse)
		{
			if (rbc.InvMass == 0.0)
				return;

			//TOAST_CORE_CRITICAL("Linear Velocity BEFORE applying impulse: %lf", rbc.LinearVelocity.Length());

			rbc.LinearVelocity += (impulse * rbc.InvMass);

			//TOAST_CORE_CRITICAL("Linear Velocity AFTER applying impulse: %lf", rbc.LinearVelocity.Length());
		}

		static void ApplyImpulseAngular(RigidBodyComponent& rbc, Matrix objectInvInertiaWorld, Vector3 impulse)
		{
			if (rbc.InvMass == 0.0)
				return;

			rbc.AngularVelocity += objectInvInertiaWorld * impulse;

			const double maxAngularSpeed = 30.0;

			if (rbc.AngularVelocity.Length() > maxAngularSpeed)
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

		static bool TerrainCollisionCheck(Ref<PlanetNode>& leafNode, Entity* planet, Entity* object, TerrainCollision& collision, float dt)
		{
			TOAST_PROFILE_FUNCTION();

			collision.Object = object;
			collision.Planet = planet;

			RigidBodyComponent& rbcObject = object->GetComponent<RigidBodyComponent>();
			TransformComponent objectTC = object->GetComponent<TransformComponent>();

			Vector3 posObject = { object->GetComponent<TransformComponent>().Translation };

			Vector3 Apos = leafNode->A.Position;
			Vector3 Bpos = leafNode->B.Position;
			Vector3 Cpos = leafNode->C.Position;

			if (object->HasComponent<SphereColliderComponent>())
			{
				bool collisionDetected = false;
				double sphereRadius = object->GetComponent<SphereColliderComponent>().Collider->mRadius;

				collisionDetected = SphereTerrainCollisionCheck(posObject, sphereRadius, dt, collision, { Apos, Bpos, Cpos });

				if (collisionDetected) 
					return true;
			}
			else if (object->HasComponent<BoxColliderComponent>())
			{
				bool collisionDetected = false;

				collisionDetected = BoxPlanetCollisionCheck(collision, { Apos, Bpos, Cpos });

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

			if (objectHasRigidBody)
				rbcObject = &collision.Object->GetComponent<RigidBodyComponent>();


			Ref<Shape> collider;
			if (collision.Object->HasComponent<SphereColliderComponent>())
				collider = collision.Object->GetComponent<SphereColliderComponent>().Collider;
			else if (collision.Object->HasComponent<BoxColliderComponent>())
				collider = collision.Object->GetComponent<BoxColliderComponent>().Collider;

			Vector3 objectCoMWorld = Matrix(collision.Object->GetComponent<TransformComponent>().GetTransform()) * rbcObject->CenterOfMass;

			Matrix rotationMatrix = Matrix(collision.Object->GetComponent<TransformComponent>().GetRotation());
			Matrix objectInvInertiaWorld = rotationMatrix * collider->GetInvInertiaTensor() * rotationMatrix.Transpose();

			// Elasticity
			const double elasticityObject = objectHasRigidBody ? rbcObject->Elasticity : 0.0;
			const double elasticity = elasticityObject;

			Vector3 totalImpulse = { 0, 0, 0 };
			Vector3 totalAngularImpulse = { 0, 0, 0 };

			int contactCount = collision.ContactPoints.size();

			for (const ContactPoint& contact : collision.ContactPoints)
			{
				const Vector3 rObject = contact.PtOnObjectWorldSpace - objectCoMWorld;

				const Vector3 angularJObject = Vector3::Cross(objectInvInertiaWorld * Vector3::Cross(rObject, collision.Normal), rObject);
				const double angularFactor = Vector3::Dot(angularJObject, collision.Normal);

				const double epsilon = 1e-6;
				const double denominator = rbcObject->InvMass + (std::max)(angularFactor, epsilon);

				const Vector3 velObject = rbcObject->LinearVelocity + Vector3::Cross(rbcObject->AngularVelocity, rObject);

				// In terrain collision we can set the vab to velObject cause the planet isn't really moving in that sense.
				const Vector3 vab = velObject;
				const double impulseJ = (-(1.0 + elasticity) * Vector3::Dot(vab, collision.Normal)) / denominator;
				const Vector3 vectorImpulseJ = collision.Normal * impulseJ;

				totalImpulse += vectorImpulseJ;
				totalAngularImpulse += Vector3::Cross(rObject, vectorImpulseJ);
			}

			if (contactCount > 0)
			{
				// Average the impulses over the contact points
				totalImpulse /= contactCount;
				totalAngularImpulse /= contactCount;
			}

			if (collision.Object->HasComponent<SphereColliderComponent>() && totalImpulse.Length() < 1000.0)
			{
				rbcObject->IsStatic = true;
			}
			else 
			{
				ApplyLinearImpulse(*rbcObject, totalImpulse);
				ApplyImpulseAngular(*rbcObject, objectInvInertiaWorld, totalAngularImpulse);
			}

			Vector3 objectPos = { collision.Object->GetComponent<TransformComponent>().Translation };

			Vector3 updatedPos = objectPos + collision.Normal * collision.Depth;

			collision.Object->GetComponent<TransformComponent>().Translation = { (float)updatedPos.x, (float)updatedPos.y, (float)updatedPos.z };

			// Friction in the future

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

		//static bool BroadPhaseCheck(Ref<ShapeBox>& terrainCollider, Entity& object, float dt)
		//{
		//	TOAST_PROFILE_FUNCTION();

		//	Vector3 objectPos = object.GetComponent<TransformComponent>().Translation;
		//	Vector3 objectLinearVel = object.GetComponent<RigidBodyComponent>().LinearVelocity;
		//	Bounds terrainBounds = terrainCollider->GetBounds();
		//	Bounds objectBounds;

		//	Ref<Shape> collider;
		//	if (object.HasComponent<SphereColliderComponent>())
		//		collider = object.GetComponent<SphereColliderComponent>().Collider;
		//	else if (object.HasComponent<BoxColliderComponent>())
		//		collider = object.GetComponent<BoxColliderComponent>().Collider;

		//	if (!collider)
		//		return false;

		//	objectBounds = collider->GetBounds();

		//	objectBounds = objectBounds + objectPos;

		//	objectBounds.Expand(objectPos + objectLinearVel * dt);

		//	bool intersects = terrainBounds.Intersects(objectBounds);

		//	if (intersects)
		//	{
		//		//TOAST_CORE_CRITICAL("Broadphase about to be passed");
		//		//objectBounds.maxs.ToString("Object Maxs: ");
		//		//objectBounds.mins.ToString("Object Mins: ");
		//		//terrainBounds.maxs.ToString("Terrain Maxs: ");
		//		//terrainBounds.mins.ToString("Terrain Mins: ");

		//	}

		//	return intersects;
		//}

		static void UpdateSphereAltitudeAndCollision(Entity* planetEntity, Entity* objectEntity, double dt) 
		{
			TerrainCollision terrainCollision;

			terrainCollision.Planet = planetEntity;
			terrainCollision.Object = objectEntity;

			auto& planet = planetEntity->GetComponent<PlanetComponent>();
			auto& transform = objectEntity->GetComponent<TransformComponent>();
			auto& rigidBody = objectEntity->GetComponent<RigidBodyComponent>();

			double sphereRadius = 0.0;
			if (objectEntity->HasComponent<SphereColliderComponent>()) 
				sphereRadius = objectEntity->GetComponent<SphereColliderComponent>().Collider->mRadius;

			Vector3 objectPos = transform.Translation;
			Vector3 planetCenter = Vector3(planet.PlanetData.planetCenter.x,
				planet.PlanetData.planetCenter.y,
				planet.PlanetData.planetCenter.z);

			Vector3 toCenter = planetCenter - objectPos;
			double distToCenter = toCenter.Length();
			if (distToCenter < 1e-8) {
				// Object is basically at the planet center
				rigidBody.Altitude = -(planet.PlanetData.radius);
				return;
			}

			// Create a ray from object to planet center
			Ray ray;
			ray.Origin = objectPos;
			ray.Direction = toCenter / distToCenter; // normalize direction

			// Ray cast against the planet nodes
			double closestT = DBL_MAX;
			Vector3 bestHit;
			bool hitFound = false;
			for (auto& rootNode : planet.PlanetNodesWorldSpace) {

				if (rootNode == NULL)
					continue;

				double t;
				Vector3 hp;
				if (RaycastPlanetNode(ray, rootNode, t, hp)) {
					if (!hitFound || t < closestT) {
						hitFound = true;
						closestT = t;
						bestHit = hp;
					}
				}
			}

			if (!hitFound)
			{
				// No intersection found, object is above no actual mesh intersection
				// Approximate altitude by planet radius (no local terrain detail)
				return;
			}

			// We have a hit point on the planet surface mesh
			double surfaceDistFromCenter = (bestHit - planetCenter).Length();

			// Check if the sphere is penetrating the terrain
			double penetration = sphereRadius - (distToCenter - surfaceDistFromCenter);
			terrainCollision.Depth = penetration;
			rigidBody.Altitude = -terrainCollision.Depth;

			terrainCollision.Normal = Vector3::Normalize(bestHit - planetCenter);

			Vector3 sphereContactPoint = objectPos - terrainCollision.Normal * (sphereRadius - penetration);

			terrainCollision.ContactPoints.clear();
			terrainCollision.ContactPoints.emplace_back();
			terrainCollision.ContactPoints.back().PtOnPlanetWorldSpace = bestHit;
			terrainCollision.ContactPoints.back().PtOnObjectWorldSpace = sphereContactPoint;

			if (penetration > 0.0) 
				ResolveTerrainCollision(terrainCollision);
		}

		static void CheckTerrainBroadPhase(Ref<PlanetNode>& node, Entity* planetEntity, Entity* objectEntity, double dt_sub, const Bounds& objectBounds)
		{
			// Broad phase intersection test
			if (!node->NodeBounds.Intersects(objectBounds)) 
				return;

			// If not a leaf, go deeper
			//TOAST_CORE_CRITICAL("Subdivision Level: %d, Number of children: %d", node->SubdivisionLevel, node->ChildNodes.size());
			if (!node->ChildNodes.empty()) 
			{
				for (auto& child : node->ChildNodes) 
					CheckTerrainBroadPhase(child, planetEntity, objectEntity, dt_sub, objectBounds);
			}
			else 
			{
				// Leaf node: Perform narrow-phase on its triangle(s)
				TerrainCollision terrainCollision;
				if (TerrainCollisionCheck(node, planetEntity, objectEntity, terrainCollision, dt_sub))
					ResolveTerrainCollision(terrainCollision);
			}
		}

		static void CheckPlanetCollisions(Entity planetEntity, Entity objectEntity, double dt_sub) {
			auto& planet = planetEntity.GetComponent<PlanetComponent>();

			//TOAST_CORE_CRITICAL("NEW PLANET CHECK");

			Ref<Shape> collider;
			if (objectEntity.HasComponent<SphereColliderComponent>())
				collider = objectEntity.GetComponent<SphereColliderComponent>().Collider;
			else if (objectEntity.HasComponent<BoxColliderComponent>())
				collider = objectEntity.GetComponent<BoxColliderComponent>().Collider;

			if (!collider)
				return;

			Vector3 objectPos = objectEntity.GetComponent<TransformComponent>().Translation;
			Vector3 objectLinearVel = objectEntity.GetComponent<RigidBodyComponent>().LinearVelocity;
			bool reqAltitude = objectEntity.GetComponent<RigidBodyComponent>().ReqAltitude;

			// Get object bounds
			Bounds objectBounds;
			objectBounds = collider->GetBounds();
			objectBounds = objectBounds + objectPos;
			objectBounds.Expand(objectPos + objectLinearVel * dt_sub);

			// Traverse the planets root nodes
			if (!reqAltitude)
			{
				int i = 0;
				for (auto& rootNode : planet.PlanetNodesWorldSpace)
					CheckTerrainBroadPhase(rootNode, &planetEntity, &objectEntity, dt_sub, objectBounds);
			}
			else 
				UpdateSphereAltitudeAndCollision(&planetEntity, &objectEntity, dt_sub);
		}

		static void Update(entt::registry* registry, Scene* scene, double dt, double slowmotion, uint32_t numSubSteps)
		{
			TOAST_PROFILE_FUNCTION();

			dt = dt / slowmotion;

			double dt_sub = dt / static_cast<double>(numSubSteps);

			//RendererDebug::SubmitLine(Vector3(-10.0, 10.0, 10.0), Vector3(10.0, 10.0, 10.0), DirectX::XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f));

			auto view = registry->view<TransformComponent, RigidBodyComponent>();

			auto planetView = registry->view<PlanetComponent>();
			if (planetView.size() > 0)
			{
				Entity planetEntity = { planetView[0], scene };
				TerrainColliderComponent& tcc = planetEntity.GetComponent<TerrainColliderComponent>();
				for (auto entity : view)
				{
					Entity objectEntity = { entity, scene };
					auto [tc, rbc] = view.get<TransformComponent, RigidBodyComponent>(entity);

					Ref<Shape> collider;
					if(objectEntity.HasComponent<SphereColliderComponent>())
						collider = objectEntity.GetComponent<SphereColliderComponent>().Collider;
					if (objectEntity.HasComponent<BoxColliderComponent>())
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

						for (int i = 0; i < numSubSteps; ++i)
						{
							// Gravity
							if (!rbc.IsStatic)
							{
								Vector3 impulseGravity = Gravity(planetEntity, objectEntity, dt_sub);
								ApplyLinearImpulse(rbc, impulseGravity);
							}

							// Terrain collision check
							CheckPlanetCollisions(planetEntity, objectEntity, dt_sub);

							UpdateBody(objectEntity, dt);
						}
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