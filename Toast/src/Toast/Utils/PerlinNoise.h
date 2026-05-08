#pragma once
#include <cstdint>
#include <cmath>
#include <algorithm>

#include "Toast/Core/Math/Vector.h"

// Minimal helper. Replace with your own if you prefer.
static inline float Toast_Lerp(float a, float b, float t) { return a + t * (b - a); }
static inline float Toast_Fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
static inline float Toast_Frac(float x) { return x - std::floor(x); }

// IMPORTANT: HLSL floor() semantics for negatives is std::floor().
static inline int Toast_FloorToInt(float x) { return (int)std::floor((double)x); }

// Your GPU uses a 256 permutation array. On CPU you already store Perm[256] per detail.
namespace Toast {

	struct NoiseVec2
	{
		float x = 0.0f;
		float y = 0.0f;

		NoiseVec2() = default;
		NoiseVec2(float vx, float vy) : x(vx), y(vy) {}

		NoiseVec2 operator+(const NoiseVec2& r) const { return { x + r.x, y + r.y }; }
		NoiseVec2 operator-(const NoiseVec2& r) const { return { x - r.x, y - r.y }; }
		NoiseVec2 operator*(const NoiseVec2& r) const { return { x * r.x, y * r.y }; }
		NoiseVec2 operator*(float s) const { return { x * s, y * s }; }
		NoiseVec2 operator/(float s) const { return { x / s, y / s }; }

		NoiseVec2& operator+=(const NoiseVec2& r)
		{
			x += r.x;
			y += r.y;
			return *this;
		}
	};

	static inline NoiseVec2 operator*(float s, const NoiseVec2& v)
	{
		return { v.x * s, v.y * s };
	}

	static inline float Dot(const NoiseVec2& a, const NoiseVec2& b)
	{
		return a.x * b.x + a.y * b.y;
	}

	static inline float Length(const NoiseVec2& v)
	{
		return std::sqrt(Dot(v, v));
	}

	static inline NoiseVec2 Floor(const NoiseVec2& v)
	{
		return { std::floor(v.x), std::floor(v.y) };
	}

	static inline NoiseVec2 Frac(const NoiseVec2& v)
	{
		return { Toast_Frac(v.x), Toast_Frac(v.y) };
	}

	static inline NoiseVec2 SafeNormalize2(const NoiseVec2& v)
	{
		float len = Length(v);
		return (len > 1e-10f) ? (v / len) : NoiseVec2(0.0f, 0.0f);
	}

	struct ToastPerlin3D
	{
		// Returns Perm[i & 255]
		static inline int PermAt(const int* perm256, int i)
		{
			return perm256[i & 255];
		}

		// classic Perlin hashing: perm[x + perm[y + perm[z]]]
		static inline int Hash3(const int* perm256, int x, int y, int z)
		{
			int a = PermAt(perm256, x);
			int b = PermAt(perm256, (a + y));
			return PermAt(perm256, (b + z));
		}

		// Matches your HLSL Grad3 exactly.
		static inline float Grad3(int hash, float x, float y, float z)
		{
			int h = hash & 15;
			float u = (h < 8) ? x : y;
			float v = (h < 4) ? y : ((h == 12 || h == 14) ? x : z);
			return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
		}

		// 3D Perlin in ~[-1,1] (same as your HLSL)
		static inline float Perlin3D(const int* perm256, float x, float y, float z)
		{
			int xi = Toast_FloorToInt(x) & 255;
			int yi = Toast_FloorToInt(y) & 255;
			int zi = Toast_FloorToInt(z) & 255;

			float xf = x - (float)Toast_FloorToInt(x);
			float yf = y - (float)Toast_FloorToInt(y);
			float zf = z - (float)Toast_FloorToInt(z);

			float u = Toast_Fade(xf);
			float v = Toast_Fade(yf);
			float w = Toast_Fade(zf);

			int xi1 = (xi + 1) & 255;
			int yi1 = (yi + 1) & 255;
			int zi1 = (zi + 1) & 255;

			int aaa = Hash3(perm256, xi, yi, zi);
			int aba = Hash3(perm256, xi, yi1, zi);
			int aab = Hash3(perm256, xi, yi, zi1);
			int abb = Hash3(perm256, xi, yi1, zi1);

			int baa = Hash3(perm256, xi1, yi, zi);
			int bba = Hash3(perm256, xi1, yi1, zi);
			int bab = Hash3(perm256, xi1, yi, zi1);
			int bbb = Hash3(perm256, xi1, yi1, zi1);

			float x00 = Toast_Lerp(Grad3(aaa, xf, yf, zf),
				Grad3(baa, xf - 1, yf, zf), u);
			float x10 = Toast_Lerp(Grad3(aba, xf, yf - 1, zf),
				Grad3(bba, xf - 1, yf - 1, zf), u);
			float x01 = Toast_Lerp(Grad3(aab, xf, yf, zf - 1),
				Grad3(bab, xf - 1, yf, zf - 1), u);
			float x11 = Toast_Lerp(Grad3(abb, xf, yf - 1, zf - 1),
				Grad3(bbb, xf - 1, yf - 1, zf - 1), u);

			float y0 = Toast_Lerp(x00, x10, v);
			float y1 = Toast_Lerp(x01, x11, v);

			return Toast_Lerp(y0, y1, w);
		}
	};

