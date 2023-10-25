#include "tpch.h"
#include "Shapes.h"

#include "../Core/Math/Math.h"

namespace Toast {

	////////////////////////////////////////////////////////////////////////////////////////  
	//      SPHERE       ///////////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	ShapeSphere::ShapeSphere(double radius) : mRadius(radius)
	{
		mCenterOfMass = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
	}

	Bounds ShapeSphere::GetBounds(const Vector3& pos, const Quaternion& quat) const
	{
		Bounds result;
		result.mins = Vector3(-mRadius + pos.x, -mRadius + pos.y, -mRadius + pos.z);
		result.maxs = Vector3(mRadius + pos.x, mRadius + pos.y, mRadius + pos.z);

		return result;
	}

	Bounds ShapeSphere::GetBounds() const
	{
		Bounds result;
		result.mins = Vector3(-mRadius, -mRadius, -mRadius);
		result.maxs = Vector3(mRadius, mRadius, mRadius);

		return result;
	}

	Matrix ShapeSphere::GetInertiaTensor() const
	{
		Matrix tensor(
			1.0, 0.0, 0.0, 0.0,
			0.0, 1.0, 0.0, 0.0,
			0.0, 0.0, 1.0, 0.0,
			0.0, 0.0, 0.0, 1.0
		);
		tensor.m_00 = 0.4 * mRadius * mRadius;
		tensor.m_11 = 0.4 * mRadius * mRadius;
		tensor.m_22 = 0.4 * mRadius * mRadius;
		return tensor;
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

	Matrix ShapeBox::GetInertiaTensor() const
	{
		// Inertia Tensor for a box centered around zero
		const double dx = mBounds.maxs.x - mBounds.mins.x;
		const double dy = mBounds.maxs.y - mBounds.mins.y;
		const double dz = mBounds.maxs.z - mBounds.mins.z;

		Matrix tensor(
			1.0, 0.0, 0.0, 0.0,
			0.0, 1.0, 0.0, 0.0,
			0.0, 0.0, 1.0, 0.0,
			0.0, 0.0, 0.0, 1.0
		);

		tensor.m_00 = (dy * dy + dz * dz) / 12.0;
		tensor.m_11 = (dx * dx + dz * dz) / 12.0;
		tensor.m_22 = (dx * dx + dy * dy) / 12.0;

		// Now we need to use the parallel axis theorem to get the inertia tensor for a box that is not 
		// centered around zero
		Vector3 cm;
		cm.x = (mBounds.mins.x + mBounds.maxs.x) * 0.5;
		cm.y = (mBounds.mins.y + mBounds.maxs.y) * 0.5;
		cm.z = (mBounds.mins.z + mBounds.maxs.z) * 0.5;

		const Vector3 R = { 0.0 - cm.x, 0.0 - cm.y, 0.0 - cm.z };
		const double R2 = R.Magnitude() * R.Magnitude();
		Matrix patTensor;
		// Row 1
		patTensor.m_11 = R2 - R.x * R.x;
		patTensor.m_12 = R.x * R.y;
		patTensor.m_13 = R.x * R.z;
		// Row 2
		patTensor.m_21 = R.y * R.x;
		patTensor.m_22 = R2 - R.y * R.y;
		patTensor.m_23 = R.y * R.z;
		// Row 3
		patTensor.m_31 = R.z * R.x;
		patTensor.m_32 = R.z * R.y;
		patTensor.m_33 = R2 - R.z * R.z;

		// Now we need to add the center of mass tensor and the parallel axis theorem tensor together;
		tensor.m_11 += patTensor.m_11;
		tensor.m_12 += patTensor.m_12;
		tensor.m_13 += patTensor.m_13;
		tensor.m_21 += patTensor.m_21;
		tensor.m_22 += patTensor.m_22;
		tensor.m_23 += patTensor.m_23;
		tensor.m_31 += patTensor.m_31;
		tensor.m_32 += patTensor.m_32;
		tensor.m_33 += patTensor.m_33;

		return tensor;
	}

	Bounds ShapeBox::GetBounds(const Vector3& pos, const Quaternion& quat) const
	{
		//DirectX::XMFLOAT3 corners[8];
		//corners[0] = { mBounds.mins.x, mBounds.mins.y, mBounds.mins.z };
		//corners[1] = { mBounds.mins.x, mBounds.mins.y, mBounds.maxs.z };
		//corners[2] = { mBounds.mins.x, mBounds.maxs.y, mBounds.mins.z };
		//corners[3] = { mBounds.maxs.x, mBounds.mins.y, mBounds.mins.z };

		//corners[4] = { mBounds.maxs.x, mBounds.maxs.y, mBounds.maxs.z };
		//corners[5] = { mBounds.mins.x, mBounds.maxs.y, mBounds.mins.z };
		//corners[6] = { mBounds.maxs.x, mBounds.mins.y, mBounds.maxs.z };
		//corners[7] = { mBounds.mins.x, mBounds.maxs.y, mBounds.maxs.z };

		//Bounds bounds;
		//for (int i = 0; i < 8; i++)
		//{
		//	DirectX::XMStoreFloat3(&corners[i], DirectX::XMVectorAdd(DirectX::XMVector3Rotate(DirectX::XMLoadFloat3(&corners[i]), DirectX::XMLoadFloat4(&quat)), DirectX::XMLoadFloat3(&pos)));
		//	bounds.Expand(corners[i]);
		//}

		return mBounds;
	}

	Bounds ShapeBox::GetBounds() const
	{
		return mBounds;
	}

	Bounds ShapeBox::GetBounds(std::vector<Vertex>& pts, Matrix& transform)
	{
		Bounds bounds;
		bounds.maxs = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
		bounds.mins = { FLT_MAX, FLT_MAX, FLT_MAX };

		for (Vertex& point : pts)
		{
			Vector3 vec = { point.Position };
			vec = transform * vec;

			bounds.maxs.x = std::max(vec.x, vec.x);
			bounds.maxs.y = std::max(vec.y, vec.y);
			bounds.maxs.z = std::max(vec.z, vec.z);

			bounds.mins.x = std::min(vec.x, vec.x);
			bounds.mins.y = std::min(vec.y, vec.y);
			bounds.mins.z = std::min(vec.z, vec.z);
		}

		mBounds = bounds;

		return bounds;
	}

	float ShapeBox::FastestLinearSpeed(const Vector3& angularVelocity, const Vector3& dir) const
	{
		double maxSpeed = 0.0;
		for (int i = 0; i < mPoints.size(); i++)
		{
			Vector3 r = mPoints[i] - DirectX::XMLoadFloat3(&mCenterOfMass);
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

	Bounds ShapeTerrain::GetBounds(const Vector3& pos, const Quaternion& quat) const
	{
		Bounds result;
		result.mins = Vector3(-pos.x, -pos.y, -pos.z);
		result.maxs = Vector3(pos.x, pos.y, pos.z);

		return result;
	}

	Bounds ShapeTerrain::GetBounds() const
	{
		Bounds result;
		result.mins = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
		result.maxs = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);

		return result;
	}

	Matrix ShapeTerrain::GetInertiaTensor() const
	{
		Matrix tensor(
			1.0, 0.0, 0.0, 0.0,
			0.0, 1.0, 0.0, 0.0,
			0.0, 0.0, 1.0, 0.0,
			0.0, 0.0, 0.0, 1.0
		);

		return tensor;
	}

	Vector3 ShapeTerrain::Support(Vector3& dir, const Vector3& pos, const Quaternion& quat, const double bias) const
	{
		return { 0.0, 0.0, 0.0 };
	}

}