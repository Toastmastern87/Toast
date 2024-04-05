#pragma once

#include <initializer_list>

#include "Toast/Core/Math/Quaternion.h"

namespace Toast {

	class Vector3
	{
	public:
		Vector3() = default;
		Vector3(float xIn, float yIn, float zIn, float wIn = 1.0f);
		Vector3(double xIn, double yIn, double zIn, double wIn = 1.0f) : x(xIn), y(yIn), z(zIn), w(wIn) {}
		Vector3(DirectX::XMVECTOR vec);
		Vector3(std::initializer_list<double> list);

		Vector3(DirectX::XMFLOAT3 vec);

		Vector3 operator+(const Vector3& rhs) const {
			return Vector3(x + rhs.x, y + rhs.y, z + rhs.z);
		}

		Vector3& operator+=(const Vector3& rhs) {
			x += rhs.x;
			y += rhs.y;
			z += rhs.z;
			return *this;
		}

		bool operator==(const Vector3& rhs) const {
			return x == rhs.x && y == rhs.y && z == rhs.z;
		}

		Vector3 operator-(const Vector3& rhs) const {
			return Vector3(x - rhs.x, y - rhs.y, z - rhs.z);
		}

		Vector3 operator*(double scalar) const {
			return Vector3(x * scalar, y * scalar, z * scalar, w);
		}

		Vector3 operator*=(double scalar) const {
			return Vector3(x * scalar, y * scalar, z * scalar, w);
		}

		Vector3 operator/(double scalar) const {
			return Vector3(x / scalar, y / scalar, z / scalar, w);
		}

		double Magnitude() const;

		static Vector3 Normalize(std::initializer_list<double> list);
		static Vector3 Normalize(const Vector3& vec);

		Vector3 HomogeneousNormalize() const;

		static Vector3 Cross(const Vector3& a, const Vector3& b);

		static double Dot(const Vector3& a, const Vector3& b);

		static Vector3 Rotate(const Vector3 vec, const Quaternion& q);

		void ToString();
	public:
		double x, y, z, w;
	};

	class Vector2
	{
	public:
		Vector2() = default;
		Vector2(float xIn, float yIn = 1.0f);
		Vector2(double xIn, double yIn, double zIn, double wIn = 1.0f) : x(xIn), y(yIn) {}
		Vector2(DirectX::XMVECTOR vec);
		Vector2(std::initializer_list<double> list);

		Vector2(DirectX::XMFLOAT2 vec);

		Vector2 operator+(const Vector3& rhs) const {
			return Vector2(x + rhs.x, y + rhs.y);
		}

		Vector2& operator+=(const Vector2& rhs) {
			x += rhs.x;
			y += rhs.y;
			return *this;
		}

		bool operator==(const Vector2& rhs) const {
			return x == rhs.x && y == rhs.y;
		}

		Vector2 operator-(const Vector2& rhs) const {
			return Vector2(x - rhs.x, y - rhs.y);
		}

		Vector2 operator*(double scalar) const {
			return Vector2(x * scalar, y * scalar);
		}

		Vector2 operator*=(double scalar) const {
			return Vector2(x * scalar, y * scalar);
		}

		Vector2 operator/(double scalar) const {
			return Vector2(x / scalar, y / scalar);
		}

		double Magnitude() const;

		static Vector2 Normalize(std::initializer_list<double> list);
		static Vector2 Normalize(Vector2& vec);

		static Vector2 Cross(const Vector2& a, const Vector2& b);

		static double Dot(const Vector2& a, const Vector2& b);

		void ToString();
	public:
		double x, y;
	};

}