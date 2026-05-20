#pragma once

#include "Bounds.h"

#include "Toast/Core/Math/Math.h"

#include "../Renderer/Mesh.h"

#include <DirectXMath.h>

#include <../vendor/directxtex/include/DirectXTex.h>

namespace Toast {

	enum ShapeType
	{
		SPHERE,
		BOX,
		TERRAIN
	};

	class Shape 
	{
	public:
		virtual void Build(const Vector3* pts, const int num) {}

		virtual ShapeType GetType() const = 0;

		virtual Vector3 Support(Vector3& dir, const Vector3& pos, const Quaternion& quat, const double bias) const = 0;

		//virtual float FastestLinearSpeed(const Vector3& angularVelocity, const Vector3& dir) const { return 0.0f; }

		virtual void CalculateBounds() = 0;
		virtual Bounds GetBounds() { return mBounds;  }

		virtual void SetIsDirty(bool dirty) { mIsDirty = dirty; }
		virtual bool GetIsDirty() const { return mIsDirty; }

		//virtual Vector3 GetCenterOfMass() const { return mCenterOfMass; }

	protected:
		bool mIsDirty = true;

		Bounds mBounds;

		//Vector3 mCenterOfMass;
	};

	class ShapeSphere : public Shape
	{
	public:
		ShapeSphere(double radius);

		ShapeType GetType() const override { return ShapeType::SPHERE; }

		Vector3 Support(Vector3& dir, const Vector3& pos, const Quaternion& quat, const double bias) const override;

		void CalculateBounds() override;

	public:
		double mRadius;
	};

	class ShapeBox : public Shape
	{
	public:
		ShapeBox() = default;
		ShapeBox(Vector3 size) : mSize(size) {};
		ShapeBox(const Bounds& bounds)
		{
			mBounds = bounds;      
		}
		explicit ShapeBox(const Vector3* pts, const int num) {}

		ShapeType GetType() const override { return ShapeType::BOX; }

		Vector3 Support(Vector3& dir, const Vector3& pos, const Quaternion& quat, const double bias) const override;

		void SetBounds(Bounds bounds);
		void ExpandToFit(const Bounds& b);

		void CalculateBounds() override;
		void BuildCornerPoints();

		//float FastestLinearSpeed(const Vector3& angularVelocity, const Vector3& dir) const override;
	public:
		Vector3 mSize = { 1.0f, 1.0f, 1.0f };
		Vector3 mOffset = { 0.0f, 0.0f, 0.0f };

		std::vector<Vector3> mPoints;		
	};

}