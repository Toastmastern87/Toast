#pragma once

#include "Toast/Core/Math.h"

#include <DirectXMath.h>
#include <vector>

using namespace DirectX;

namespace Toast {

	struct PrimitiveVertex
	{
		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT3 Normal;
		DirectX::XMFLOAT3 Tangent;
		DirectX::XMFLOAT3 Binormal;
		DirectX::XMFLOAT2 Texcoord;

		PrimitiveVertex(DirectX::XMFLOAT3 pos, DirectX::XMFLOAT3 nor, DirectX::XMFLOAT3 tan, DirectX::XMFLOAT3 bin, DirectX::XMFLOAT2 uv)
		{
			Position = pos;
			Normal = nor;
			Tangent = tan;
			Binormal = bin;
			Texcoord = uv;
		}

		PrimitiveVertex(DirectX::XMFLOAT3 pos)
		{
			Position = pos;
			Normal = { 0.0f, 0.0f, 0.0f };
			Tangent = { 0.0f, 0.0f, 0.0f };
			Binormal = { 0.0f, 0.0f, 0.0f };
			Texcoord = { 0.0f, 0.0f };
		}

		PrimitiveVertex()
		{
			Position = { 0.0f, 0.0f, 0.0f };
			Normal = { 0.0f, 0.0f, 0.0f };
			Tangent = { 0.0f, 0.0f, 0.0f };
			Binormal = { 0.0f, 0.0f, 0.0f };
			Texcoord = { 0.0f, 0.0f };
		}

		PrimitiveVertex operator+(const PrimitiveVertex& a)
		{
			return PrimitiveVertex({ this->Position.x + a.Position.x, this->Position.y + a.Position.y, this->Position.z + a.Position.z });
		}

		PrimitiveVertex operator*(const float factor)
		{
			return PrimitiveVertex({ this->Position.x * factor, this->Position.y * factor, this->Position.z * factor });
		}

		PrimitiveVertex operator/(const float factor)
		{
			return PrimitiveVertex({ this->Position.x / factor, this->Position.y / factor, this->Position.z / factor });
		}

		PrimitiveVertex& operator*=(const float factor)
		{
			this->Position.x *= factor;
			this->Position.y *= factor;
			this->Position.z *= factor;

			return *this;
		}

		PrimitiveVertex operator-(const PrimitiveVertex& a)
		{
			return PrimitiveVertex({ this->Position.x - a.Position.x, this->Position.y - a.Position.y, this->Position.z - a.Position.z });
		}
	};

	struct PrimitiveMesh
	{
		std::vector<PrimitiveVertex> Vertices;
		std::vector<uint32_t> Indices;
	};

	class Primitives
	{
	public:
		static PrimitiveMesh CreateCube() 
		{
			PrimitiveMesh cubeMesh;

			cubeMesh.Vertices.push_back(PrimitiveVertex({ -0.5f, 0.5f, -0.5f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.25f }));
			cubeMesh.Vertices.push_back(PrimitiveVertex({ -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.25f, 0.25f }));
			cubeMesh.Vertices.push_back(PrimitiveVertex({ 0.5f, 0.5f, -0.5f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.5f }));
			cubeMesh.Vertices.push_back(PrimitiveVertex({ 0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.25f, 0.5f }));
			cubeMesh.Vertices.push_back(PrimitiveVertex({ -0.5f, -0.5f, 0.5f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.5f, 0.25f }));
			cubeMesh.Vertices.push_back(PrimitiveVertex({ 0.5f, -0.5f, 0.5f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.5f, 0.5f }));
			cubeMesh.Vertices.push_back(PrimitiveVertex({ -0.5f, 0.5f, 0.5f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.75f, 0.25f }));
			cubeMesh.Vertices.push_back(PrimitiveVertex({ 0.5f, 0.5f, 0.5f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.75f, 0.5f }));
			cubeMesh.Vertices.push_back(PrimitiveVertex({ -0.5f, 0.5f, -0.5f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.25f }));
			cubeMesh.Vertices.push_back(PrimitiveVertex({ 0.5f, 0.5f, -0.5f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.5f }));
			cubeMesh.Vertices.push_back(PrimitiveVertex({ -0.5f, 0.5f, -0.5f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.25f, 0.0f }));
			cubeMesh.Vertices.push_back(PrimitiveVertex({ -0.5f, 0.5f, 0.5f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.5f, 0.0f }));
			cubeMesh.Vertices.push_back(PrimitiveVertex({ 0.5f, 0.5f, -0.5f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.25f, 0.75f }));
			cubeMesh.Vertices.push_back(PrimitiveVertex({ 0.5f, 0.5f, 0.5f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.5f, 0.75f }));

			cubeMesh.Indices = std::vector<uint32_t>{
				//Front
				0, 2, 1,
				1, 2, 3,

				//Back
				4, 5, 6,
				5, 7, 6,

				//Top
				6, 7, 8,
				7, 9, 8,

				//Bottom
				1, 3, 4,
				3, 5, 4,

				//Left
				1, 11, 10,
				1, 4, 11,

				//Right
				3, 12, 5,
				5, 12, 13
			};

			return cubeMesh;
		}

