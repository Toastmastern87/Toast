#include "tpch.h"

#include "PlanetSystem.h"

#include "Toast/Scene/Components.h"

#include <chrono>

#define BASE_PLANET_SUBDIVISIONS 7

namespace Toast {

	std::mutex PlanetSystem::planetDataMutex;
	std::mutex PlanetSystem::terrainCollidersMutex;
	std::future<void> PlanetSystem::generationFuture;
	std::atomic<bool> PlanetSystem::newPlanetReady{ false };
	std::atomic<bool> PlanetSystem::planetGenerationOngoing{ false };

	std::vector<Ref<PlanetNode>> PlanetSystem::sPlanetNodes;

	uint32_t PlanetSystem::HashFace(uint32_t index0, uint32_t index1, uint32_t index2)
	{
		// Simple hash combining indices; you can make this more complex as needed
		return static_cast<uint32_t>(index0 * 73856093 ^ index1 * 19349663 ^ index2 * 83492791);
	}

	Vector2 PlanetSystem::GetUVFromPosition(const Vector3 pos, double width, double height)
	{
		TOAST_PROFILE_FUNCTION();

		Vector3 normalizedPos = Vector3::Normalize(pos);

		double theta = atan2(normalizedPos.z, normalizedPos.x);
		double phi = asin(normalizedPos.y);

		Vector2 uv = Vector2(theta / M_PI, phi / M_PIDIV2);
		uv.x = uv.x * 0.5 + 0.5;
		uv.y = uv.y * 0.5 + 0.5;

		uv.x = uv.x * (width - 1.0);
		uv.y = uv.y * (height - 1.0);

		return uv;
	}

	void PlanetSystem::GetFaceBounds(const std::initializer_list<Vector3>& vertices, Bounds& bounds)
	{
		// Check if the list is empty
		if (vertices.size() == 0)
		{
			// Handle the error as needed, e.g., throw an exception or set min/max to zero
			bounds.mins = bounds.maxs = Vector3(0.0f, 0.0f, 0.0f);
			return;
		}

		// Initialize min and max with the first vertex
		auto it = vertices.begin();
		bounds.mins = bounds.maxs = *it;

		// Iterate over the rest of the vertices
		for (++it; it != vertices.end(); ++it)
		{
			const Vector3& vertex = *it;

			// Update min bounds
			if (vertex.x < bounds.mins.x) bounds.mins.x = vertex.x;
			if (vertex.y < bounds.mins.y) bounds.mins.y = vertex.y;
			if (vertex.z < bounds.mins.z) bounds.mins.z = vertex.z;

			// Update max bounds
			if (vertex.x > bounds.maxs.x) bounds.maxs.x = vertex.x;
			if (vertex.y > bounds.maxs.y) bounds.maxs.y = vertex.y;
			if (vertex.z > bounds.maxs.z) bounds.maxs.z = vertex.z;
		}
	}

	void PlanetSystem::SubdivideBasePlanet(PlanetComponent& planet, Ref<PlanetNode>& node, double scale)
	{
		if (node->SubdivisionLevel >= BASE_PLANET_SUBDIVISIONS)
			return;

		CPUVertex A, B, C;
		double height;

		A.Position = node->B.Position + ((node->C.Position - node->B.Position) * 0.5);
		A.UV = GetUVFromPosition(Vector3::Normalize(A.Position), (double)planet.TerrainData.Width, (double)planet.TerrainData.Height);
		height = GetHeight(A.UV, planet.TerrainData);
		A.Position = Vector3::Normalize(A.Position) * (planet.PlanetData.radius + height);

		B.Position = node->C.Position + ((node->A.Position - node->C.Position) * 0.5);
		B.UV = GetUVFromPosition(Vector3::Normalize(B.Position), (double)planet.TerrainData.Width, (double)planet.TerrainData.Height);
		height = GetHeight(B.UV, planet.TerrainData);
		B.Position = Vector3::Normalize(B.Position) * (planet.PlanetData.radius + height);

		C.Position = node->A.Position + ((node->B.Position - node->A.Position) * 0.5);
		C.UV = GetUVFromPosition(Vector3::Normalize(C.Position), (double)planet.TerrainData.Width, (double)planet.TerrainData.Height);
		height = GetHeight(C.UV, planet.TerrainData);
		C.Position = Vector3::Normalize(C.Position) * (planet.PlanetData.radius + height);

		node->ChildNodes.emplace_back(CreateRef<PlanetNode>(A, B, C, node->SubdivisionLevel + 1));
		node->ChildNodes.emplace_back(CreateRef<PlanetNode>(C, B, node->A, node->SubdivisionLevel + 1));
		node->ChildNodes.emplace_back(CreateRef<PlanetNode>(node->B, A, C, node->SubdivisionLevel + 1));
		node->ChildNodes.emplace_back(CreateRef<PlanetNode>(B, A, node->C, node->SubdivisionLevel + 1));

		for (auto& child : node->ChildNodes) 
			SubdivideBasePlanet(planet, child, scale);
	}

