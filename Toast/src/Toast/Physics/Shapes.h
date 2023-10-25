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
		virtual Matrix GetInertiaTensor() const = 0;

		virtual Vector3 Support(Vector3& dir, const Vector3& pos, const Quaternion& quat, const double bias) const = 0;

		virtual float FastestLinearSpeed(const Vector3& angularVelocity, const Vector3& dir) const { return 0.0f; }

		virtual Bounds GetBounds(const Vector3& pos, const Quaternion& quat) const = 0;
		virtual Bounds GetBounds() const = 0;

		virtual Vector3 GetCenterOfMass() const { return mCenterOfMass; }

	protected:
		DirectX::XMFLOAT3 mCenterOfMass;
	};

	class ShapeSphere : public Shape
	{
	public:
		ShapeSphere(double radius);

		ShapeType GetType() const override { return ShapeType::SPHERE; }
		Matrix GetInertiaTensor() const override;

		Vector3 Support(Vector3& dir, const Vector3& pos, const Quaternion& quat, const double bias) const override;

		Bounds GetBounds(const Vector3& pos, const Quaternion& quat) const override;
		Bounds GetBounds() const override;

	public:
		double mRadius;
	};

	class ShapeBox : public Shape
	{
	public:
		ShapeBox(Vector3 size) : mSize(size) {}
		explicit ShapeBox(const Vector3* pts, const int num) {}

		ShapeType GetType() const override { return ShapeType::BOX; }

		Vector3 Support(Vector3& dir, const Vector3& pos, const Quaternion& quat, const double bias) const override;

		Matrix GetInertiaTensor() const override;

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
		ShapeTerrain(std::string filePath, std::tuple<DirectX::TexMetadata, DirectX::ScratchImage*> terrainData) : FilePath(filePath), TerrainData(terrainData) {}

		ShapeType GetType() const override { return ShapeType::TERRAIN; }
		Matrix GetInertiaTensor() const override;

		Vector3 Support(Vector3& dir, const Vector3& pos, const Quaternion& quat, const double bias) const override;

		Bounds GetBounds(const Vector3& pos, const Quaternion& quat) const override;
		Bounds GetBounds() const override;

	public:
		std::string FilePath;
		std::tuple<DirectX::TexMetadata, DirectX::ScratchImage*> TerrainData;
	};

}