#pragma once

#include "Toast/Renderer/Mesh.h"
#include "Toast/Core/Math.h"

#include <DirectXMath.h>
#include <vector>

#define M_PI 3.14159265358979323846f

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

	class MeshFactory
	{
	public:
		static uint32_t CreateCube(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, float width = 1.0f, float height = 1.0f, float depth = 1.0f)
		{
			//Front
			vertices.push_back(Vertex({ -(width / 2.0f), (height / 2.0f), -(depth / 2.0f) }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.25f }));
			vertices.push_back(Vertex({ -(width / 2.0f), -(height / 2.0f), -(depth / 2.0f) }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.25f, 0.25f }));
			vertices.push_back(Vertex({ (width / 2.0f), (height / 2.0f), -(depth / 2.0f) }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.5f }));
			vertices.push_back(Vertex({ (width / 2.0f), -(height / 2.0f), -(depth / 2.0f) }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.25f, 0.5f }));
			
			//Back
			vertices.push_back(Vertex({ -(width / 2.0f), -(height / 2.0f), (depth / 2.0f) }, { 0.0f, 0.0f, 1.0f }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.5f, 0.25f }));
			vertices.push_back(Vertex({ (width / 2.0f), -(height / 2.0f), (depth / 2.0f) }, { 0.0f, 0.0f, 1.0f }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.5f, 0.5f }));
			vertices.push_back(Vertex({ -(width / 2.0f), (height / 2.0f), (depth / 2.0f) }, { 0.0f, 0.0f, 1.0f }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.75f, 0.25f }));
			vertices.push_back(Vertex({ (width / 2.0f), (height / 2.0f), (depth / 2.0f) }, { 0.0f, 0.0f, 1.0f }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.75f, 0.5f }));
			
			//Top
			vertices.push_back(Vertex({ -(width / 2.0f), (height / 2.0f), (depth / 2.0f) }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f }, { 0.75f, 0.25f }));
			vertices.push_back(Vertex({ (width / 2.0f), (height / 2.0f), (depth / 2.0f) }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f }, { 0.75f, 0.5f }));
			vertices.push_back(Vertex({ -(width / 2.0f), (height / 2.0f), -(depth / 2.0f) }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.25f }));
			vertices.push_back(Vertex({ (width / 2.0f), (height / 2.0f), -(depth / 2.0f) }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.5f }));

			//Bottom
			vertices.push_back(Vertex({ -(width / 2.0f), -(height / 2.0f), -(depth / 2.0f) }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f }, { 0.25f, 0.25f }));
			vertices.push_back(Vertex({ (width / 2.0f), -(height / 2.0f), -(depth / 2.0f) }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f }, { 0.25f, 0.5f }));
			vertices.push_back(Vertex({ -(width / 2.0f), -(height / 2.0f), (depth / 2.0f) }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f }, { 0.5f, 0.25f }));
			vertices.push_back(Vertex({ (width / 2.0f), -(height / 2.0f), (depth / 2.0f) }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f }, { 0.5f, 0.5f }));

			//Left
			vertices.push_back(Vertex({ -(width / 2.0f), -(height / 2.0f), -(depth / 2.0f) }, { -1.0f, 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.25f, 0.25f }));
			vertices.push_back(Vertex({ -(width / 2.0f), (height / 2.0f), (depth / 2.0f) }, { -1.0f, 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.5f, 0.0f }));
			vertices.push_back(Vertex({ -(width / 2.0f), (height / 2.0f), -(depth / 2.0f) }, { -1.0f, 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.25f, 0.0f }));
			vertices.push_back(Vertex({ -(width / 2.0f), -(height / 2.0f), (depth / 2.0f) }, { -1.0f, 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.5f, 0.25f }));

			//Right
			vertices.push_back(Vertex({ (width / 2.0f), -(height / 2.0f), -(depth / 2.0f) }, { 1.0f, 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 0.25f, 0.5f }));
			vertices.push_back(Vertex({ (width / 2.0f), (height / 2.0f), -(depth / 2.0f) }, { 1.0f, 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 0.25f, 0.75f }));
			vertices.push_back(Vertex({ (width / 2.0f), -(height / 2.0f), (depth / 2.0f) }, { 1.0f, 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 0.5f, 0.5f }));
			vertices.push_back(Vertex({ (width / 2.0f), (height / 2.0f), (depth / 2.0f) }, { 1.0f, 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 0.5f, 0.75f }));

			indices = std::vector<uint32_t>{
				//Front
				0, 2, 1,
				1, 2, 3,

				//Back
				4, 5, 6,
				5, 7, 6,

				//Top
				8, 9, 10,
				9, 11, 10,

				//Bottom
				12, 13, 14,
				13, 15, 14,

				//Left
				16, 17, 18,
				16, 19, 17,

				//Right
				20, 21, 22,
				22, 21, 23
			};

			return 36;
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

		static uint32_t CreateSphere(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, uint32_t subdivisions = 0)
		{
			uint32_t indexCount = 0;

			std::vector<Vertex> startVertices;

			float ratio = (1.0f + sqrt(5.0f)) / 2.0f;
			float vectorLength = Math::GetVectorLength({ ratio, 0.0f, -1.0f });

			startVertices.push_back(Vertex({ ratio, 0.0f, -1.0f }) / vectorLength * 0.5f);
			startVertices.push_back(Vertex({ -ratio, 0.0f, -1.0f }) / vectorLength * 0.5f);
			startVertices.push_back(Vertex({ ratio, 0.0f, 1.0f }) / vectorLength * 0.5f);
			startVertices.push_back(Vertex({ -ratio, 0.0f, 1.0f }) / vectorLength * 0.5f);
			startVertices.push_back(Vertex({ 0.0f, -1.0f, ratio }) / vectorLength * 0.5f);
			startVertices.push_back(Vertex({ 0.0f, -1.0f, -ratio }) / vectorLength * 0.5f);
			startVertices.push_back(Vertex({ 0.0f, 1.0f, ratio }) / vectorLength * 0.5f);
			startVertices.push_back(Vertex({ 0.0f, 1.0f, -ratio }) / vectorLength * 0.5f);
			startVertices.push_back(Vertex({ -1.0f, ratio, 0.0f }) / vectorLength * 0.5f);
			startVertices.push_back(Vertex({ -1.0f, -ratio, 0.0f }) / vectorLength * 0.5f);
			startVertices.push_back(Vertex({ 1.0f , ratio, 0.0f }) / vectorLength * 0.5f);
			startVertices.push_back(Vertex({ 1.0f , -ratio, 0.0f }) / vectorLength * 0.5f);

			indexCount += SplitTriangle(vertices, indices, startVertices[1], startVertices[3], startVertices[8], 0, subdivisions);
			indexCount += SplitTriangle(vertices, indices, startVertices[3], startVertices[1], startVertices[9], 0, subdivisions);
			indexCount += SplitTriangle(vertices, indices, startVertices[2], startVertices[0], startVertices[10], 0, subdivisions);
			indexCount += SplitTriangle(vertices, indices, startVertices[0], startVertices[2], startVertices[11], 0, subdivisions);
			
			indexCount += SplitTriangle(vertices, indices, startVertices[5], startVertices[7], startVertices[0], 0, subdivisions);
			indexCount += SplitTriangle(vertices, indices, startVertices[7], startVertices[5], startVertices[1], 0, subdivisions);
			indexCount += SplitTriangle(vertices, indices, startVertices[6], startVertices[4], startVertices[2], 0, subdivisions);
			indexCount += SplitTriangle(vertices, indices, startVertices[4], startVertices[6], startVertices[3], 0, subdivisions);
			 
			indexCount += SplitTriangle(vertices, indices, startVertices[9], startVertices[11], startVertices[4], 0, subdivisions);
			indexCount += SplitTriangle(vertices, indices, startVertices[11], startVertices[9], startVertices[5], 0, subdivisions);
			indexCount += SplitTriangle(vertices, indices, startVertices[10], startVertices[8], startVertices[6], 0, subdivisions);
			indexCount += SplitTriangle(vertices, indices, startVertices[8], startVertices[10], startVertices[7], 0, subdivisions);
			 
			indexCount += SplitTriangle(vertices, indices, startVertices[7], startVertices[1], startVertices[8], 0, subdivisions);
			indexCount += SplitTriangle(vertices, indices, startVertices[1], startVertices[5], startVertices[9], 0, subdivisions);
			indexCount += SplitTriangle(vertices, indices, startVertices[0], startVertices[7], startVertices[10], 0, subdivisions);
			indexCount += SplitTriangle(vertices, indices, startVertices[5], startVertices[0], startVertices[11], 0, subdivisions);
			
			indexCount += SplitTriangle(vertices, indices, startVertices[3], startVertices[6], startVertices[8], 0, subdivisions);
			indexCount += SplitTriangle(vertices, indices, startVertices[4], startVertices[3], startVertices[9], 0, subdivisions);
			indexCount += SplitTriangle(vertices, indices, startVertices[6], startVertices[2], startVertices[10], 0, subdivisions);
			indexCount += SplitTriangle(vertices, indices, startVertices[2], startVertices[4], startVertices[11], 0, subdivisions);

			return indexCount;
		}

		static uint32_t SplitTriangle(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, Vertex a, Vertex b, Vertex c, int8_t subdivision, int8_t maxSubdivisions)
		{
			uint32_t indexCount = 0;

			if (subdivision < maxSubdivisions)
			{
				int8_t nSubdivision = subdivision + 1;

				Vertex A = b + ((c - b) * 0.5f);
				Vertex B = c + ((a - c) * 0.5f);
				Vertex C = a + ((b - a) * 0.5f);

				A *= 0.5f / Math::GetVectorLength({ A.Position.x, A.Position.y, A.Position.z });
				B *= 0.5f / Math::GetVectorLength({ B.Position.x, B.Position.y, B.Position.z });
				C *= 0.5f / Math::GetVectorLength({ C.Position.x, C.Position.y, C.Position.z });

				indexCount += SplitTriangle(vertices, indices, C, B, a, nSubdivision, maxSubdivisions);
				indexCount += SplitTriangle(vertices, indices, C, b, A, nSubdivision, maxSubdivisions);
				indexCount += SplitTriangle(vertices, indices, c, B, A, nSubdivision, maxSubdivisions);
				indexCount += SplitTriangle(vertices, indices, C, A, B, nSubdivision, maxSubdivisions);
			}
			else 
			{
				vertices.push_back(a);
				indices.push_back((uint32_t)(vertices.size() - 1));
	
				vertices.push_back(b);
				indices.push_back((uint32_t)(vertices.size() - 1));

				vertices.push_back(c);
				indices.push_back((uint32_t)(vertices.size() - 1));

				indexCount += 3;
			}

			return indexCount;
		}

		static uint32_t CreateOctahedron(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, uint32_t subdivisions = 0)
		{
			uint32_t indexCount = 0;

			std::vector<Vertex> startVertices;

			float goldenValue = 0.5f * std::sinf(M_PI / 4.0f);

			startVertices.push_back(Vertex({0.0f, 0.5f, 0.0f}));
			startVertices.push_back(Vertex({ goldenValue, 0.0f, goldenValue }));
			startVertices.push_back(Vertex({ goldenValue, 0.0f, -goldenValue }));
			startVertices.push_back(Vertex({-goldenValue, 0.0f, -goldenValue }));
			startVertices.push_back(Vertex({-goldenValue, 0.0f, goldenValue }));
			startVertices.push_back(Vertex({0.0f, -0.5f, 0.0f}));

			indexCount += SplitTriangle(vertices, indices, startVertices[0], startVertices[1], startVertices[2], 0, subdivisions);
			indexCount += SplitTriangle(vertices, indices, startVertices[0], startVertices[2], startVertices[3], 0, subdivisions);
			indexCount += SplitTriangle(vertices, indices, startVertices[0], startVertices[3], startVertices[4], 0, subdivisions);
			indexCount += SplitTriangle(vertices, indices, startVertices[0], startVertices[4], startVertices[1], 0, subdivisions);

			indexCount += SplitTriangle(vertices, indices, startVertices[5], startVertices[1], startVertices[2], 0, subdivisions);
			indexCount += SplitTriangle(vertices, indices, startVertices[5], startVertices[2], startVertices[3], 0, subdivisions);
			indexCount += SplitTriangle(vertices, indices, startVertices[5], startVertices[3], startVertices[4], 0, subdivisions);
			indexCount += SplitTriangle(vertices, indices, startVertices[5], startVertices[4], startVertices[1], 0, subdivisions);

			//indexCount += SplitTriangle(vertices, indices, startVertices[1], startVertices[3], startVertices[8], 0, subdivisions);
			//indexCount += SplitTriangle(vertices, indices, startVertices[3], startVertices[1], startVertices[9], 0, subdivisions);
			//indexCount += SplitTriangle(vertices, indices, startVertices[2], startVertices[0], startVertices[10], 0, subdivisions);
			//indexCount += SplitTriangle(vertices, indices, startVertices[0], startVertices[2], startVertices[11], 0, subdivisions);

			//indexCount += SplitTriangle(vertices, indices, startVertices[5], startVertices[7], startVertices[0], 0, subdivisions);
			//indexCount += SplitTriangle(vertices, indices, startVertices[7], startVertices[5], startVertices[1], 0, subdivisions);
			//indexCount += SplitTriangle(vertices, indices, startVertices[6], startVertices[4], startVertices[2], 0, subdivisions);
			//indexCount += SplitTriangle(vertices, indices, startVertices[4], startVertices[6], startVertices[3], 0, subdivisions);

			//indexCount += SplitTriangle(vertices, indices, startVertices[9], startVertices[11], startVertices[4], 0, subdivisions);
			//indexCount += SplitTriangle(vertices, indices, startVertices[11], startVertices[9], startVertices[5], 0, subdivisions);
			//indexCount += SplitTriangle(vertices, indices, startVertices[10], startVertices[8], startVertices[6], 0, subdivisions);
			//indexCount += SplitTriangle(vertices, indices, startVertices[8], startVertices[10], startVertices[7], 0, subdivisions);

			//indexCount += SplitTriangle(vertices, indices, startVertices[7], startVertices[1], startVertices[8], 0, subdivisions);
			//indexCount += SplitTriangle(vertices, indices, startVertices[1], startVertices[5], startVertices[9], 0, subdivisions);
			//indexCount += SplitTriangle(vertices, indices, startVertices[0], startVertices[7], startVertices[10], 0, subdivisions);
			//indexCount += SplitTriangle(vertices, indices, startVertices[5], startVertices[0], startVertices[11], 0, subdivisions);

			//indexCount += SplitTriangle(vertices, indices, startVertices[3], startVertices[6], startVertices[8], 0, subdivisions);
			//indexCount += SplitTriangle(vertices, indices, startVertices[4], startVertices[3], startVertices[9], 0, subdivisions);
			//indexCount += SplitTriangle(vertices, indices, startVertices[6], startVertices[2], startVertices[10], 0, subdivisions);
			//indexCount += SplitTriangle(vertices, indices, startVertices[2], startVertices[4], startVertices[11], 0, subdivisions);

			return indexCount;
		}
	};
}