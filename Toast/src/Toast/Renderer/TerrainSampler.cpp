#include "tpch.h"
#include "TerrainSampler.h"

namespace Toast {

	CubeSampleCPU DirectionToCube(const Vector3& vIn)
	{
		using namespace DirectX;
		Vector3 v = Vector3::Normalize(vIn);

		double ax = fabs(v.x);
		double ay = fabs(v.y);
		double az = fabs(v.z);

		uint32_t face;
		Vector2 uvFace;
		if (ax >= ay && ax >= az)
		{
			if (v.x > 0.0f) { face = 0; uvFace = { -v.z / ax,  v.y / ax }; }
			else { face = 1; uvFace = { v.z / ax,  v.y / ax }; }
		}
		else if (ay >= ax && ay >= az)
		{
			if (v.y > 0.0f) { face = 2; uvFace = { v.x / ay, -v.z / ay }; }
			else { face = 3; uvFace = { v.x / ay,  v.z / ay }; }
		}
		else
		{
			if (v.z > 0.0f) { face = 4; uvFace = { v.x / az,  v.y / az }; }
			else { face = 5; uvFace = { -v.x / az,  v.y / az }; }
		}

		CubeSampleCPU cs;
		cs.face = face;
		cs.u = 0.5 * uvFace.x + 0.5;
		cs.v = 0.5 * uvFace.y + 0.5;
		return cs;
	}

	Vector3 CubeFaceUVToDir(uint32_t face, double u, double v)
	{
		double px = 2.0 * u - 1.0;
		double py = 2.0 * (1.0 - v) - 1.0;

		double dx, dy, dz;
		switch (face)
		{
		case 0: dx = 1.0; dy = py;  dz = -px; break;
		case 1: dx = -1.0; dy = py;  dz = px; break;
		case 2: dx = px;  dy = 1.0; dz = -py; break;
		case 3: dx = px;  dy = -1.0; dz = py; break;
		case 4: dx = px;  dy = py;  dz = 1.0; break;
		default: dx = -px; dy = py;  dz = -1.0; break;
		}
		return Vector3::Normalize(Vector3(dx, dy, dz));
	}

	CubeSampleCPU RemapFaceUV(uint32_t face, double u, double v)
	{
		if (u >= 0.0 && u <= 1.0 && v >= 0.0 && v <= 1.0)
			return CubeSampleCPU{ face, u, v };

		auto dir = CubeFaceUVToDir(face, u, v);
		return DirectionToCube(dir);
	}

	float SampleCubeBilinear(const CubeData<float>& td, const Vector3& dirIn)
	{
		CubeSampleCPU cs = DirectionToCube(dirIn);

		uint32_t face = cs.face;
		double u = cs.u;
		double v = cs.v;

		uint32_t W = td.Width;
		uint32_t H = td.Height;

		double px = u * W - 0.5;
		double py = v * H - 0.5;

		int ix0 = (int)std::floor(px);
		int iy0 = (int)std::floor(py);
		int ix1 = ix0 + 1;
		int iy1 = iy0 + 1;

		double fx = px - std::floor(px);
		double fy = py - std::floor(py);

		auto uvFromIJ = [&](int ix, int iy)
			{
				double uu = (static_cast<double>(ix) + 0.5) / static_cast<double>(W);
				double vv = (static_cast<double>(iy) + 0.5) / static_cast<double>(H);
				return std::pair<double, double>(uu, vv);
			};

		auto [u00, v00_uv] = uvFromIJ(ix0, iy0);
		auto [u10, v10_uv] = uvFromIJ(ix1, iy0);
		auto [u01, v01_uv] = uvFromIJ(ix0, iy1);
		auto [u11, v11_uv] = uvFromIJ(ix1, iy1);

		CubeSampleCPU c00 = RemapFaceUV(face, u00, v00_uv);
		CubeSampleCPU c10 = RemapFaceUV(face, u10, v10_uv);
		CubeSampleCPU c01 = RemapFaceUV(face, u01, v01_uv);
		CubeSampleCPU c11 = RemapFaceUV(face, u11, v11_uv);

		auto clampIJ = [&](const CubeSampleCPU& c) -> std::pair<uint32_t, uint32_t>
			{
				double x = c.u * W;
				double y = c.v * H;
				int ix = std::clamp((int)x, 0, (int)W - 1);
				int iy = std::clamp((int)y, 0, (int)H - 1);
				return { (uint32_t)ix, (uint32_t)iy };
			};

		auto [i00x, i00y] = clampIJ(c00);
		auto [i10x, i10y] = clampIJ(c10);
		auto [i01x, i01y] = clampIJ(c01);
		auto [i11x, i11y] = clampIJ(c11);

		const auto& f00 = td.FaceData[c00.face];
		const auto& f10 = td.FaceData[c10.face];
		const auto& f01 = td.FaceData[c01.face];
		const auto& f11 = td.FaceData[c11.face];

		double h00 = f00[Index2D(i00x, i00y, W)];
		double h10 = f10[Index2D(i10x, i10y, W)];
		double h01 = f01[Index2D(i01x, i01y, W)];
		double h11 = f11[Index2D(i11x, i11y, W)];

		double vx0 = h00 + (h10 - h00) * fx;
		double vx1 = h01 + (h11 - h01) * fx;
		double vFinal = vx0 + (vx1 - vx0) * fy;

		return (float)vFinal;
	}

	float SampleHeightFromDir(const CubeData<float>& td, const Vector3& dirPlanet)
	{
		return SampleCubeBilinear(td, dirPlanet);
	}

	float SampleHeightNearest(const CubeData<float>& td, const Vector3& dirIn)
	{
		CubeSampleCPU cs = DirectionToCube(dirIn);
		uint32_t W = td.Width;
		uint32_t H = td.Height;

		uint32_t ix = std::clamp((uint32_t)(cs.u * W), 0u, W - 1);
		uint32_t iy = std::clamp((uint32_t)(cs.v * H), 0u, H - 1);

		return (float)td.FaceData[cs.face][iy * W + ix];
	}

	float SampleColorAvgFromDir(const CubeData<uint32_t>& td, const Vector3& dirPlanet)
	{
		CubeSampleCPU cs = DirectionToCube(dirPlanet);

		int ix = std::clamp((int)(cs.u * td.Width), 0, (int)td.Width - 1);
		int iy = std::clamp((int)(cs.v * td.Height), 0, (int)td.Height - 1);

		uint32_t texel = td.FaceData[cs.face][Index2D(ix, iy, td.Width)];

		uint8_t rByte = (uint8_t)(texel & 0xFF);
		uint8_t gByte = (uint8_t)((texel >> 8) & 0xFF);
		uint8_t bByte = (uint8_t)((texel >> 16) & 0xFF);

		// Match GPU: just divide by 255, no sRGB conversion
		float r = rByte / 255.0f;
		float g = gByte / 255.0f;
		float b = bByte / 255.0f;

		return (r + g + b) / 3.0f;
	}

}