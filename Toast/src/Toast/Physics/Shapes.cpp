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

	void ShapeBox::SetBounds(Bounds bounds)
	{
		mBounds = bounds;

		Vector3 half = (bounds.maxs - bounds.mins) * 0.5;
		mSize = half * 2.0;          
		mCenterOfMass = (bounds.mins + bounds.maxs) * 0.5;

		BuildCornerPoints();
	}

	void ShapeBox::ExpandToFit(const Bounds& b)
	{
		mBounds.mins.x = (std::min)(mBounds.mins.x, b.mins.x);
		mBounds.mins.y = (std::min)(mBounds.mins.y, b.mins.y);
		mBounds.mins.z = (std::min)(mBounds.mins.z, b.mins.z);

		mBounds.maxs.x = (std::max)(mBounds.maxs.x, b.maxs.x);
		mBounds.maxs.y = (std::max)(mBounds.maxs.y, b.maxs.y);
		mBounds.maxs.z = (std::max)(mBounds.maxs.z, b.maxs.z);

		Vector3 half = (mBounds.maxs - mBounds.mins);
		mSize = half;
		mCenterOfMass = (mBounds.mins + mBounds.maxs);

		BuildCornerPoints();
	}

	void ShapeBox::CalculateBounds()
	{
		Vector3 halfSize = mSize;

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

	void ShapeBox::BuildCornerPoints()
	{
		Vector3 h = mSize;

		mPoints.clear();
		mPoints.reserve(8);
		mPoints.emplace_back(-h.x, -h.y, -h.z);
		mPoints.emplace_back(h.x, -h.y, -h.z);
		mPoints.emplace_back(h.x, h.y, -h.z);
		mPoints.emplace_back(-h.x, h.y, -h.z);
		mPoints.emplace_back(-h.x, -h.y, h.z);
		mPoints.emplace_back(h.x, -h.y, h.z);
		mPoints.emplace_back(h.x, h.y, h.z);
		mPoints.emplace_back(-h.x, h.y, h.z);
	}

}