		static PrimitiveMesh CreateGrid(uint32_t gridSize = 5) 
		{
			PrimitiveMesh gridMesh;

			for (uint32_t x = 0; x <= gridSize; x++)
			{
				for (uint32_t z = 0; z <= gridSize; z++)
				{
					gridMesh.Vertices.push_back(PrimitiveVertex({ -((float)gridSize / 2.0f) + x, 0.0f, -((float)gridSize / 2.0f) + z }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f }));
				}
			}

			for (uint32_t x = 0; x < gridSize; x++)
			{
				for (uint32_t z = 0; z < gridSize; z++)
				{
					gridMesh.Indices.push_back(z + (uint32_t)1 + (gridSize + (uint32_t)1) * x);
					gridMesh.Indices.push_back(z + gridSize + (uint32_t)2 + (gridSize + (uint32_t)1) * x);
					gridMesh.Indices.push_back(z + (gridSize + (uint32_t)1) * x);

					gridMesh.Indices.push_back(z + gridSize + (uint32_t)2 + (gridSize + (uint32_t)1) * x);
					gridMesh.Indices.push_back(z + gridSize + (uint32_t)1 + (gridSize + (uint32_t)1) * x);
					gridMesh.Indices.push_back(z + (gridSize + (uint32_t)1) * x);
				}
			}

			return gridMesh;
		}

