#pragma once

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

			uint32_t gridSize = 5;

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

		static PrimitiveMesh CreateIcosphere()
		{
			PrimitiveMesh icosphereMesh;

			DirectX::XMFLOAT3 firstVertex, secondVertex, thirdVertex, fourthVertex, fifthVertex, sixthVertex, seventhVertex, eigthVertex, ninthVertex, tenthVertex, eleventhVertex, twelvethVertex;

			float ratio = (1.0f + sqrt(5.0f)) / 2.0f;

			DirectX::XMStoreFloat3(&firstVertex, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&DirectX::XMFLOAT3(ratio, 0.0f, -1.0f))) * 0.5f);
			DirectX::XMStoreFloat3(&secondVertex, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&DirectX::XMFLOAT3(-ratio, 0.0f, -1.0f))) * 0.5f);
			DirectX::XMStoreFloat3(&thirdVertex, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&DirectX::XMFLOAT3(ratio, 0.0f, 1.0f))) * 0.5f);
			DirectX::XMStoreFloat3(&fourthVertex, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&DirectX::XMFLOAT3(-ratio, 0.0f, 1.0f))) * 0.5f);
			DirectX::XMStoreFloat3(&fifthVertex, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&DirectX::XMFLOAT3(0.0f, -1.0f, ratio))) * 0.5f);
			DirectX::XMStoreFloat3(&sixthVertex, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&DirectX::XMFLOAT3(0.0f, -1.0f, -ratio))) * 0.5f);
			DirectX::XMStoreFloat3(&seventhVertex, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&DirectX::XMFLOAT3(0.0f, 1.0f, ratio))) * 0.5f);
			DirectX::XMStoreFloat3(&eigthVertex, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&DirectX::XMFLOAT3(0.0f, 1.0f, -ratio))) * 0.5f);
			DirectX::XMStoreFloat3(&ninthVertex, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&DirectX::XMFLOAT3(-1.0f, ratio, 0.0f))) * 0.5f);
			DirectX::XMStoreFloat3(&tenthVertex, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&DirectX::XMFLOAT3(-1.0f, -ratio, 0.0f))) * 0.5f);
			DirectX::XMStoreFloat3(&eleventhVertex, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&DirectX::XMFLOAT3(1.0f, ratio, 0.0f))) * 0.5f);
			DirectX::XMStoreFloat3(&twelvethVertex, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&DirectX::XMFLOAT3(1.0f, -ratio, 0.0f))) * 0.5f);

			icosphereMesh.Vertices.push_back(PrimitiveVertex(firstVertex, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f }));
			icosphereMesh.Vertices.push_back(PrimitiveVertex(secondVertex, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f }));
			icosphereMesh.Vertices.push_back(PrimitiveVertex(thirdVertex, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f }));
			icosphereMesh.Vertices.push_back(PrimitiveVertex(fourthVertex, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f }));
			icosphereMesh.Vertices.push_back(PrimitiveVertex(fifthVertex, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f }));
			icosphereMesh.Vertices.push_back(PrimitiveVertex(sixthVertex, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f }));
			icosphereMesh.Vertices.push_back(PrimitiveVertex(seventhVertex, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f }));
			icosphereMesh.Vertices.push_back(PrimitiveVertex(eigthVertex, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f }));
			icosphereMesh.Vertices.push_back(PrimitiveVertex(ninthVertex, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f }));
			icosphereMesh.Vertices.push_back(PrimitiveVertex(tenthVertex, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f }));
			icosphereMesh.Vertices.push_back(PrimitiveVertex(eleventhVertex, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f }));
			icosphereMesh.Vertices.push_back(PrimitiveVertex(twelvethVertex, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f }));

			icosphereMesh.Indices = std::vector<uint32_t>{
				1, 3, 8,
				3, 1, 9,
				2, 0, 10,
				0, 2, 11,

				5, 7, 0,
				7, 5, 1,
				6, 4, 2,
				4, 6, 3,

				9, 11, 4,
				11, 9, 5,
				10, 8, 6,
				8, 10, 7,

				7, 1, 8,
				1, 5, 9,
				0, 7, 10,
				5, 0, 11,

				3, 6, 8,
				4, 3, 9,
				6, 2, 10,
				2, 4, 11
			};

			return icosphereMesh;
		}
	};
}