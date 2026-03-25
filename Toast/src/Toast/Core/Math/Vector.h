#pragma once

#include <initializer_list>

#include "Toast/Core/Math/Quaternion.h"

namespace Toast {

	class Vector3
	{
	public:
		Vector3();
		Vector3(float xIn, float yIn, float zIn, float wIn = 1.0f)
			: x((double)xIn), y((double)yIn), z((double)zIn), w((double)wIn) {}
		Vector3(double xIn, double yIn, double zIn, double wIn = 1.0f) 
			: x(xIn), y(yIn), z(zIn), w(wIn) {}
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

		Vector3 operator-=(const Vector3& rhs) {
			x -= rhs.x; y -= rhs.y; z -= rhs.z;
			return *this;
		}

		Vector3 operator*(double scalar) const {
			return Vector3(x * scalar, y * scalar, z * scalar, w);
		}

		friend Vector3 operator*(double scalar, const Vector3& v) {
			return Vector3(v.x * scalar, v.y * scalar, v.z * scalar, v.w);
		}

		Vector3& operator*=(double scalar) {
			x *= scalar; y *= scalar; z *= scalar;
			return *this;
		}

		Vector3 operator/(double scalar) const {
			return Vector3(x / scalar, y / scalar, z / scalar, w);
		}

		Vector3& operator/=(double scalar) {
			x /= scalar; y /= scalar; z /= scalar;
			return *this;
		}

		inline bool operator<(const Vector3& rhs) noexcept
		{
			return (x < rhs.x) || (x == rhs.x && y < rhs.y) || (x == rhs.x && y == rhs.y && z < rhs.z);
		}

		static Vector3 Min(const Vector3& a, const Vector3& b) noexcept
		{
			return { (std::min)(a.x, b.x),
					 (std::min)(a.y, b.y),
					 (std::min)(a.z, b.z) };
		}

		static Vector3 Max(const Vector3& a, const Vector3& b) noexcept
		{
			return { (std::max)(a.x, b.x),
					 (std::max)(a.y, b.y),
					 (std::max)(a.z, b.z) };
		}

		double Length() const;
		static double Length(const Vector3& vec);
		double LengthSquared() const;
		static double LengthSquared(const Vector3& vec);

		static Vector3 Normalize(std::initializer_list<double> list);
		static Vector3 Normalize(const Vector3& vec);

		Vector3 HomogeneousNormalize() const;

		static Vector3 Cross(const Vector3& a, const Vector3& b);

		static double Dot(const Vector3& a, const Vector3& b);

		static Vector3 Rotate(const Vector3 v, const Quaternion& qUnit);

		void ToString();
		void ToString(const std::string& label);
		void ToString(const std::string& label) const;
	public:
		double x, y, z, w;
	};

	////////////////////////////////////////////////////////////////////////////////////////
	//      VECTOR3 SIMD – AVX2 implementation (double precision)                         //
	////////////////////////////////////////////////////////////////////////////////////////	

#ifdef __AVX2__ 

	class Vector3AVX2
	{
	public:
		Vector3AVX2() : mV(_mm256_setr_pd(0.0, 0.0, 0.0, 1.0)) {}
		explicit Vector3AVX2(double x, double y, double z, double w = 1.0)
			: mV(_mm256_setr_pd(x, y, z, w)) {}
		explicit Vector3AVX2(const Vector3& s)
			: mV(_mm256_setr_pd(s.x, s.y, s.z, s.w)) {}
		explicit Vector3AVX2(__m256d raw) : mV(raw) {}

		// Load and Store functions
		inline Vector3 ToVector3() const
		{
			alignas(32) double tmp[4];
			_mm256_store_pd(tmp, mV);        // needs 32-byte alignment
			return { tmp[0], tmp[1], tmp[2], tmp[3] };
		}

		static inline Vector3AVX2 Load(const double* p){ return Vector3AVX2(_mm256_loadu_pd(p)); }
		inline void Store(double* p) const{	_mm256_storeu_pd(p, mV); }

		// Simple mathematical Operations
		friend inline Vector3AVX2 operator+(Vector3AVX2 a, Vector3AVX2 b)
		{
			return Vector3AVX2(_mm256_add_pd(a.mV, b.mV));
		}

		friend inline Vector3AVX2 operator-(Vector3AVX2 a, Vector3AVX2 b)
		{
			return Vector3AVX2(_mm256_sub_pd(a.mV, b.mV));
		}

		friend inline Vector3AVX2 operator*(Vector3AVX2 a, double s)
		{
			return Vector3AVX2(_mm256_mul_pd(a.mV, _mm256_set1_pd(s)));
		}

		friend inline Vector3AVX2 operator/(Vector3AVX2 a, double s)
		{
			return Vector3AVX2(_mm256_div_pd(a.mV, _mm256_set1_pd(s)));
		}

		// Advanced mathematical operations
		inline double Dot(const Vector3AVX2& b) const
		{
			__m256d mul = _mm256_mul_pd(mV, b.mV);                      
			mul = _mm256_blend_pd(mul, _mm256_setzero_pd(), 0b1000); 
			__m128d high = _mm256_extractf128_pd(mul, 1);            
			__m128d low = _mm256_castpd256_pd128(mul);                
			__m128d sum = _mm_add_pd(low, high);                     
			sum = _mm_hadd_pd(sum, sum);                      
			return _mm_cvtsd_f64(sum);
		}

