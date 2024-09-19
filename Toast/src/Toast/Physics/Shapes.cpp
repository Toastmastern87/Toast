#include "tpch.h"
#include "Shapes.h"

#include "../Core/Math/Math.h"

namespace Toast {

	////////////////////////////////////////////////////////////////////////////////////////  
	//      SPHERE       ///////////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	ShapeSphere::ShapeSphere(double radius) : mRadius(radius)
	{
		mCenterOfMass = Vector3(0.0f, 0.0f, 0.0f);
	}

	void ShapeSphere::CalculateBounds()
	{
		mBounds.mins = Vector3(-mRadius, -mRadius, -mRadius);
		mBounds.maxs = Vector3(mRadius, mRadius, mRadius);
	}

	void ShapeSphere::CalculateInertiaTensor(double mass)
	{
		Matrix tensor = Matrix::Zero();

		tensor.m_00 = 0.4 * mRadius * mRadius;
		tensor.m_11 = 0.4 * mRadius * mRadius;
		tensor.m_22 = 0.4 * mRadius * mRadius;
		tensor.m_33 = 1.0;
		
		mInertiaTensor = tensor;
		mInvInertiaTensor = Matrix::Inverse(mInertiaTensor);
	}

	Vector3 ShapeSphere::Support(Vector3& dir, const Vector3& pos, const Quaternion& rot, const double bias) const
	{
		return { pos.x + dir.x * (mRadius + bias), pos.y + dir.y * (mRadius + bias) , pos.z + dir.z * (mRadius + bias) };
	}

	////////////////////////////////////////////////////////////////////////////////////////  
	//        BOX        ///////////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	Vector3 ShapeBox::Support(Vector3& dir, const Vector3& pos, const Quaternion& quat, const double bias) const
	{
		// Find the point in the furthest in the direction of dir
		Vector3 maxPt = Vector3::Rotate(mPoints[0], quat) + pos;
		double maxDist = Vector3::Dot(dir, maxPt);

		for (int i = 1; i < mPoints.size(); i++)
		{
			Vector3 pt = Vector3::Rotate(mPoints[i], quat)  + pos;
			double dist = Vector3::Dot(dir, pt);

			if (dist > maxDist) 
			{
				maxDist = dist;
				maxPt = pt;
			}
		}

		Vector3 norm = Vector3::Normalize(dir);
		norm = norm * bias;

		Vector3 result = maxPt + norm;

		return result;
	}

	void ShapeBox::CalculateInertiaTensor(double mass)
	{	
		// Inertia Tensor for a box centered around zero
		const double width = mBounds.maxs.x - mBounds.mins.x;
		const double height = mBounds.maxs.y - mBounds.mins.y;
		const double depth = mBounds.maxs.z - mBounds.mins.z;

		Matrix tensor = Matrix::Zero();

		tensor.m_00 = (height * height + depth * depth) * mass * (1.0 / 12.0);
		tensor.m_11 = (width * width + depth * depth) * mass * (1.0 / 12.0);
		tensor.m_22 = (width * width + height * height) * mass * (1.0 / 12.0);
		tensor.m_33 = 1.0;  // To make the matrix invertible

		//// Now we need to use the parallel axis theorem to get the inertia tensor for a box that is not 
		//// centered around zero ADD THIS LATER WHEN CENTER OF MASS CAN BE AWAY FROM THE CENTER OF THE OBJECT
		//Vector3 cm;
		//cm.x = (mBounds.mins.x + mBounds.maxs.x) * 0.5;
		//cm.y = (mBounds.mins.y + mBounds.maxs.y) * 0.5;
		//cm.z = (mBounds.mins.z + mBounds.maxs.z) * 0.5;

		//const Vector3 R = { 0.0 - cm.x, 0.0 - cm.y, 0.0 - cm.z };
		//const double R2 = R.Magnitude() * R.Magnitude();
		//Matrix patTensor;
		//// Row 1
		//patTensor.m_11 = R2 - R.x * R.x;
		// 
		//patTensor.m_12 = R.x * R.y;
		//patTensor.m_13 = R.x * R.z;
		//// Row 2
		//patTensor.m_21 = R.y * R.x;
		//patTensor.m_22 = R2 - R.y * R.y;
		//patTensor.m_23 = R.y * R.z;
		//// Row 3
		//patTensor.m_31 = R.z * R.x;
		//patTensor.m_32 = R.z * R.y;
		//patTensor.m_33 = R2 - R.z * R.z;

		//// Now we need to add the center of mass tensor and the parallel axis theorem tensor together;
		//tensor.m_11 += patTensor.m_11;
		//tensor.m_12 += patTensor.m_12;
		//tensor.m_13 += patTensor.m_13;
		//tensor.m_21 += patTensor.m_21;
		//tensor.m_22 += patTensor.m_22;
		//tensor.m_23 += patTensor.m_23;
		//tensor.m_31 += patTensor.m_31;
		//tensor.m_32 += patTensor.m_32;
		//tensor.m_33 += patTensor.m_33;

		mInertiaTensor = tensor;

		mInvInertiaTensor = Matrix::Inverse(mInertiaTensor);
	}

