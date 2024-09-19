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
		virtual void CalculateInertiaTensor(double mass) = 0;

		virtual Vector3 Support(Vector3& dir, const Vector3& pos, const Quaternion& quat, const double bias) const = 0;

		virtual float FastestLinearSpeed(const Vector3& angularVelocity, const Vector3& dir) const { return 0.0f; }

		virtual void CalculateBounds() = 0;
		virtual Bounds GetBounds() { return mBounds;  }

		virtual void SetIsDirty(bool dirty) { mIsDirty = dirty; }
		virtual bool GetIsDirty() const { return mIsDirty; }

		virtual Vector3 GetCenterOfMass() const { return mCenterOfMass; }
		virtual Matrix GetInertiaTensor() const { return mInertiaTensor; }
		virtual Matrix GetInvInertiaTensor() const { return mInvInertiaTensor; }

	protected:
		bool mIsDirty = true;

		Bounds mBounds;

		Vector3 mCenterOfMass;
		Matrix mInertiaTensor;
		Matrix mInvInertiaTensor;
	};

	class ShapeSphere : public Shape
	{
	public:
		ShapeSphere(double radius);

		ShapeType GetType() const override { return ShapeType::SPHERE; }

		void CalculateInertiaTensor(double mass = 100.0) override;

		Vector3 Support(Vector3& dir, const Vector3& pos, const Quaternion& quat, const double bias) const override;

		void CalculateBounds() override;

	public:
		double mRadius;
	};

	class ShapeBox : public Shape
	{
	public:
		ShapeBox(Vector3 size) : mSize(size) {};
		explicit ShapeBox(const Vector3* pts, const int num) {}

		ShapeType GetType() const override { return ShapeType::BOX; }

		Vector3 Support(Vector3& dir, const Vector3& pos, const Quaternion& quat, const double bias) const override;

		void CalculateInertiaTensor(double mass = 100.0) override;

		void CalculateBounds() override;

		float FastestLinearSpeed(const Vector3& angularVelocity, const Vector3& dir) const override;

	public:
		Vector3 mSize = { 1.0f, 1.0f, 1.0f };

		std::vector<Vector3> mPoints;
		
	};

	class ShapeTerrain : public Shape
	{
	public:
		ShapeTerrain(double maxAltitude = 1.0) : mMaxAltitude(maxAltitude) {}
		ShapeTerrain(double maxAltitude, std::string filePath) : mMaxAltitude(maxAltitude), mFilePath(filePath) {};

		ShapeType GetType() const override { return ShapeType::TERRAIN; }

		void CalculateInertiaTensor(double mass) override;

		Vector3 Support(Vector3& dir, const Vector3& pos, const Quaternion& quat, const double bias) const override;

		void CalculateBounds() override;
	public:
		double mMaxAltitude;

		std::string mFilePath;
	};

	class ShapeTerrainFace : public Shape
	{
	public:
		ShapeTerrainFace(double maxAltitude = 1.0){};
		ShapeTerrainFace(double maxAltitude, std::string filePath){};
		ShapeTerrainFace(Vector3 p1, Vector3 p2, Vector3 p3, Vector3 p4, Vector3 p5, Vector3 p6);

		ShapeType GetType() const override { return ShapeType::TERRAIN; }

		void CalculateInertiaTensor(double mass) override;

		Vector3 Support(Vector3& dir, const Vector3& pos, const Quaternion& quat, const double bias) const override;

		void CalculateBounds() override;
	public:
		std::vector<Vector3> mPoints;
	};

}