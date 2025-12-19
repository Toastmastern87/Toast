#pragma once
#include <cstdint>
#include <cmath>
#include <algorithm>

#include "Toast/Core/Math/Vector.h"

// Minimal helper. Replace with your own if you prefer.
static inline float Toast_Lerp(float a, float b, float t) { return a + t * (b - a); }
static inline float Toast_Fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

// IMPORTANT: HLSL floor() semantics for negatives is std::floor().
static inline int Toast_FloorToInt(float x) { return (int)std::floor((double)x); }

// Your GPU uses a 256 permutation array. On CPU you already store Perm[256] per detail.
namespace Toast {

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

	// Fractal wrapper matching your HLSL.
	static inline float FractalPerlin3D(const int* perm256, float px, float py, float pz, int octaves, float baseFreq, float baseAmp)
	{
		float result = 0.0f;
		float frequency = baseFreq;
		float amplitude = baseAmp;

		for (int i = 0; i < octaves; ++i)
		{
			result += ToastPerlin3D::Perlin3D(perm256, px * frequency, py * frequency, pz * frequency) * amplitude;
			frequency *= 2.0f;
			amplitude *= 0.5f;
		}

		return result;
	}

}