	void PlanetSystem::SubdivideFace(CPUVertex& A, CPUVertex& B, CPUVertex& C, Vector3& cameraPosPlanetSpace, PlanetComponent& planet, const Vector3& planetCenter, Matrix& planetTransform, uint16_t subdivision, const siv::PerlinNoise& perlin, TerrainDetailComponent* terrainDetail)
	{
		double height;
		NextPlanetFace nextFace;
		Vector2 uvCoords;
		Bounds bounds;

		double aDistance = (A.Position - cameraPosPlanetSpace).LengthSqrt();
		double bDistance = (B.Position - cameraPosPlanetSpace).LengthSqrt();
		double cDistance = (C.Position - cameraPosPlanetSpace).LengthSqrt();

		if (subdivision >= BASE_PLANET_SUBDIVISIONS + planet.Subdivisions)	
			nextFace = NextPlanetFace::LEAF;
		else
		{
			if (aDistance < planet.DistanceLUT[(uint32_t)subdivision - BASE_PLANET_SUBDIVISIONS] && bDistance < planet.DistanceLUT[(uint32_t)subdivision - BASE_PLANET_SUBDIVISIONS] && cDistance < planet.DistanceLUT[(uint32_t)subdivision - BASE_PLANET_SUBDIVISIONS])
				nextFace = NextPlanetFace::SPLIT;
			else 
				nextFace = NextPlanetFace::LEAF; // Add triangle due to distance
		}

		if (nextFace == NextPlanetFace::SPLIT)
		{
			CPUVertex a, b, c;
			double mediumTerrainDetailNoise = 0.0;

			a.Position = B.Position + ((C.Position - B.Position) * 0.5);
			b.Position = C.Position + ((A.Position - C.Position) * 0.5);
			c.Position = A.Position + ((B.Position - A.Position) * 0.5);

			Vector3 aNormalized = Vector3::Normalize(a.Position);
			a.UV = GetUVFromPosition(aNormalized, (double)planet.TerrainData.Width, (double)planet.TerrainData.Height);
			if (terrainDetail && subdivision > terrainDetail->SubdivisionActivation)
				mediumTerrainDetailNoise = perlin.octave2D_01(a.UV.x * terrainDetail->Frequency, a.UV.y * terrainDetail->Frequency, terrainDetail->Octaves) * terrainDetail->Amplitude;
			height = GetHeight(a.UV, planet.TerrainData);
			a.Position = aNormalized * (planet.PlanetData.radius + height + mediumTerrainDetailNoise);

			Vector3 bNormalized = Vector3::Normalize(b.Position);
			b.UV = GetUVFromPosition(bNormalized, (double)planet.TerrainData.Width, (double)planet.TerrainData.Height);
			if (terrainDetail && subdivision > terrainDetail->SubdivisionActivation)
				mediumTerrainDetailNoise = perlin.octave2D_01(b.UV.x * terrainDetail->Frequency, b.UV.y * terrainDetail->Frequency, terrainDetail->Octaves) * terrainDetail->Amplitude;
			height = GetHeight(b.UV, planet.TerrainData);
			b.Position = bNormalized * (planet.PlanetData.radius + height + mediumTerrainDetailNoise);

			Vector3 cNormalized = Vector3::Normalize(c.Position);
			c.UV = GetUVFromPosition(cNormalized, (double)planet.TerrainData.Width, (double)planet.TerrainData.Height);
			if (terrainDetail && subdivision > terrainDetail->SubdivisionActivation)
				mediumTerrainDetailNoise = perlin.octave2D_01(c.UV.x * terrainDetail->Frequency, c.UV.y * terrainDetail->Frequency, terrainDetail->Octaves) * terrainDetail->Amplitude;
			height = GetHeight(c.UV, planet.TerrainData);
			c.Position = cNormalized * (planet.PlanetData.radius + height + mediumTerrainDetailNoise);

			int16_t nextSubdivision = subdivision + 1;

			SubdivideFace(a, b, c, cameraPosPlanetSpace, planet, planetCenter, planetTransform, nextSubdivision, perlin, terrainDetail);
			SubdivideFace(c, b, A, cameraPosPlanetSpace, planet, planetCenter, planetTransform, nextSubdivision, perlin, terrainDetail);
			SubdivideFace(B, a, c, cameraPosPlanetSpace, planet, planetCenter, planetTransform, nextSubdivision, perlin, terrainDetail);
			SubdivideFace(b, a, C, cameraPosPlanetSpace, planet, planetCenter, planetTransform, nextSubdivision, perlin, terrainDetail);
		}
		else
		{
			bool crackTriangle = false;
			CPUVertex closestVertex, furthestVertex, middleVertex;

			double closestDistance = (std::min)(aDistance, (std::min)(bDistance, cDistance));
			double furthestDistance = (std::max)(aDistance, (std::max)(bDistance, cDistance));
			double secondClosestDistance;

			if (closestDistance == aDistance)
				closestVertex = A;
			else if (closestDistance == bDistance)
				closestVertex = B;
			else
				closestVertex = C;

			if (furthestDistance == aDistance)
				furthestVertex = A;
			else if (furthestDistance == bDistance)
				furthestVertex = B;
			else
				furthestVertex = C;

			if (closestDistance == aDistance)
				secondClosestDistance = (furthestDistance == bDistance) ? cDistance : bDistance;
			else if (closestDistance == bDistance)
				secondClosestDistance = (furthestDistance == aDistance) ? cDistance : aDistance;
			else
				secondClosestDistance = (furthestDistance == aDistance) ? bDistance : aDistance;

			// Identify middle vertex based on distances
			if ((closestDistance != aDistance) && (furthestDistance != aDistance))
				middleVertex = A;
			else if ((closestDistance != bDistance) && (furthestDistance != bDistance))
				middleVertex = B;
			else
				middleVertex = C;

			if(subdivision < (planet.Subdivisions + BASE_PLANET_SUBDIVISIONS))
			{
				if (closestDistance < planet.DistanceLUT[(uint32_t)subdivision - BASE_PLANET_SUBDIVISIONS] && secondClosestDistance < planet.DistanceLUT[(uint32_t)subdivision - BASE_PLANET_SUBDIVISIONS])
					crackTriangle = true;
			}

			// Function to add or retrieve a vertex
			auto addVertex = [&](const CPUVertex& cpuVertex, const Vector3& transformedPos) -> size_t {
				// Create a Vertex instance
				Vertex v;
				v.Position = { (float)transformedPos.x, (float)transformedPos.y, (float)transformedPos.z };
				v.Texcoord = { (float)cpuVertex.UV.x, (float)cpuVertex.UV.y };
				// Initialize normal to zero; we'll accumulate face normals
				v.Normal = { 0.0f, 0.0f, 0.0f };
				v.Tangent = { 0.0f, 0.0f, 0.0f, 0.0f };
				v.Color = { 0.0f, 0.0f, 0.0f };

				// Try to insert the vertex into the map
				auto result = planet.VertexMap.emplace(v, planet.BuildVertices.size());
				if (result.second) {
					// Vertex was not in the map; add it to the vertex list
					planet.BuildVertices.emplace_back(v);
				}
				// Return the index of the vertex
				return result.first->second;
				};

			if (!crackTriangle)
			{
				Vector3 vecA = planetTransform * A.Position;
				Vector3 vecB = planetTransform * B.Position;
				Vector3 vecC = planetTransform * C.Position;

				Vector3 normal = Vector3::Normalize(Vector3::Cross(vecB - vecA, vecC - vecA));

				if (planet.PlanetData.smoothShading)
				{
					// Add or retrieve vertices
					size_t indexA = addVertex(A, vecA);
					size_t indexB = addVertex(B, vecB);
					size_t indexC = addVertex(C, vecC);

					// Accumulate normals
					planet.BuildVertices[indexA].Normal.x += (float)normal.x;
					planet.BuildVertices[indexA].Normal.y += (float)normal.y;
					planet.BuildVertices[indexA].Normal.z += (float)normal.z;

					planet.BuildVertices[indexB].Normal.x += (float)normal.x;
					planet.BuildVertices[indexB].Normal.y += (float)normal.y;
					planet.BuildVertices[indexB].Normal.z += (float)normal.z;

					planet.BuildVertices[indexC].Normal.x += (float)normal.x;
					planet.BuildVertices[indexC].Normal.y += (float)normal.y;
					planet.BuildVertices[indexC].Normal.z += (float)normal.z;

					// Add indices
					planet.BuildIndices.emplace_back(indexA);
					planet.BuildIndices.emplace_back(indexB);
					planet.BuildIndices.emplace_back(indexC);
				}
				else 
				{
					Vertex vertexA = Vertex(vecA, A.UV, normal);
					planet.BuildVertices.emplace_back(vertexA);
					planet.BuildIndices.emplace_back(planet.BuildVertices.size() - 1);

					Vertex vertexB = Vertex(vecB, B.UV, normal);
					planet.BuildVertices.emplace_back(vertexB);
					planet.BuildIndices.emplace_back(planet.BuildVertices.size() - 1);

					Vertex vertexC = Vertex(vecC, C.UV, normal);
					planet.BuildVertices.emplace_back(vertexC);
					planet.BuildIndices.emplace_back(planet.BuildVertices.size() - 1);
				}

				// Chunks are used by the physics engine
				AssignFaceToChunk(vecA, vecB, vecC, planet.TerrainChunks, planetCenter);
			}
			else
			{
				double mediumTerrainDetailNoise = 0.0;
				// Calculate new vertex	
				CPUVertex additionalVertex;
				additionalVertex.Position = (closestVertex.Position + middleVertex.Position) * 0.5;
				Vector3 additionalVertexNormalized = Vector3::Normalize(additionalVertex.Position);
				additionalVertex.UV = GetUVFromPosition(additionalVertexNormalized, (double)planet.TerrainData.Width, (double)planet.TerrainData.Height);
				if(terrainDetail && subdivision > terrainDetail->SubdivisionActivation)
					mediumTerrainDetailNoise = perlin.octave2D_01(additionalVertex.UV.x * terrainDetail->Frequency, additionalVertex.UV.y * terrainDetail->Frequency, terrainDetail->Octaves) * terrainDetail->Amplitude;
				double height = GetHeight(additionalVertex.UV, planet.TerrainData);
				additionalVertex.Position = additionalVertexNormalized * (planet.PlanetData.radius + height + mediumTerrainDetailNoise);

				Vector3 additionalVertexPos = planetTransform * additionalVertex.Position;
				Vector3 closestVertexPos = planetTransform * closestVertex.Position;
				Vector3 middleVertexPos = planetTransform * middleVertex.Position;
				Vector3 furthestVertexPos = planetTransform * furthestVertex.Position;

				// First triangle
				Vector3 normal = Vector3::Normalize(Vector3::Cross(additionalVertexPos - closestVertexPos, additionalVertexPos - furthestVertexPos));

				if (normal.y < 0.0)
					normal = normal * -1.0;

				if (planet.PlanetData.smoothShading)
				{
					// Add or retrieve vertices
					size_t indexA = addVertex(A, additionalVertexPos);
					size_t indexB = addVertex(B, closestVertexPos);
					size_t indexC = addVertex(C, furthestVertexPos);

					// Accumulate normals
					planet.BuildVertices[indexA].Normal.x += (float)normal.x;
					planet.BuildVertices[indexA].Normal.y += (float)normal.y;
					planet.BuildVertices[indexA].Normal.z += (float)normal.z;

					planet.BuildVertices[indexB].Normal.x += (float)normal.x;
					planet.BuildVertices[indexB].Normal.y += (float)normal.y;
					planet.BuildVertices[indexB].Normal.z += (float)normal.z;

					planet.BuildVertices[indexC].Normal.x += (float)normal.x;
					planet.BuildVertices[indexC].Normal.y += (float)normal.y;
					planet.BuildVertices[indexC].Normal.z += (float)normal.z;

					// Add indices
					planet.BuildIndices.emplace_back(indexA);
					planet.BuildIndices.emplace_back(indexB);
					planet.BuildIndices.emplace_back(indexC);
				}
				else
				{
					Vertex vertexA = Vertex(additionalVertexPos, additionalVertex.UV, normal);
					planet.BuildVertices.emplace_back(vertexA);
					planet.BuildIndices.emplace_back(planet.BuildVertices.size() - 1);

					Vertex vertexB = Vertex(closestVertexPos, closestVertex.UV, normal);
					planet.BuildVertices.emplace_back(vertexB);
					planet.BuildIndices.emplace_back(planet.BuildVertices.size() - 1);

					Vertex vertexC = Vertex(furthestVertexPos, furthestVertex.UV, normal);
					planet.BuildVertices.emplace_back(vertexC);
					planet.BuildIndices.emplace_back(planet.BuildVertices.size() - 1);
				}

				AssignFaceToChunk(additionalVertexPos, closestVertexPos, furthestVertexPos, planet.TerrainChunks, planetCenter);

				// Second triangle
				normal = Vector3::Normalize(Vector3::Cross(additionalVertexPos - furthestVertexPos, additionalVertexPos - middleVertexPos));
				if (normal.y < 0.0)
					normal = normal * -1.0;

				if (planet.PlanetData.smoothShading)
				{
					// Add or retrieve vertices
					size_t indexA = addVertex(A, additionalVertexPos);
					size_t indexB = addVertex(B, furthestVertexPos);
					size_t indexC = addVertex(C, middleVertexPos);

					// Accumulate normals
					planet.BuildVertices[indexA].Normal.x += (float)normal.x;
					planet.BuildVertices[indexA].Normal.y += (float)normal.y;
					planet.BuildVertices[indexA].Normal.z += (float)normal.z;

					planet.BuildVertices[indexB].Normal.x += (float)normal.x;
					planet.BuildVertices[indexB].Normal.y += (float)normal.y;
					planet.BuildVertices[indexB].Normal.z += (float)normal.z;

					planet.BuildVertices[indexC].Normal.x += (float)normal.x;
					planet.BuildVertices[indexC].Normal.y += (float)normal.y;
					planet.BuildVertices[indexC].Normal.z += (float)normal.z;

					// Add indices
					planet.BuildIndices.emplace_back(indexA);
					planet.BuildIndices.emplace_back(indexB);
					planet.BuildIndices.emplace_back(indexC);
				}
				else
				{
					Vertex vertexD = Vertex(additionalVertexPos, additionalVertex.UV, normal);
					vertexD.Color = { 1.0f, 0.0f, 0.0f };
					planet.BuildVertices.emplace_back(vertexD);
					planet.BuildIndices.emplace_back(planet.BuildVertices.size() - 1);

					Vertex vertexF = Vertex(furthestVertexPos, furthestVertex.UV, normal);
					planet.BuildVertices.emplace_back(vertexF);
					planet.BuildIndices.emplace_back(planet.BuildVertices.size() - 1);

					Vertex vertexE = Vertex(middleVertexPos, middleVertex.UV, normal);
					planet.BuildVertices.emplace_back(vertexE);
					planet.BuildIndices.emplace_back(planet.BuildVertices.size() - 1);
				}

				AssignFaceToChunk(additionalVertexPos, furthestVertexPos, middleVertexPos, planet.TerrainChunks, planetCenter);
			}
		
			return;
		}
	}

