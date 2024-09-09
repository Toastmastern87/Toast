#pragma once

#include <initializer_list>

#include "Toast/Core/Math/Quaternion.h"

namespace Toast {

	class Vector3
	{
	public:
		Vector3();
		Vector3(float xIn, float yIn, float zIn, float wIn = 1.0f);
		Vector3(double xIn, double yIn, double zIn, double wIn = 1.0f) : x(xIn), y(yIn), z(zIn), w(wIn) {}
		Vector3(DirectX::XMVECTOR vec);
		Vector3(std::initializer_list<double> list);
		Vector3(DirectX::XMFLOAT3 vec);

		// Nested Hasher
		struct Hasher {
			std::size_t operator()(const Vector3& v) const {
				std::size_t hx = std::hash<double>()(v.x);
				std::size_t hy = std::hash<double>()(v.y);
				std::size_t hz = std::hash<double>()(v.z);

				return hx ^ (hy << 1) ^ (hz << 2) ^ (hx >> 2) ^ (hy >> 1);
			}
		};

		// Nested Equal
		struct Equal {
			bool operator()(const Vector3& v1, const Vector3& v2) const {
				constexpr double epsilon = 1e-6;
				return (std::abs(v1.x - v2.x) < epsilon) &&
					(std::abs(v1.y - v2.y) < epsilon) &&
					(std::abs(v1.z - v2.z) < epsilon);
			}
		};

		Vector3 operator+(const Vector3& rhs) const {
			return Vector3(x + rhs.x, y + rhs.y, z + rhs.z);
		}

		Vector3 operator-() const {
			return Vector3(-x, -y, -z, w);
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

		Vector3 operator/=(double scalar) const {
			return Vector3(x / scalar, y / scalar, z / scalar, w);
		}

		double Magnitude() const;
		static double Magnitude(Vector3 vec);
		double MagnitudeSqrt() const;

		static Vector3 Normalize(std::initializer_list<double> list);
		static Vector3 Normalize(const Vector3& vec);

		Vector3 HomogeneousNormalize() const;

		static Vector3 Cross(const Vector3& a, const Vector3& b);

		static double Dot(const Vector3& a, const Vector3& b);

		static Vector3 Rotate(const Vector3 vec, const Quaternion& q);

		void ToString();
		void ToString(const std::string& label);
		void ToString(const std::string& label) const;
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
		Vector2(std::initializer_list<uint32_t> list);

		Vector2(DirectX::XMFLOAT2 vec);

		Vector2 operator+(const Vector2& rhs) const {
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
		void ToString(const std::string& label);
	public:
		double x, y;
	};

}