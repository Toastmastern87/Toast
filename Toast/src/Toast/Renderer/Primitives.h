#pragma once

#include <DirectXMath.h>
#include <vector>

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

		static PrimitiveMesh CreateGrid() 
		{
			PrimitiveMesh gridMesh;

			float gridSize = 10.0f;

			for (int x = 0; x <= gridSize; x++)
			{
				for (int z = 0; z <= gridSize; z++)
				{
					gridMesh.Vertices.push_back(PrimitiveVertex({ -(gridSize / 2.0f) + x, 0.0f, -(gridSize / 2.0f) + z }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f }));
				}
			}

			for (int x = 0; x < gridSize; x++)
			{
				for (int z = 0; z < gridSize; z++)
				{
					gridMesh.Indices.push_back(z + 1 + (gridSize + 1) * x);
					gridMesh.Indices.push_back(z + gridSize + 2 + (gridSize + 1) * x);
					gridMesh.Indices.push_back(z + (gridSize + 1) * x);

					gridMesh.Indices.push_back(z + gridSize + 2 + (gridSize + 1) * x);
					gridMesh.Indices.push_back(z + gridSize + 1 + (gridSize + 1) * x);
					gridMesh.Indices.push_back(z + (gridSize + 1) * x);
				}
			}

			return gridMesh;
		}
	};
}