	void PlanetSystem::CalculateBasePlanet(PlanetComponent& planet, double scale)
	{
		TOAST_PROFILE_FUNCTION();

		auto start = std::chrono::high_resolution_clock::now();

		double ratio = ((1.0 + sqrt(5.0)) / 2.0);

		std::vector<Vector3> initialVertices;
		std::vector<uint32_t> initialIndices;

		initialVertices = std::vector<Vector3>{
			Vector3::Normalize({ ratio, 0.0, -1.0 }) * scale,
			Vector3::Normalize({ -ratio, 0.0, -1.0 }) * scale,
			Vector3::Normalize({ ratio, 0.0, 1.0 }) * scale,
			Vector3::Normalize({ -ratio, 0.0, 1.0 }) * scale,
			Vector3::Normalize({ 0.0, -1.0, ratio }) * scale,
			Vector3::Normalize({ 0.0, -1.0, -ratio }) * scale,
			Vector3::Normalize({ 0.0, 1.0, ratio }) * scale,
			Vector3::Normalize({ 0.0, 1.0, -ratio }) * scale,
			Vector3::Normalize({ -1.0, ratio, 0.0 }) * scale,
			Vector3::Normalize({ -1.0, -ratio, 0.0 }) * scale,
			Vector3::Normalize({ 1.0, ratio, 0.0 }) * scale,
			Vector3::Normalize({ 1.0, -ratio, 0.0 }) * scale
		};

		initialIndices = std::vector<uint32_t>{
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

		for (int i = 0; i < initialIndices.size() - 2; i += 3)
		{
			int16_t subdivision = 0;
			double height;

			CPUVertex A, B, C;
			A.Position = initialVertices[initialIndices[i]];
			A.UV = GetUVFromPosition(Vector3::Normalize(A.Position), (double)planet.TerrainData.Width, (double)planet.TerrainData.Height);
			height = GetHeight(A.UV, planet.TerrainData);
			A.Position = Vector3::Normalize(A.Position) * (planet.PlanetData.radius + height);
			
			B.Position = initialVertices[initialIndices[i + 1]];
			B.UV = GetUVFromPosition(Vector3::Normalize(B.Position), (double)planet.TerrainData.Width, (double)planet.TerrainData.Height);
			height = GetHeight(B.UV, planet.TerrainData);
			B.Position = Vector3::Normalize(B.Position) * (planet.PlanetData.radius + height);

			C.Position = initialVertices[initialIndices[i + 2]];
			C.UV = GetUVFromPosition(Vector3::Normalize(C.Position), (double)planet.TerrainData.Width, (double)planet.TerrainData.Height);
			height = GetHeight(C.UV, planet.TerrainData);
			C.Position = Vector3::Normalize(C.Position) * (planet.PlanetData.radius + height);

			Ref<PlanetNode> rootNode = CreateRef<PlanetNode>(A, B, C, 0);
			SubdivideBasePlanet(planet, rootNode, scale);
			sPlanetNodes.emplace_back(rootNode);
		}

		// Stop timing
		auto end = std::chrono::high_resolution_clock::now();

		// Calculate the duration
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

		TOAST_CORE_INFO("Base planet created with %d number of planet nodes, example of child node: %d, time: %dms", sPlanetNodes.size(), sPlanetNodes[0]->ChildNodes.size(), duration);
	}

	void PlanetSystem::DetailObjectPlacement(const PlanetComponent& planet, TerrainObjectComponent& objects, DirectX::XMMATRIX noScaleTransform, DirectX::XMVECTOR& camPos)
	{
		std::vector<DirectX::XMFLOAT3> objectPositions;

		Matrix planetTransform = { noScaleTransform };
		Vector3 cameraPos = { camPos };

		const siv::PerlinNoise& perlin = siv::PerlinNoise(static_cast<uint32_t>(19871102));

		std::vector<Vertex> vertices = planet.RenderMesh->GetVertices();
		std::vector<uint32_t> indices = planet.RenderMesh->GetIndices();
		
		if (indices.size() > 0 && vertices.size() > 0)
		{
			for (int i = 0; i < indices.size() - 2; i += 3)
			{
				Vector3 A = vertices[indices[i]].Position;
				Vector3 B = vertices[indices[i + 1]].Position;
				Vector3 C = vertices[indices[i + 2]].Position;

				double aDistance = (A - cameraPos).LengthSqrt();
				double bDistance = (B - cameraPos).LengthSqrt();
				double cDistance = (C - cameraPos).LengthSqrt();

				if (aDistance < planet.DistanceLUT[(uint32_t)objects.SubdivisionActivation] && bDistance < planet.DistanceLUT[(uint32_t)objects.SubdivisionActivation] && cDistance < planet.DistanceLUT[(uint32_t)objects.SubdivisionActivation])
				{
					Vector2 aUV = vertices[indices[i]].Texcoord;
					Vector2 bUV = vertices[indices[i + 1]].Texcoord;
					Vector2 cUV = vertices[indices[i + 2]].Texcoord;

					Vector2 centerUV = (aUV + bUV + cUV) / 3.0;

					double noiseValue = perlin.octave2D_01(centerUV.x, centerUV.y, 4);
					int stonesInThisTriangle = static_cast<int>(std::round(static_cast<double>(objects.MaxNrOfObjectPerFace) * noiseValue));

					if (stonesInThisTriangle > 0)
					{
						uint32_t seed = HashFace(indices[i], indices[i+1], indices[i+2]);
						std::mt19937 rng(seed);
						std::uniform_real_distribution<double> dist(0.0f, 1.0f);

						for (int j = 0; j < stonesInThisTriangle; ++j) {
							// Generate barycentric coordinates deterministically
							double u = dist(rng);
							double v = dist(rng);
							if (u + v > 1.0f) {
								u = 1.0f - u;
								v = 1.0f - v;
							}
							float w = 1.0f - u - v;

							// Calculate the object's local position
							Vector3 objectPosition = A * u + B * v + C * w;

							objectPositions.emplace_back(DirectX::XMFLOAT3(objectPosition.x, objectPosition.y, objectPosition.z));
						}
					}
				}
			}
		}

		if(objectPositions.size() > 0)
			objects.MeshObject->SetInstanceData(&objectPositions[0], objectPositions.size() * sizeof(DirectX::XMFLOAT3), objectPositions.size());
	}

	void PlanetSystem::TraverseNode(Ref<PlanetNode>& node, PlanetComponent& planet, Vector3& cameraPosPlanetSpace, const Vector3& planetCenter, bool backfaceCull, bool frustumCullActivated, Ref<Frustum>& frustum, Matrix& planetTransform, const siv::PerlinNoise& perlin, TerrainDetailComponent* terrainDetail)
	{
		Vector3 center = (node->A.Position + node->B.Position + node->C.Position) / 3.0;
		Vector3 viewVector = center - cameraPosPlanetSpace;
		double cameraDistance = viewVector.Length();

		double dotProduct = Vector3::Dot(Vector3::Normalize(center), Vector3::Normalize(viewVector));

		double backFaceCullingIgnoreDistance = 50000.0;
		if (cameraDistance > backFaceCullingIgnoreDistance)
		{
			TOAST_PROFILE_SCOPE("Backface culling test");
			std::lock_guard<std::mutex> lock(planetDataMutex);
			if (backfaceCull && dotProduct >= planet.FaceLevelDotLUT[(uint32_t)node->SubdivisionLevel]) 
				return;
		}
		 
		if (frustumCullActivated)
		{
			TOAST_PROFILE_SCOPE("Frustum culling test");
			auto intersect = frustum->ContainsTriangleVolume(Vector3::Normalize(node->A.Position) * planet.PlanetData.radius, Vector3::Normalize(node->B.Position) * planet.PlanetData.radius, Vector3::Normalize(node->C.Position) * planet.PlanetData.radius, planet.HeightMultLUT[node->SubdivisionLevel]);

			if (intersect == VolumeTri::OUTSIDE) 
				return;
		}

		//TOAST_CORE_CRITICAL("node->SubdivisionLevel going to subdivision: %d", node->SubdivisionLevel);

		if (node->SubdivisionLevel >= BASE_PLANET_SUBDIVISIONS)
			SubdivideFace(node->A, node->B, node->C, cameraPosPlanetSpace, planet, planetCenter, planetTransform, BASE_PLANET_SUBDIVISIONS, perlin, terrainDetail);
		else 
		{
			for (auto& child : node->ChildNodes)
				TraverseNode(child, planet, cameraPosPlanetSpace, planetCenter, backfaceCull, frustumCullActivated, frustum, planetTransform, perlin, terrainDetail);
		}
	}

	void PlanetSystem::GeneratePlanet(Ref<Frustum>& frustum, DirectX::XMFLOAT3& scale, const Vector3& planetCenter, DirectX::XMMATRIX noScaleTransform, DirectX::XMVECTOR camPos, bool backfaceCull, bool frustumCullActivated,  PlanetComponent& planet, std::unordered_map<std::pair<int, int>, Ref<ShapeBox>, PairHash>& terrainColliders, std::unordered_map<std::pair<int, int>, std::vector<Vector3>, PairHash>& terrainColliderPositions, TerrainDetailComponent* terrainDetail)
	{
		TOAST_PROFILE_FUNCTION();

		auto start = std::chrono::high_resolution_clock::now();

		//TOAST_CORE_INFO("Planet build started on planet thread");

		planetGenerationOngoing.store(true);

		siv::PerlinNoise perlin;

		if (terrainDetail)
			perlin = siv::PerlinNoise(static_cast<uint32_t>(terrainDetail->Seed));

		int triangleAdded = 0;

		Matrix planetTransform = { noScaleTransform };
		Vector3 cameraPos = { camPos };

		Vector3 cameraPosPlanetSpace = Matrix::Inverse(planetTransform) * cameraPos;
		
		{
			std::lock_guard<std::mutex> lock(planetDataMutex);

			planet.VertexMap.clear();
			planet.BuildVertices.clear();
			planet.BuildIndices.clear();

			planet.TerrainChunks.clear();
		}

		{
			std::lock_guard<std::mutex> lock(terrainCollidersMutex);
			terrainColliders.clear();
			terrainColliderPositions.clear();
		}

		{
			TOAST_PROFILE_SCOPE("Looping through the tree structure!");

			for (auto& node : sPlanetNodes) 
				TraverseNode(node, planet, cameraPosPlanetSpace, planetCenter, backfaceCull, frustumCullActivated, frustum, planetTransform, perlin, terrainDetail);

			for (auto& vertex : planet.BuildVertices) {
				Vector3 normal(vertex.Normal.x, vertex.Normal.y, vertex.Normal.z);
				normal = Vector3::Normalize(normal);
				vertex.Normal = { (float)normal.x, (float)normal.y, (float)normal.z };
			}
		}

		for (const auto& chunkEntry : planet.TerrainChunks)
		{
			const auto& chunkKey = chunkEntry.first;
			const auto& verticesInChunk = chunkEntry.second;

			if (verticesInChunk.empty()) 
				continue;

			{
				std::lock_guard<std::mutex> lock(terrainCollidersMutex);
				terrainColliderPositions[chunkKey].insert(terrainColliderPositions[chunkKey].end(), verticesInChunk.begin(), verticesInChunk.end());
			}

			Bounds chunkBounds;
			GetVerticesBounds(verticesInChunk, chunkBounds);

			// Create a collider for the chunk
			Ref<ShapeBox> collider = CreateRef<ShapeBox>();
			collider->SetBounds(chunkBounds);

			// Add the collider to the list
			terrainColliders[chunkKey] = collider;
		}

		newPlanetReady.store(true);
		planetGenerationOngoing.store(false);

		// Stop timing
		auto end = std::chrono::high_resolution_clock::now();

		// Calculate the duration
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

		//TOAST_CORE_INFO("Planet created with %d number of vertices and %d number indices, time: %dms", planet.BuildVertices.size(), planet.BuildIndices.size(), duration.count());

		return;
	}

	void PlanetSystem::RegeneratePlanet(Ref<Frustum>& frustum, DirectX::XMFLOAT3& scale, const Vector3& planetCenter, DirectX::XMMATRIX noScaleTransform, DirectX::XMVECTOR camPos, bool backfaceCull, bool frustumCullActivated, PlanetComponent& planet, std::unordered_map<std::pair<int, int>, Ref<ShapeBox>, PairHash>& terrainColliders, std::unordered_map<std::pair<int, int>, std::vector<Vector3>, PairHash>& terrainColliderPositions, TerrainDetailComponent* terrainDetail)
	{
		if (generationFuture.valid() && generationFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
			return;

		if (!newPlanetReady.load() && !planetGenerationOngoing.load())
		{
			generationFuture = std::async(std::launch::async, &PlanetSystem::GeneratePlanet,
				std::ref(frustum),
				std::ref(scale),
				std::ref(planetCenter),
				std::ref(noScaleTransform),
				camPos,
				backfaceCull,
				frustumCullActivated,
				std::ref(planet),
				std::ref(terrainColliders),
				std::ref(terrainColliderPositions),
				terrainDetail);
		}

		return;
	}

	void PlanetSystem::UpdatePlanet(Ref<Mesh>& renderPlanet, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, TerrainColliderComponent& terrainCollider)
	{
		std::lock_guard<std::mutex> lock(planetDataMutex);
		if (newPlanetReady.load())
		{
			{
				std::lock_guard<std::mutex> lock(terrainCollidersMutex);
				terrainCollider.Colliders = terrainCollider.BuildColliders;
				terrainCollider.ColliderPositions = terrainCollider.BuildColliderPositions;
			}

			renderPlanet->mLODGroups[0]->Vertices = vertices;
			renderPlanet->mLODGroups[0]->Indices = indices;
			renderPlanet->InvalidatePlanet();

			newPlanetReady.store(false);
		}
	}

	double PlanetSystem::GetHeight(Vector2 uvCoords, TerrainData& terrainData)
	{
		uint32_t x1 = (uint32_t)(uvCoords.x);
		uint32_t y1 = (uint32_t)(uvCoords.y);

		uint32_t x2 = x1 == (terrainData.Width - 1) ? 0 : x1 + 1;
		uint32_t y2 = y1 == (terrainData.Height - 1) ? 0 : y1 + 1;

		double Q11 = static_cast<double>(terrainData.HeightData[y1 * (terrainData.RowPitch / 2) + x1]);
		double Q12 = static_cast<double>(terrainData.HeightData[y2 * (terrainData.RowPitch / 2) + x1]);
		double Q21 = static_cast<double>(terrainData.HeightData[y1 * (terrainData.RowPitch / 2) + x2]);
		double Q22 = static_cast<double>(terrainData.HeightData[y2 * (terrainData.RowPitch / 2) + x2]);

		return Math::BilinearInterpolation(uvCoords, Q11, Q12, Q21, Q22);
	}

	void PlanetSystem::Shutdown()
	{
		if (generationFuture.valid()) {
			generationFuture.wait();
		}
	}

	void PlanetSystem::GenerateDistanceLUT(std::vector<double>& distanceLUT, float radius, float FoV, float viewportSizeX)
	{
		distanceLUT.clear();

		distanceLUT.emplace_back(100000.0 * 100000.0);
		distanceLUT.emplace_back(50000.0 * 50000.0);
		distanceLUT.emplace_back(25000.0 * 25000.0);
		distanceLUT.emplace_back(15000.0 * 15000.0);
		distanceLUT.emplace_back(10000.0 * 10000.0);
		distanceLUT.emplace_back(7500.0 * 7500.0);
		distanceLUT.emplace_back(5000.0 * 5000.0);
		distanceLUT.emplace_back(2500.0 * 2500.0);
		distanceLUT.emplace_back(2000.0 * 2000.0);
		distanceLUT.emplace_back(1500.0 * 1500.0);
		distanceLUT.emplace_back(1000.0 * 1000.0);
		distanceLUT.emplace_back(750.0 * 750.0);
		distanceLUT.emplace_back(500.0 * 500.0);
		distanceLUT.emplace_back(250.0 * 250.0);
		distanceLUT.emplace_back(150.0 * 150.0);
		distanceLUT.emplace_back(100.0 * 100.0);
		distanceLUT.emplace_back(75.0 * 75);
		distanceLUT.emplace_back(50.0 * 50.0);
		distanceLUT.emplace_back(25.0 * 25.0);
		distanceLUT.emplace_back(15.0 * 15.0);
		distanceLUT.emplace_back(5.0 * 5.0);

		//double startLog = log(radius * 6.0);  // Starting log value for 1,000,000
		//double endLog = log(radius * 0.00001475143826523086);  // Ending log value for 50

		//for (int level = 0; level < 20; level++)
		//{
		//	// Interpolate between startLog and endLog based on current level
		//	double t = static_cast<double>(level) / MAX_SUBDIVISION;
		//	double currentLog = (1 - t) * startLog + t * endLog;

		//	// Convert the logarithmic value back to linear space
		//	double currentDistance = exp(currentLog);

		//	distanceLUT.emplace_back(currentDistance);
		//}

		//int i = 0;
		//for (auto level : distanceLUT)
		//{
		//	i++;
		//	TOAST_CORE_INFO("distanceLUT[%d]: %f", i, level);
		//}
	}

	void PlanetSystem::GenerateFaceDotLevelLUT(std::vector<double>& faceLevelDotLUT, float planetRadius, float maxHeight)
	{
		std::lock_guard<std::mutex> lock(planetDataMutex);

		float cullingAngle = acos((double)planetRadius / ((double)planetRadius + (double)maxHeight));

		faceLevelDotLUT.clear();
		faceLevelDotLUT.emplace_back(0.5 + sinf(cullingAngle));
		double angle = acos(0.5);
		for (int i = 1; i <= MAX_SUBDIVISION; i++)
		{
			angle *= 0.5;
			faceLevelDotLUT.emplace_back(sin(angle + cullingAngle));
		}

		//for (auto level : faceLevelDotLUT)
		//	TOAST_CORE_INFO("FacelevelDotLUT: %f", level);
	}

	void PlanetSystem::GenerateHeightMultLUT(std::vector<double>& heightMultLUT, double planetRadius, double maxHeight)
	{
		heightMultLUT.clear();

		double ratio = ((1.0 + sqrt(5.0)) / 2.0);

		std::vector<Vector3> vertices = std::vector<Vector3>{
			Vector3::Normalize({ -ratio, 0.0, -1.0 }) * planetRadius,
			Vector3::Normalize({ -ratio, 0.0, 1.0 }) * planetRadius,
			Vector3::Normalize({ -1.0, ratio, 0.0 }) * planetRadius,
		};

		Vector3 a = vertices[0];
		Vector3 b = vertices[1];
		Vector3 c = vertices[2];

		Vector3 center = (a + b + c) / 3.0;

		center *= planetRadius / (center.Length() + maxHeight);
		heightMultLUT.push_back(1.0 / Vector3::Dot(Vector3::Normalize(a), Vector3::Normalize(center)) - 1.0);
		double normMaxHeight = maxHeight / planetRadius;

		for (int i = 1; i <= MAX_SUBDIVISION; i++)
		{
			Vector3 A = b + ((c - b) * 0.5);
			Vector3 B = c + ((a - c) * 0.5);
			c = a + ((b - a) * 0.5);
			a = A * planetRadius / A.Length();
			b = B * planetRadius / B.Length();
			c *= planetRadius / c.Length();
			heightMultLUT.push_back((1.0 / Vector3::Dot(Vector3::Normalize(a), Vector3::Normalize(center)) + normMaxHeight) - 1.0);
		}

		//for (auto level : heightMultLUT)
		//	TOAST_CORE_INFO("heightMultLUT: %lf", level);
	}

	uint32_t PlanetSystem::GetOrAddVector3(std::unordered_map<Vector3, uint32_t, Vector3::Hasher, Vector3::Equal>& vertexMap, const Vector3& vertex, std::vector<Vector3>& vertices)
	{
		TOAST_PROFILE_FUNCTION();

		auto it = vertexMap.find(vertex);
		if (it != vertexMap.end()) {
			return it->second;
		}
		else {
			vertices.emplace_back(vertex);
			uint32_t newIndex = vertices.size() - 1;
			vertexMap[vertex] = newIndex;
			return newIndex;
		}
	}

	void PlanetSystem::AssignFaceToChunk(const Vector3& vecA, const Vector3& vecB, const Vector3& vecC,
		std::unordered_map<std::pair<int, int>, std::vector<Vector3>, PairHash>& chunks,
		const Vector3& planetCenter)
	{
		Vector3 centerPoint = (vecA + vecB + vecC) / 3.0;

		// Compute the direction vector from the planet's center to the vertex
		Vector3 direction = centerPoint - planetCenter;
		Vector3 normalizedDirection = Vector3::Normalize(direction);

		// Convert to spherical coordinates
		double latitude = std::asin(normalizedDirection.y) * (180.0 / M_PI); // Degrees
		double longitude = std::atan2(normalizedDirection.z, normalizedDirection.x) * (180.0 / M_PI);
		if (longitude < 0.0)
			longitude += 360.0;

		// Determine bin indices
		const int NUM_LATITUDE_BINS = 18;   // Adjust as needed
		const int NUM_LONGITUDE_BINS = 36;  // Adjust as needed

		int latIndex = static_cast<int>((latitude + 90.0) / (180.0 / NUM_LATITUDE_BINS));
		int lonIndex = static_cast<int>(longitude / (360.0 / NUM_LONGITUDE_BINS));

		// Clamp indices to valid ranges
		latIndex = (std::min)(latIndex, NUM_LATITUDE_BINS - 1);
		lonIndex = (std::min)(lonIndex, NUM_LONGITUDE_BINS - 1);

		// Create the chunk key
		std::pair<int, int> chunkKey = { latIndex, lonIndex };

		// Add the vertex to the chunk
		chunks[chunkKey].emplace_back(vecA);
		chunks[chunkKey].emplace_back(vecB);
		chunks[chunkKey].emplace_back(vecC);
	}

	void PlanetSystem::GetVerticesBounds(const std::vector<Vector3>& vertices, Bounds& bounds)
	{
		if (vertices.empty())
		{
			bounds = Bounds();
			return;
		}

		// Initialize min and max with the first vertex
		Vector3 min = vertices[0];
		Vector3 max = vertices[0];

		// Iterate over all vertices
		for (const auto& vertex : vertices)
		{
			min.x = (std::min)(min.x, vertex.x);
			min.y = (std::min)(min.y, vertex.y);
			min.z = (std::min)(min.z, vertex.z);

			max.x = (std::max)(max.x, vertex.x);
			max.y = (std::max)(max.y, vertex.y);
			max.z = (std::max)(max.z, vertex.z);
		}

		bounds.mins = min;
		bounds.maxs = max;
	}

}