		static inline Vector3AVX2 Cross(Vector3AVX2 a, Vector3AVX2 b)
		{
			// a.y, a.z, a.x, w   and   b.z, b.x, b.y, w
			const __m256d a_yzx = _mm256_permute4x64_pd(a.mV, _MM_SHUFFLE(3, 0, 2, 1));
			const __m256d b_zxy = _mm256_permute4x64_pd(b.mV, _MM_SHUFFLE(3, 1, 0, 2));

			// a.z, a.x, a.y, w   and   b.y, b.z, b.x, w
			const __m256d a_zxy = _mm256_permute4x64_pd(a.mV, _MM_SHUFFLE(3, 1, 0, 2));
			const __m256d b_yzx = _mm256_permute4x64_pd(b.mV, _MM_SHUFFLE(3, 0, 2, 1));

			__m256d res = _mm256_sub_pd(_mm256_mul_pd(a_yzx, b_zxy),
				_mm256_mul_pd(a_zxy, b_yzx));

			// force w = 1.0 to preserve homogeneous coords
			res = _mm256_blend_pd(res, _mm256_setr_pd(0.0, 0.0, 0.0, 1.0), 0b1000);
			return Vector3AVX2(res);
		}

		inline double Length() const { return std::sqrt(Dot(*this)); }

		inline Vector3AVX2 Normalised() const
		{
			// 1. x² y² z² w²   (w lane is ignored later)
			__m256d sq = _mm256_mul_pd(mV, mV);

			// 2. zero out w lane so it doesn't pollute the dot
			sq = _mm256_blend_pd(sq, _mm256_setzero_pd(), 0b1000);

			// 3. horizontal add (x²+y²) in lo128 , (z²+0) in hi128
			__m128d lo128 = _mm256_castpd256_pd128(sq);          // lanes 0,1
			__m128d hi128 = _mm256_extractf128_pd(sq, 1);        // lanes 2,3
			__m128d dot2 = _mm_add_pd(lo128, hi128);            // {x²+z² , y²}

			double len2 = _mm_cvtsd_f64(dot2)                  // x²+z²
				+ _mm_cvtsd_f64(_mm_unpackhi_pd(dot2, dot2)); // +y²

			// 4. convert to float and do fast rsqrt
			float len2f = static_cast<float>(len2);
			__m128 lenF = _mm_set_ss(len2f);
			__m128 rsqrtF = _mm_rsqrt_ss(lenF);                  // 1/√len² (approx)

			// Optional Newton–Raphson step for better accuracy
			rsqrtF = _mm_mul_ss(rsqrtF,
				_mm_sub_ss(_mm_set_ss(1.5f),
					_mm_mul_ss(_mm_mul_ss(rsqrtF, rsqrtF),
						_mm_mul_ss(lenF, _mm_set_ss(0.5f)))));

			double invLen = _mm_cvtss_f32(rsqrtF);               // back to double

			// 5. scale original vector
			__m256d inv = _mm256_set1_pd(invLen);
			return Vector3AVX2(_mm256_mul_pd(mV, inv));
		}

	private:
		__m256d mV;
	};

#else   
	#pragma message("Vector3AVX2 disabled – compile with AVX2 to enable.")
#endif

	////////////////////////////////////////////////////////////////////////////////////////
	//      VECTOR3x4 SIMD – AVX2 implementation (double precision)					      //
	//		Batching 4 vectors to eachother to optimize even more   	                  //
	////////////////////////////////////////////////////////////////////////////////////////	

#ifdef __AVX2__ 

	struct Vec3x4d                 // structure-of-arrays (4 doubles each lane)
	{
		__m256d x, y, z;           // 256-bit registers

		static Vec3x4d Load(const Vector3* p)          // gather 4 Vector3
		{
			return { _mm256_set_pd(p[3].x,p[2].x,p[1].x,p[0].x),
					 _mm256_set_pd(p[3].y,p[2].y,p[1].y,p[0].y),
					 _mm256_set_pd(p[3].z,p[2].z,p[1].z,p[0].z) };
		}

		static Vec3x4d Load(const Vector3& v0, const Vector3& v1, const Vector3& v2, const Vector3& v3)
		{
			return {
				_mm256_set_pd(v3.x,v2.x,v1.x,v0.x),
				_mm256_set_pd(v3.y,v2.y,v1.y,v0.y),
				_mm256_set_pd(v3.z,v2.z,v1.z,v0.z)
			};
		}

		void Store(Vector3* p) const                   // scatter back
		{
			alignas(32) double sx[4], sy[4], sz[4];
			_mm256_store_pd(sx, x);
			_mm256_store_pd(sy, y);
			_mm256_store_pd(sz, z);

			for (int i = 0; i < 4; ++i)
			{
				p[i].x = sx[i];
				p[i].y = sy[i];
				p[i].z = sz[i];
			}
		}

		inline Vec3x4d Normalize() const {
			// length² = x*x + y*y + z*z
			__m256d xsq = _mm256_mul_pd(x, x);
			__m256d ysq = _mm256_mul_pd(y, y);
			__m256d zsq = _mm256_mul_pd(z, z);
			__m256d sum = _mm256_add_pd(_mm256_add_pd(xsq, ysq), zsq);

			// len = sqrt(sum)
			__m256d len = _mm256_sqrt_pd(sum);
			// invLen = 1/len
			__m256d invLen = _mm256_div_pd(_mm256_set1_pd(1.0), len);

			return {
				_mm256_mul_pd(x, invLen),
				_mm256_mul_pd(y, invLen),
				_mm256_mul_pd(z, invLen)
			};
		}
	};

#else   
#pragma message("Vector3AVX2 disabled – compile with AVX2 to enable.")
#endif

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

		double Length() const;

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