		static PrimitiveMesh CreateIcosphere(uint32_t subdivisions = 0)
		{
			PrimitiveMesh icosphereMesh;
			std::vector<PrimitiveVertex> startVertices;

			float ratio = (1.0f + sqrt(5.0f)) / 2.0f;
			float vectorLength = Math::GetVectorLength({ ratio, 0.0f, -1.0f });

			startVertices.push_back(PrimitiveVertex({ ratio, 0.0f, -1.0f }) / vectorLength * 0.5f);
			startVertices.push_back(PrimitiveVertex({ -ratio, 0.0f, -1.0f }) / vectorLength * 0.5f);
			startVertices.push_back(PrimitiveVertex({ ratio, 0.0f, 1.0f }) / vectorLength * 0.5f);
			startVertices.push_back(PrimitiveVertex({ -ratio, 0.0f, 1.0f }) / vectorLength * 0.5f);
			startVertices.push_back(PrimitiveVertex({ 0.0f, -1.0f, ratio }) / vectorLength * 0.5f);
			startVertices.push_back(PrimitiveVertex({ 0.0f, -1.0f, -ratio }) / vectorLength * 0.5f);
			startVertices.push_back(PrimitiveVertex({ 0.0f, 1.0f, ratio }) / vectorLength * 0.5f);
			startVertices.push_back(PrimitiveVertex({ 0.0f, 1.0f, -ratio }) / vectorLength * 0.5f);
			startVertices.push_back(PrimitiveVertex({ -1.0f, ratio, 0.0f }) / vectorLength * 0.5f);
			startVertices.push_back(PrimitiveVertex({ -1.0f, -ratio, 0.0f }) / vectorLength * 0.5f);
			startVertices.push_back(PrimitiveVertex({ 1.0f , ratio, 0.0f }) / vectorLength * 0.5f);
			startVertices.push_back(PrimitiveVertex({ 1.0f , -ratio, 0.0f }) / vectorLength * 0.5f);

			SplitTriangle(icosphereMesh, startVertices[1], startVertices[3], startVertices[8], 0, subdivisions);
			SplitTriangle(icosphereMesh, startVertices[3], startVertices[1], startVertices[9], 0, subdivisions);
			SplitTriangle(icosphereMesh, startVertices[2], startVertices[0], startVertices[10], 0, subdivisions);
			SplitTriangle(icosphereMesh, startVertices[0], startVertices[2], startVertices[11], 0, subdivisions);

			SplitTriangle(icosphereMesh, startVertices[5], startVertices[7], startVertices[0], 0, subdivisions);
			SplitTriangle(icosphereMesh, startVertices[7], startVertices[5], startVertices[1], 0, subdivisions);
			SplitTriangle(icosphereMesh, startVertices[6], startVertices[4], startVertices[2], 0, subdivisions);
			SplitTriangle(icosphereMesh, startVertices[4], startVertices[6], startVertices[3], 0, subdivisions);

			SplitTriangle(icosphereMesh, startVertices[9], startVertices[11], startVertices[4], 0, subdivisions);
			SplitTriangle(icosphereMesh, startVertices[11], startVertices[9], startVertices[5], 0, subdivisions);
			SplitTriangle(icosphereMesh, startVertices[10], startVertices[8], startVertices[6], 0, subdivisions);
			SplitTriangle(icosphereMesh, startVertices[8], startVertices[10], startVertices[7], 0, subdivisions);

			SplitTriangle(icosphereMesh, startVertices[7], startVertices[1], startVertices[8], 0, subdivisions);
			SplitTriangle(icosphereMesh, startVertices[1], startVertices[5], startVertices[9], 0, subdivisions);
			SplitTriangle(icosphereMesh, startVertices[0], startVertices[7], startVertices[10], 0, subdivisions);
			SplitTriangle(icosphereMesh, startVertices[5], startVertices[0], startVertices[11], 0, subdivisions);

			SplitTriangle(icosphereMesh, startVertices[3], startVertices[6], startVertices[8], 0, subdivisions);
			SplitTriangle(icosphereMesh, startVertices[4], startVertices[3], startVertices[9], 0, subdivisions);
			SplitTriangle(icosphereMesh, startVertices[6], startVertices[2], startVertices[10], 0, subdivisions);
			SplitTriangle(icosphereMesh, startVertices[2], startVertices[4], startVertices[11], 0, subdivisions);

			return icosphereMesh;
		}

		static void SplitTriangle(PrimitiveMesh& icosphereMesh, PrimitiveVertex a, PrimitiveVertex b, PrimitiveVertex c, int8_t subdivision, int8_t maxSubdivisions)
		{
			if (subdivision < maxSubdivisions)
			{
				int8_t nSubdivision = subdivision + 1;

				PrimitiveVertex A = b + ((c - b) * 0.5f);
				PrimitiveVertex B = c + ((a - c) * 0.5f);
				PrimitiveVertex C = a + ((b - a) * 0.5f);

				A *= 0.5f / Math::GetVectorLength({ A.Position.x, A.Position.y, A.Position.z });
				B *= 0.5f / Math::GetVectorLength({ B.Position.x, B.Position.y, B.Position.z });
				C *= 0.5f / Math::GetVectorLength({ C.Position.x, C.Position.y, C.Position.z });

				SplitTriangle(icosphereMesh, C, B, a, nSubdivision, maxSubdivisions);
				SplitTriangle(icosphereMesh, C, b, A, nSubdivision, maxSubdivisions);
				SplitTriangle(icosphereMesh, c, B, A, nSubdivision, maxSubdivisions);
				SplitTriangle(icosphereMesh, C, A, B, nSubdivision, maxSubdivisions);

			}
			else 
			{
				icosphereMesh.Vertices.push_back(a);
				icosphereMesh.Indices.push_back((uint32_t)(icosphereMesh.Vertices.size() - 1));

				icosphereMesh.Vertices.push_back(b);
				icosphereMesh.Indices.push_back((uint32_t)(icosphereMesh.Vertices.size() - 1));

				icosphereMesh.Vertices.push_back(c);
				icosphereMesh.Indices.push_back((uint32_t)(icosphereMesh.Vertices.size() - 1));
			}
		}
	};
}