	void ShapeBox::CalculateBounds()
	{
		Vector3 halfSize = mSize * 0.5;

		mBounds.mins = -halfSize;
		mBounds.maxs = halfSize;
	}

	float ShapeBox::FastestLinearSpeed(const Vector3& angularVelocity, const Vector3& dir) const
	{
		double maxSpeed = 0.0;
		for (int i = 0; i < mPoints.size(); i++)
		{
			Vector3 r = mPoints[i] - mCenterOfMass;
			Vector3 linearVelocity = Vector3::Cross(angularVelocity, r);
			double speed = Vector3::Dot(dir, linearVelocity);

			if (speed > maxSpeed)
				maxSpeed = speed;
		}

		return maxSpeed;
	}

	////////////////////////////////////////////////////////////////////////////////////////  
	//      TERRAIN      ///////////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	void ShapeTerrain::CalculateBounds()
	{
		mBounds.mins = Vector3(-mMaxAltitude, -mMaxAltitude, -mMaxAltitude);
		mBounds.maxs = Vector3(mMaxAltitude, mMaxAltitude, mMaxAltitude);
	}

	void ShapeTerrain::CalculateInertiaTensor(double mass)
	{
		Matrix tensor(
			1.0, 0.0, 0.0, 0.0,
			0.0, 1.0, 0.0, 0.0,
			0.0, 0.0, 1.0, 0.0,
			0.0, 0.0, 0.0, 1.0
		);

		mInertiaTensor = tensor;
		mInvInertiaTensor = tensor;
	}

	Vector3 ShapeTerrain::Support(Vector3& dir, const Vector3& pos, const Quaternion& quat, const double bias) const
	{
		return { 0.0, 0.0, 0.0 };
	}

	////////////////////////////////////////////////////////////////////////////////////////  
	//      TERRAIN FACE      //////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	ShapeTerrainFace::ShapeTerrainFace(Vector3 p1, Vector3 p2, Vector3 p3, Vector3 p4, Vector3 p5, Vector3 p6)
	{
		mPoints.emplace_back(p1);
		mPoints.emplace_back(p2);
		mPoints.emplace_back(p3);
		mPoints.emplace_back(p4);
		mPoints.emplace_back(p5);
		mPoints.emplace_back(p6);
	}

	void ShapeTerrainFace::CalculateInertiaTensor(double mass)
	{

	}

	Vector3 ShapeTerrainFace::Support(Vector3& dir, const Vector3& pos, const Quaternion& quat, const double bias) const
	{
		return { 0.0, 0.0, 0.0 };
	}

	void ShapeTerrainFace::CalculateBounds()
	{

	}

}