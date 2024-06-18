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
		static Ref<Mesh> CreateCube(float size = 1.0f, Vector3 color = { 0.0, 0.0, 0.0 })
		{
			std::vector<Vertex> vertices;
			vertices.resize(8);

			vertices[0] = Vertex(Vector3(-size, -size, -size), color); // Left-Bottom-Back
			vertices[1] = Vertex(Vector3(size, -size, -size), color);  // Right-Bottom-Back
			vertices[2] = Vertex(Vector3(-size, size, -size), color);  // Left-Top-Back
			vertices[3] = Vertex(Vector3(size, size, -size), color);   // Right-Top-Back
			vertices[4] = Vertex(Vector3(-size, -size, size), color);  // Left-Bottom-Front
			vertices[5] = Vertex(Vector3(size, -size, size), color);   // Right-Bottom-Front
			vertices[6] = Vertex(Vector3(-size, size, size), color);   // Left-Top-Front
			vertices[7] = Vertex(Vector3(size, size, size), color);    // Right-Top-Front


			std::vector<uint32_t> indices;
			indices.resize(36);
			indices = {
					// Front face
					4, 6, 5,  
					5, 6, 7,  

					// Back face
					0, 1, 2,  
					1, 3, 2,  

					// Top face
					2, 3, 6,
					3, 7, 6,

					// Bottom face
					0, 4, 1,
					1, 4, 5,

					// Left face
					0, 2, 4,  
					4, 2, 6,  

					// Right face
					1, 5, 3,
					5, 7, 3
			};

			return CreateRef<Mesh>(vertices, indices, DirectX::XMMatrixIdentity());
		}

		static Ref<Mesh> CreateNoneTexturedCube(DirectX::XMFLOAT3 size)
		{
			std::vector<Vertex> vertices;
			vertices.resize(8);

			// Upper corners
			// Top Right
			vertices[0] = Vertex({ size.x * 0.5f,  size.y * 0.5f,  size.z * 0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 0.75f, 0.25f }, { 0.0f, 0.0f, 0.0f });
			// Top left
			vertices[1] = Vertex({ -size.x * 0.5f,  size.y * 0.5f,  size.z * 0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 0.75f, 0.5f }, { 0.0f, 0.0f, 0.0f });
			// Bottom left
			vertices[2] = Vertex({ -size.x * 0.5f,  size.y * 0.5f,  -size.z * 0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 1.0f, 0.25f }, { 0.0f, 0.0f, 0.0f });
			// Bottom right
			vertices[3] = Vertex({ size.x * 0.5f,  size.y * 0.5f,  -size.z * 0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 1.0f, 0.5f }, { 0.0f, 0.0f, 0.0f });

			// Lower corners
			// Top right
			vertices[4] = Vertex({ size.x * 0.5f,  -size.y * 0.5f, size.z * 0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 0.75f, 0.25f }, { 0.0f, 0.0f, 0.0f });
			// Top left
			vertices[5] = Vertex({ -size.x * 0.5f,  -size.y * 0.5f, size.z * 0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 0.75f, 0.5f }, { 0.0f, 0.0f, 0.0f });
			// Bottom left
			vertices[6] = Vertex({ -size.x * 0.5f,  -size.y * 0.5f, -size.z * 0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 1.0f, 0.25f }, { 0.0f, 0.0f, 0.0f });
			// Bottom right
			vertices[7] = Vertex({ size.x * 0.5f,  -size.y * 0.5f, -size.z * 0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 1.0f, 0.5f }, { 0.0f, 0.0f, 0.0f });

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

		//static uint32_t SplitTriangle(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, Vertex a, Vertex b, Vertex c, int8_t subdivision, int8_t maxSubdivisions)
		//{
		//	uint32_t indexCount = 0;

		//	if (subdivision < maxSubdivisions)
		//	{
		//		int8_t nSubdivision = subdivision + 1;

		//		Vertex A = b + ((c - b) * 0.5f);
		//		Vertex B = c + ((a - c) * 0.5f);
		//		Vertex C = a + ((b - a) * 0.5f);

		//		A *= 0.5f / Math::GetVectorLength({ A.Position.x, A.Position.y, A.Position.z });
		//		B *= 0.5f / Math::GetVectorLength({ B.Position.x, B.Position.y, B.Position.z });
		//		C *= 0.5f / Math::GetVectorLength({ C.Position.x, C.Position.y, C.Position.z });

		//		indexCount += SplitTriangle(vertices, indices, C, B, a, nSubdivision, maxSubdivisions);
		//		indexCount += SplitTriangle(vertices, indices, C, b, A, nSubdivision, maxSubdivisions);
		//		indexCount += SplitTriangle(vertices, indices, c, B, A, nSubdivision, maxSubdivisions);
		//		indexCount += SplitTriangle(vertices, indices, C, A, B, nSubdivision, maxSubdivisions);
		//	}
		//	else 
		//	{
		//		vertices.push_back(a);
		//		indices.push_back((uint32_t)(vertices.size() - 1));
	
		//		vertices.push_back(b);
		//		indices.push_back((uint32_t)(vertices.size() - 1));

		//		vertices.push_back(c);
		//		indices.push_back((uint32_t)(vertices.size() - 1));

		//		indexCount += 3;
		//	}

		//	return indexCount;
		//}

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