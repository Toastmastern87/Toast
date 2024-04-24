#pragma once

#include "Toast/Core/Math/Vector.h"
#include "Toast/Core/Math/Quaternion.h"
#include "Toast/Core/Math/Matrix.h"

#include <DirectXMath.h>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

template<typename T>
inline T min(T a, T b) {
	return (a < b) ? a : b;
}

template<typename T>
inline T max(T a, T b) {
	return (a > b) ? a : b;
}

template<typename T>
T min(std::initializer_list<T> ilist) {
	return *std::min_element(ilist.begin(), ilist.end());
}

template<typename T>
T max(std::initializer_list<T> ilist) {
	return *std::max_element(ilist.begin(), ilist.end());
}

namespace Toast {

	namespace Math 
	{
		#define MAX_INT_VALUE	65535.0
		#define M_PI			3.14159265358979323846
		#define M_PIDIV2		(3.14159265358979323846 / 2.0)

		static double DegreesToRadians(double degrees)
		{
			return degrees * (3.14159265358979323846 / 180.0);
		}

		static void BarycentricCoordinates(const Vector3& P, const Vector3& A, const Vector3& B, const Vector3& C, double& u, double& v, double& w) {
			Vector3 v0 = B - A, v1 = C - A, v2 = P - A;

			Vector3 n = Vector3::Cross(v0, v1); // Normal vector of the plane
			double projFactor = Vector3::Dot(n, v2) / Vector3::Dot(n, n);

			double d00 = Vector3::Dot(v0, v0);
			double d01 = Vector3::Dot(v0, v1);
			double d11 = Vector3::Dot(v1, v1);
			double d20 = Vector3::Dot(v2, v0);
			double d21 = Vector3::Dot(v2, v1);
			double denom = d00 * d11 - d01 * d01;

			v = (d11 * d20 - d01 * d21) / denom;
			w = (d00 * d21 - d01 * d20) / denom;
			u = 1.0 - v - w;
		}

		static void FBarycentricCoordinates(const Vector3& P, const Vector3& A, const Vector3& B, const Vector3& C, float& u, float& v, float& w) {
			DirectX::XMFLOAT3 point = { (float)P.x, (float)P.y, (float)P.z };
			DirectX::XMVECTOR pointVec = DirectX::XMLoadFloat3(&point);

			DirectX::XMFLOAT3 Apoint = { (float)A.x, (float)A.y, (float)A.z };
			DirectX::XMVECTOR ApointVec = DirectX::XMLoadFloat3(&Apoint);

			DirectX::XMFLOAT3 Bpoint = { (float)B.x, (float)B.y, (float)B.z };
			DirectX::XMVECTOR BpointVec = DirectX::XMLoadFloat3(&Bpoint);

			DirectX::XMFLOAT3 Cpoint = { (float)C.x, (float)C.y, (float)C.z };
			DirectX::XMVECTOR CpointVec = DirectX::XMLoadFloat3(&Cpoint);

			DirectX::XMVECTOR v0 = DirectX::XMVectorSubtract(BpointVec, ApointVec), v1 = DirectX::XMVectorSubtract(CpointVec, ApointVec), v2 = DirectX::XMVectorSubtract(pointVec, ApointVec);
			TOAST_CORE_CRITICAL("v0: %f, %f, %f", DirectX::XMVectorGetX(v0), DirectX::XMVectorGetY(v0), DirectX::XMVectorGetZ(v0));
			TOAST_CORE_CRITICAL("v1: %f, %f, %f", DirectX::XMVectorGetX(v1), DirectX::XMVectorGetY(v1), DirectX::XMVectorGetZ(v1));
			TOAST_CORE_CRITICAL("v2: %f, %f, %f", DirectX::XMVectorGetX(v2), DirectX::XMVectorGetY(v2), DirectX::XMVectorGetZ(v2));
			float d00 = DirectX::XMVectorGetX(DirectX::XMVector3Dot(v0, v0));
			float d01 = DirectX::XMVectorGetX(DirectX::XMVector3Dot(v0, v1));
			float d11 = DirectX::XMVectorGetX(DirectX::XMVector3Dot(v1, v1));
			float d20 = DirectX::XMVectorGetX(DirectX::XMVector3Dot(v2, v0));
			float d21 = DirectX::XMVectorGetX(DirectX::XMVector3Dot(v2, v1));
			float denom = d00 * d11 - d01 * d01;

			TOAST_CORE_CRITICAL("Denom: %f", denom);

			v = (d11 * d20 - d01 * d21) / denom;
			w = (d00 * d21 - d01 * d20) / denom;
			u = 1.0f - v - w;
		}

		static double BilinearInterpolation(Vector2 P, double Q11, double Q12, double Q21, double Q22)
		{
			// Calculate the fractional parts
			double fracX = P.x - floor(P.x);
			double fracY = P.y - floor(P.y);

			// Interpolate the Q values
			double R1 = Q11 * (1.0 - fracX) + Q21 * fracX;
			double R2 = Q12 * (1.0 - fracX) + Q22 * fracX;
			double result = R1 * (1.0 - fracY) + R2 * fracY;
			return result;
		}

