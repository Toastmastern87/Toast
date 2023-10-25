#pragma once

#include "Toast/Renderer/Mesh.h"

#include "Toast/Core/Math/Math.h"

#include <DirectXMath.h>
#include <vector>

#define M_PI 3.14159265358979323846f

using namespace DirectX;

namespace Toast {

	class MeshFactory
	{
	public:
		static Ref<Mesh> CreateCube(float size = 1.0f)
		{
			std::vector<Vertex> vertices;
			vertices.resize(24);

			//Front
			vertices[0] = Vertex({ -(size / 2.0f), (size / 2.0f), -(size / 2.0f) }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.25f });
			vertices[1] = Vertex({ -(size / 2.0f), -(size / 2.0f), -(size / 2.0f) }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.25f, 0.25f });
			vertices[2] = Vertex({ (size / 2.0f), (size / 2.0f), -(size / 2.0f) }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.5f });
			vertices[3] = Vertex({ (size / 2.0f), -(size / 2.0f), -(size / 2.0f) }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.25f, 0.5f });
			
			//Back
			vertices[4] = Vertex({ -(size / 2.0f), -(size / 2.0f), (size / 2.0f) }, { 0.0f, 0.0f, 1.0f }, { 0.0f, -1.0f, 0.0f, 0.0f }, { 0.5f, 0.25f });
			vertices[5] = Vertex({ (size / 2.0f), -(size / 2.0f), (size / 2.0f) }, { 0.0f, 0.0f, 1.0f }, { 0.0f, -1.0f, 0.0f, 0.0f }, { 0.5f, 0.5f });
			vertices[6] = Vertex({ -(size / 2.0f), (size / 2.0f), (size / 2.0f) }, { 0.0f, 0.0f, 1.0f }, { 0.0f, -1.0f, 0.0f, 0.0f }, { 0.75f, 0.25f });
			vertices[7] = Vertex({ (size / 2.0f), (size / 2.0f), (size / 2.0f) }, { 0.0f, 0.0f, 1.0f }, { 0.0f, -1.0f, 0.0f, 0.0f }, { 0.75f, 0.5f });
			
			//Top
			vertices[8] = Vertex({ -(size / 2.0f), (size / 2.0f), (size / 2.0f) }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 0.75f, 0.25f });
			vertices[9] = Vertex({ (size / 2.0f), (size / 2.0f), (size / 2.0f) }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 0.75f, 0.5f });
			vertices[10] = Vertex({ -(size / 2.0f), (size / 2.0f), -(size / 2.0f) }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 1.0f, 0.25f });
			vertices[11] = Vertex({ (size / 2.0f), (size / 2.0f), -(size / 2.0f) }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 1.0f, 0.5f });

			//Bottom
			vertices[12] = Vertex({ -(size / 2.0f), -(size / 2.0f), -(size / 2.0f) }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f, 0.0f }, { 0.25f, 0.25f });
			vertices[13] = Vertex({ (size / 2.0f), -(size / 2.0f), -(size / 2.0f) }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f, 0.0f }, { 0.25f, 0.5f });
			vertices[14] = Vertex({ -(size / 2.0f), -(size / 2.0f), (size / 2.0f) }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f, 0.0f }, { 0.5f, 0.25f });
			vertices[15] = Vertex({ (size / 2.0f), -(size / 2.0f), (size / 2.0f) }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f, 0.0f }, { 0.5f, 0.5f });

			//Left
			vertices[16] = Vertex({ -(size / 2.0f), -(size / 2.0f), -(size / 2.0f) }, { -1.0f, 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f, 0.0f }, { 0.25f, 0.25f });
			vertices[17] = Vertex({ -(size / 2.0f), (size / 2.0f), (size / 2.0f) }, { -1.0f, 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f, 0.0f }, { 0.5f, 0.0f });
			vertices[18] = Vertex({ -(size / 2.0f), (size / 2.0f), -(size / 2.0f) }, { -1.0f, 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f, 0.0f }, { 0.25f, 0.0f });
			vertices[19] = Vertex({ -(size / 2.0f), -(size / 2.0f), (size / 2.0f) }, { -1.0f, 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f, 0.0f }, { 0.5f, 0.25f });

			//Right
			vertices[20] = Vertex({ (size / 2.0f), -(size / 2.0f), -(size / 2.0f) }, { 1.0f, 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f, 0.0f }, { 0.25f, 0.5f });
			vertices[21] = Vertex({ (size / 2.0f), (size / 2.0f), -(size / 2.0f) }, { 1.0f, 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f, 0.0f }, { 0.25f, 0.75f });
			vertices[22] = Vertex({ (size / 2.0f), -(size / 2.0f), (size / 2.0f) }, { 1.0f, 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f, 0.0f }, { 0.5f, 0.5f });
			vertices[23] = Vertex({ (size / 2.0f), (size / 2.0f), (size / 2.0f) }, { 1.0f, 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f, 0.0f }, { 0.5f, 0.75f });

			std::vector<uint32_t> indices;
			indices.resize(36);
			indices = {
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

			return CreateRef<Mesh>(vertices, indices, DirectX::XMMatrixIdentity());
		}

		static Ref<Mesh> CreateNoneTexturedCube(DirectX::XMFLOAT3 size)
		{
			std::vector<Vertex> vertices;
			vertices.resize(8);

			// Upper corners
			// Top Right
			vertices[0] = Vertex({ size.x * 0.5f,  size.y * 0.5f,  size.z * 0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 0.75f, 0.25f });
			// Top left
			vertices[1] = Vertex({ -size.x * 0.5f,  size.y * 0.5f,  size.z * 0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 0.75f, 0.5f });
			// Bottom left
			vertices[2] = Vertex({ -size.x * 0.5f,  size.y * 0.5f,  -size.z * 0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 1.0f, 0.25f });
			// Bottom right
			vertices[3] = Vertex({ size.x * 0.5f,  size.y * 0.5f,  -size.z * 0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 1.0f, 0.5f });

			// Lower corners
			// Top right
			vertices[4] = Vertex({ size.x * 0.5f,  -size.y * 0.5f, size.z * 0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 0.75f, 0.25f });
			// Top left
			vertices[5] = Vertex({ -size.x * 0.5f,  -size.y * 0.5f, size.z * 0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 0.75f, 0.5f });
			// Bottom left
			vertices[6] = Vertex({ -size.x * 0.5f,  -size.y * 0.5f, -size.z * 0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 1.0f, 0.25f });
			// Bottom right
			vertices[7] = Vertex({ size.x * 0.5f,  -size.y * 0.5f, -size.z * 0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 1.0f, 0.5f });

			std::vector<uint32_t> indices;
			indices.resize(36);
			indices = {
				//Top	
				0, 1, 3,
				1, 2, 3,

				//Front
				3, 2, 7,
				2, 6, 7,

				//Back
				0, 1, 4,
				0, 5, 4,

				//Bottom
				4, 5, 7,
				5, 6, 7,

				//Left
				2, 1, 6,
				1, 5, 6,

				//Right
				3, 0, 7,
				3, 4, 7
			};

			return CreateRef<Mesh>(vertices, indices, DirectX::XMMatrixIdentity());
		}

		//static uint32_t CreateSphere(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, uint32_t subdivisions = 0)
		//{
		//	uint32_t indexCount = 0;

		//	std::vector<Vertex> startVertices;

		//	float ratio = (1.0f + sqrt(5.0f)) / 2.0f;
		//	float vectorLength = Math::GetVectorLength({ ratio, 0.0f, -1.0f });

		//	startVertices.push_back(Vertex({ ratio, 0.0f, -1.0f }) / vectorLength * 0.5f);
		//	startVertices.push_back(Vertex({ -ratio, 0.0f, -1.0f }) / vectorLength * 0.5f);
		//	startVertices.push_back(Vertex({ ratio, 0.0f, 1.0f }) / vectorLength * 0.5f);
		//	startVertices.push_back(Vertex({ -ratio, 0.0f, 1.0f }) / vectorLength * 0.5f);
		//	startVertices.push_back(Vertex({ 0.0f, -1.0f, ratio }) / vectorLength * 0.5f);
		//	startVertices.push_back(Vertex({ 0.0f, -1.0f, -ratio }) / vectorLength * 0.5f);
		//	startVertices.push_back(Vertex({ 0.0f, 1.0f, ratio }) / vectorLength * 0.5f);
		//	startVertices.push_back(Vertex({ 0.0f, 1.0f, -ratio }) / vectorLength * 0.5f);
		//	startVertices.push_back(Vertex({ -1.0f, ratio, 0.0f }) / vectorLength * 0.5f);
		//	startVertices.push_back(Vertex({ -1.0f, -ratio, 0.0f }) / vectorLength * 0.5f);
		//	startVertices.push_back(Vertex({ 1.0f , ratio, 0.0f }) / vectorLength * 0.5f);
		//	startVertices.push_back(Vertex({ 1.0f , -ratio, 0.0f }) / vectorLength * 0.5f);

		//	indexCount += SplitTriangle(vertices, indices, startVertices[1], startVertices[3], startVertices[8], 0, subdivisions);
		//	indexCount += SplitTriangle(vertices, indices, startVertices[3], startVertices[1], startVertices[9], 0, subdivisions);
		//	indexCount += SplitTriangle(vertices, indices, startVertices[2], startVertices[0], startVertices[10], 0, subdivisions);
		//	indexCount += SplitTriangle(vertices, indices, startVertices[0], startVertices[2], startVertices[11], 0, subdivisions);
		//	
		//	indexCount += SplitTriangle(vertices, indices, startVertices[5], startVertices[7], startVertices[0], 0, subdivisions);
		//	indexCount += SplitTriangle(vertices, indices, startVertices[7], startVertices[5], startVertices[1], 0, subdivisions);
		//	indexCount += SplitTriangle(vertices, indices, startVertices[6], startVertices[4], startVertices[2], 0, subdivisions);
		//	indexCount += SplitTriangle(vertices, indices, startVertices[4], startVertices[6], startVertices[3], 0, subdivisions);
		//	 
		//	indexCount += SplitTriangle(vertices, indices, startVertices[9], startVertices[11], startVertices[4], 0, subdivisions);
		//	indexCount += SplitTriangle(vertices, indices, startVertices[11], startVertices[9], startVertices[5], 0, subdivisions);
		//	indexCount += SplitTriangle(vertices, indices, startVertices[10], startVertices[8], startVertices[6], 0, subdivisions);
		//	indexCount += SplitTriangle(vertices, indices, startVertices[8], startVertices[10], startVertices[7], 0, subdivisions);
		//	 
		//	indexCount += SplitTriangle(vertices, indices, startVertices[7], startVertices[1], startVertices[8], 0, subdivisions);
		//	indexCount += SplitTriangle(vertices, indices, startVertices[1], startVertices[5], startVertices[9], 0, subdivisions);
		//	indexCount += SplitTriangle(vertices, indices, startVertices[0], startVertices[7], startVertices[10], 0, subdivisions);
		//	indexCount += SplitTriangle(vertices, indices, startVertices[5], startVertices[0], startVertices[11], 0, subdivisions);
		//	
		//	indexCount += SplitTriangle(vertices, indices, startVertices[3], startVertices[6], startVertices[8], 0, subdivisions);
		//	indexCount += SplitTriangle(vertices, indices, startVertices[4], startVertices[3], startVertices[9], 0, subdivisions);
		//	indexCount += SplitTriangle(vertices, indices, startVertices[6], startVertices[2], startVertices[10], 0, subdivisions);
		//	indexCount += SplitTriangle(vertices, indices, startVertices[2], startVertices[4], startVertices[11], 0, subdivisions);

		//	return indexCount;
		//}

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

		//static uint32_t CreateOctahedron(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, uint32_t subdivisions = 0)
		//{
		//	uint32_t indexCount = 0;

		//	std::vector<Vertex> startVertices;

		//	float goldenValue = 0.5f * std::sinf(M_PI / 4.0f);

		//	startVertices.push_back(Vertex({0.0f, 0.5f, 0.0f}));
		//	startVertices.push_back(Vertex({ goldenValue, 0.0f, goldenValue }));
		//	startVertices.push_back(Vertex({ goldenValue, 0.0f, -goldenValue }));
		//	startVertices.push_back(Vertex({-goldenValue, 0.0f, -goldenValue }));
		//	startVertices.push_back(Vertex({-goldenValue, 0.0f, goldenValue }));
		//	startVertices.push_back(Vertex({0.0f, -0.5f, 0.0f}));

		//	indexCount += SplitTriangle(vertices, indices, startVertices[0], startVertices[1], startVertices[2], 0, subdivisions);
		//	indexCount += SplitTriangle(vertices, indices, startVertices[0], startVertices[2], startVertices[3], 0, subdivisions);
		//	indexCount += SplitTriangle(vertices, indices, startVertices[0], startVertices[3], startVertices[4], 0, subdivisions);
		//	indexCount += SplitTriangle(vertices, indices, startVertices[0], startVertices[4], startVertices[1], 0, subdivisions);

		//	indexCount += SplitTriangle(vertices, indices, startVertices[5], startVertices[1], startVertices[2], 0, subdivisions);
		//	indexCount += SplitTriangle(vertices, indices, startVertices[5], startVertices[2], startVertices[3], 0, subdivisions);
		//	indexCount += SplitTriangle(vertices, indices, startVertices[5], startVertices[3], startVertices[4], 0, subdivisions);
		//	indexCount += SplitTriangle(vertices, indices, startVertices[5], startVertices[4], startVertices[1], 0, subdivisions);

		//	return indexCount;
		//}
	};
}