	// Fractal wrapper matching the HLSL one.
	static inline float FractalPerlin3D(const int* perm256, float px, float py, float pz, int octaves, float baseFreq, float baseAmp, float lacunarity, float persistence)
	{
		float result = 0.0f;
		float frequency = baseFreq;
		float amplitude = baseAmp;

		for (int i = 0; i < octaves; ++i)
		{
			result += ToastPerlin3D::Perlin3D(perm256, px * frequency, py * frequency, pz * frequency ) * amplitude;

			frequency *= lacunarity;
			amplitude *= persistence;
		}

		return result;
	}

	static inline float RidgedPerlin3D(const int* perm256, float px, float py, float pz, int octaves, float baseFreq, float baseAmp, float lacunarity, float persistence, float sharpness)
	{
		float result = 0.0f;
		float frequency = baseFreq;
		float amplitude = baseAmp;

		for (int i = 0; i < octaves; ++i)
		{
			float n = ToastPerlin3D::Perlin3D(
				perm256,
				px * frequency,
				py * frequency,
				pz * frequency
			);

			n = 1.0f - std::abs(n);
			n = std::pow(n, sharpness);

			result += n * amplitude;

			frequency *= lacunarity;
			amplitude *= persistence;
		}

		return result;
	}

	static inline float TurbulencePerlin3D(const int* perm256, float px, float py, float pz, int octaves, float baseFreq, float baseAmp, float lacunarity, float persistence)
	{
		float result = 0.0f;
		float frequency = baseFreq;
		float amplitude = baseAmp;

		for (int i = 0; i < octaves; ++i)
		{
			float n = ToastPerlin3D::Perlin3D(
				perm256,
				px * frequency,
				py * frequency,
				pz * frequency
			);

			result += std::abs(n) * amplitude;

			frequency *= lacunarity;
			amplitude *= persistence;
		}

		return result;
	}

	static inline float Voronoi3D(const int* perm256, float px, float py, float pz, int octaves, float baseFreq, float baseAmp, float lacunarity, float persistence)
	{
		float result = 0.0f;
		float frequency = baseFreq;
		float amplitude = baseAmp;

		for (int octave = 0; octave < octaves; ++octave)
		{
			float sx = px * frequency;
			float sy = py * frequency;
			float sz = pz * frequency;

			float cellIx = std::floor(sx);
			float cellIy = std::floor(sy);
			float cellIz = std::floor(sz);

			float cellFx = sx - cellIx;
			float cellFy = sy - cellIy;
			float cellFz = sz - cellIz;

			float F1 = 8.0f;
			float F2 = 8.0f;

			for (int dz = -1; dz <= 1; ++dz)
			{
				for (int dy = -1; dy <= 1; ++dy)
				{
					for (int dx = -1; dx <= 1; ++dx)
					{
						int hx = static_cast<int>(cellIx + static_cast<float>(dx)) & 255;
						int hy = static_cast<int>(cellIy + static_cast<float>(dy)) & 255;
						int hz = static_cast<int>(cellIz + static_cast<float>(dz)) & 255;

						int hashI = ToastPerlin3D::Hash3(perm256, hx, hy, hz);

						float rx = Toast_Frac(static_cast<float>(hashI) * 0.1031f);
						float ry = Toast_Frac(static_cast<float>(hashI) * 0.1030f);
						float rz = Toast_Frac(static_cast<float>(hashI) * 0.0973f);

						float diffX = static_cast<float>(dx) + rx - cellFx;
						float diffY = static_cast<float>(dy) + ry - cellFy;
						float diffZ = static_cast<float>(dz) + rz - cellFz;

						float d = diffX * diffX + diffY * diffY + diffZ * diffZ;

						if (d < F1)
						{
							F2 = F1;
							F1 = d;
						}
						else if (d < F2)
						{
							F2 = d;
						}
					}
				}
			}

			float edge = std::sqrt(F2) - std::sqrt(F1);
			result += edge * amplitude;

			frequency *= lacunarity;
			amplitude *= persistence;
		}

		return result;
	}