		static double BarycentricInterpolation(const Vector3& P, const Vector3& A, const Vector3& B, const Vector3& C, double heightA, double heightB, double heightC)
		{
			double u, v, w;
			BarycentricCoordinates(P, A, B, C, u, v, w);
			double epsilon = 1e-6;

			bool isInside = (u >= -epsilon) && (v >= -epsilon) && ((u + v) <= 1 + epsilon);

			if (!isInside)
				return -1.0f;

			return u * heightA + v * heightB + w * heightC;
		}

		static double FBarycentricHeightInterpolation(const Vector3& P, const Vector3& A, const Vector3& B, const Vector3& C, float heightA, float heightB, float heightC)
		{
			float u, v, w;
			FBarycentricCoordinates(P, A, B, C, u, v, w);
			float epsilon = 1e-6f;

			bool isInside = (u >= -epsilon) && (v >= -epsilon) && ((u + v) <= 1 + epsilon);

			if (!isInside)
				return -1.0f;

			return u * heightA + v * heightB + w * heightC;
		}

		static bool TriangleIntersect(const Vector3& P, const Vector3& Q, const Vector3& A, const Vector3& B, const Vector3& C, int& triangleNumber)
		{
			//Vector3 direction = Q - P;
			//Vector3 directionNormalized = Vector3::Normalize(direction);
			//double necessary_length = direction.Magnitude() - 3389500.0 + 8200.0;

			//Vector3 QUpdate = P + directionNormalized * necessary_length;

			//Vector3 v0 = B - A, v1 = C - A, v2 = (directionNormalized - P);
			//Vector3 n = Vector3::Cross(v0, v1);
			//double denominator = Vector3::Dot(n, v2);
			////v0.ToString("v0: ");
			////v1.ToString("v1: ");
			////v2.ToString("v2: ");
			////n.ToString("n: ");
			////TOAST_CORE_CRITICAL("denominator: %lf", denominator);
			//if (fabs(denominator) < 1e-18) { TOAST_CORE_CRITICAL("Check for parallelism");  return false; } // Check for parallelism

			//double t = Vector3::Dot(n, (A - P)) / denominator;
			//if (t < 0 || t > 1) { TOAST_CORE_CRITICAL("Intersection point is outside the line segment PQ"); return false; } // Intersection point is outside the line segment PQ

			//Vector3 I = P + v2 * t;

			//// Check if I is inside the triangle
			//Vector3 c1 = Vector3::Cross(v0, I - A);
			//Vector3 c2 = Vector3::Cross((C - B), (I - B));
			//Vector3 c3 = Vector3::Cross((A - C), (I - C));

			//TOAST_CORE_CRITICAL("Vector3::Dot(n, c1): %lf, Vector3::Dot(n, c2): %lf, Vector3::Dot(n, c3): %lf", Vector3::Dot(n, c1), Vector3::Dot(n, c2), Vector3::Dot(n, c3));

			//if ((Vector3::Dot(n, c1) >= 0.0) && (Vector3::Dot(n, c2) >= 0.0) && (Vector3::Dot(n, c3) >= 0.0)) {
			//	return true; // I is inside the triangle
			//}

			//TOAST_CORE_CRITICAL("I is outside the triangle");
			//return false; // I is outside the triangle

			const double EPSILON = 1e-18;
			Vector3 edge1 = B - A;
			Vector3 edge2 = C - A;
			Vector3 direction = Vector3::Normalize((Q - P));
			Vector3 h = Vector3::Cross(direction, edge2);
			double a = Vector3::Dot(edge1, h);

			if (a > -EPSILON && a < EPSILON)  // The ray is parallel to the triangle.
				return false;

			double f = 1.0 / a;
			Vector3 s = P - A;
			double u = f * Vector3::Dot(s, h);

			if (u < 0.0 || u > 1.0)
				return false;

			Vector3 q = Vector3::Cross(s, edge1);
			double v = f * Vector3::Dot(direction, q);

			if (v < 0.0 || u + v > 1.0)
				return false;

			// At this stage, we can compute t to find out where the intersection point is on the line.
			double t = f * Vector3::Dot(edge2, q);

			if (t > EPSILON) 
				// Ray intersection
				return true;
			else  // This means that there is a line intersection but not a ray intersection.
				return false;
		}

		static double PointToPlaneDistance(Vector3 P, Vector3 Q, Vector3 A, Vector3 B, Vector3 C)
		{
			double u, v, w;

			BarycentricCoordinates(P, A, B, C, u, v, w);

			double epsilon = 1e-18;

			bool isInside = (u >= -epsilon) && (v >= -epsilon) && ((u + v) <= 1 + epsilon);

			if (!isInside)
				return -100.0f;

			// Step 1: Find two vectors that lie on the plane
			Vector3 AB = B - A;
			Vector3 AC = C - A;

			// Step 2: Compute the normal of the plane
			Vector3 normal = Vector3::Normalize(Vector3::Cross(AB, AC));

			// Step 3: Compute the distance from P to the plane
			Vector3 AP = P - A;
			double distance = std::abs(Vector3::Dot(AP, normal));

			return distance;
		}

	}

}