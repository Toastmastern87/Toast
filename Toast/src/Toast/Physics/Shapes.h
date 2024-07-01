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
		virtual void RecalcInertiaTensor() = 0;

		virtual Vector3 Support(Vector3& dir, const Vector3& pos, const Quaternion& quat, const double bias) const = 0;

		virtual float FastestLinearSpeed(const Vector3& angularVelocity, const Vector3& dir) const { return 0.0f; }

		virtual Bounds GetBounds(const Vector3& pos, const Quaternion& quat) const = 0;
		virtual Bounds GetBounds() const = 0;

		virtual Vector3 GetCenterOfMass() const { return mCenterOfMass; }
		virtual Matrix GetInertiaTensor() const { return mInertiaTensor; }
		virtual Matrix GetInvInertiaTensor() const { return mInvInertiaTensor; }

	protected:
		Vector3 mCenterOfMass;
		Matrix mInertiaTensor;
		Matrix mInvInertiaTensor;
	};

	class ShapeSphere : public Shape
	{
	public:
		ShapeSphere(double radius);

		ShapeType GetType() const override { return ShapeType::SPHERE; }

		void RecalcInertiaTensor() override;

		Vector3 Support(Vector3& dir, const Vector3& pos, const Quaternion& quat, const double bias) const override;

		Bounds GetBounds(const Vector3& pos, const Quaternion& quat) const override;
		Bounds GetBounds() const override;

	public:
		double mRadius;
	};

	class ShapeBox : public Shape
	{
	public:
		ShapeBox(Vector3 size) : mSize(size) { RecalcInertiaTensor(); }
		explicit ShapeBox(const Vector3* pts, const int num) {}

		ShapeType GetType() const override { return ShapeType::BOX; }

		Vector3 Support(Vector3& dir, const Vector3& pos, const Quaternion& quat, const double bias) const override;

		void RecalcInertiaTensor() override;

		Bounds GetBounds(const Vector3& pos, const Quaternion& quat) const override;
		Bounds GetBounds() const override;
		Bounds GetBounds(std::vector<Vertex>& pts, Matrix& transform);

		float FastestLinearSpeed(const Vector3& angularVelocity, const Vector3& dir) const override;

	public:
		Vector3 mSize = { 1.0f, 1.0f, 1.0f };

		std::vector<Vector3> mPoints;
		Bounds mBounds;
	};

	class ShapeTerrain : public Shape
	{
	public:
		ShapeTerrain() = default;
		ShapeTerrain(std::string filePath) : FilePath(filePath) {}

		ShapeType GetType() const override { return ShapeType::TERRAIN; }

		void RecalcInertiaTensor() override;

		Vector3 Support(Vector3& dir, const Vector3& pos, const Quaternion& quat, const double bias) const override;

		Bounds GetBounds(const Vector3& pos, const Quaternion& quat) const override;
		Bounds GetBounds() const override;

	public:
		std::string FilePath;
	};

}