	// ------------------------------------------------------------
	// Phacelle noise matching GPU side
	// ------------------------------------------------------------

	struct PhacelleSampleCPU
	{
		float c = 0.0f;        // cosine / height component
		float s = 0.0f;        // sine / derivative phase component
		NoiseVec2 sideDir;     // derivative direction in input coordinate space
	};

	static inline uint32_t HashU_CPU(uint32_t x, uint32_t y)
	{
		uint32_t h = x * 0x27d4eb2du;
		h ^= h >> 15;
		h *= 0x85ebca6bu;
		h ^= y;
		h *= 0x27d4eb2du;
		h ^= h >> 13;
		h *= 0xc2b2ae35u;
		h ^= h >> 16;
		return h;
	}

	static inline NoiseVec2 Hash22Stable(const NoiseVec2& p)
	{
		int32_t ipX = (int32_t)std::floor(p.x) + 100000;
		int32_t ipY = (int32_t)std::floor(p.y) + 100000;
		uint32_t x = (uint32_t)ipX;
		uint32_t y = (uint32_t)ipY;

		uint32_t h1 = HashU_CPU(x, y);
		uint32_t h2 = HashU_CPU(x ^ 0x7ed55d16u, y ^ 0xc761c23cu);

		return NoiseVec2(
			(float)(h1 & 0xFFFFFFu) / 16777216.0f,
			(float)(h2 & 0xFFFFFFu) / 16777216.0f
		);
	}

	static inline PhacelleSampleCPU PhacelleNoise(const NoiseVec2& p, const NoiseVec2& normDir, float cellScale, float offset, float normalization)
	{
		PhacelleSampleCPU result;

		constexpr float TAU = 6.28318530718f;

		// HLSL:
		// float2 sideDir = normDir.yx * float2(-1.0, 1.0) * cellScale * TAU;
		NoiseVec2 sideDir(
			normDir.y * -1.0f,
			normDir.x * 1.0f
		);

		sideDir = sideDir * (cellScale * TAU);
		offset *= TAU;

		NoiseVec2 pInt = Floor(p);
		NoiseVec2 pFrac = Frac(p);

		NoiseVec2 phaseDir(0.0f, 0.0f);
		float weightSum = 0.0f;

		for (int i = -1; i <= 2; ++i)
		{
			for (int j = -1; j <= 2; ++j)
			{
				NoiseVec2 gridOffset(static_cast<float>(i), static_cast<float>(j));
				NoiseVec2 gridPoint = pInt + gridOffset;

				// HLSL currently uses Hash22Stable(gridPoint) - 0.5
				NoiseVec2 randomOffset = Hash22Stable(gridPoint) - NoiseVec2(0.5f, 0.5f);

				NoiseVec2 vectorFromCellPoint = pFrac - gridOffset - randomOffset;

				float sqrDist = Dot(vectorFromCellPoint, vectorFromCellPoint);

				float weight = std::exp(-sqrDist * 2.0f);
				weight = std::max(0.0f, weight - 0.01111f);

				weightSum += weight;

				float waveInput = Dot(vectorFromCellPoint, sideDir) + offset;

				phaseDir += NoiseVec2(std::cos(waveInput), std::sin(waveInput)) * weight;
			}
		}

		NoiseVec2 interpolated = phaseDir / std::max(weightSum, 0.0001f);

		float magnitude = std::sqrt(Dot(interpolated, interpolated));
		magnitude = std::max(1.0f - normalization, magnitude);

		result.c = interpolated.x / magnitude;
		result.s = interpolated.y / magnitude;
		result.sideDir = sideDir;

		return result;
	}

}