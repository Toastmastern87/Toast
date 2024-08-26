#include "tpch.h"

#include "PlanetSystem.h"

#include "Toast/Scene/Components.h"

#include <chrono>

#define BASE_PLANET_SUBDIVISIONS 7

namespace Toast {

	std::mutex PlanetSystem::planetDataMutex;
	std::future<void> PlanetSystem::generationFuture;
	std::atomic<bool> PlanetSystem::newPlanetReady{ false };
	std::atomic<bool> PlanetSystem::planetGenerationOngoing{ false };

	std::vector<Ref<PlanetNode>> PlanetSystem::sPlanetNodes;

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

	void PlanetSystem::SubdivideFace(CPUVertex& A, CPUVertex& B, CPUVertex& C, Vector3& cameraPosPlanetSpace, PlanetComponent& planet, Matrix& planetTransform, uint16_t subdivision)
	{
		double height;
		NextPlanetFace nextFace;
		Vector2 uvCoords;

		double aDistance = (A.Position - cameraPosPlanetSpace).MagnitudeSqrt();
		double bDistance = (B.Position - cameraPosPlanetSpace).MagnitudeSqrt();
		double cDistance = (C.Position - cameraPosPlanetSpace).MagnitudeSqrt();

		if (subdivision >= BASE_PLANET_SUBDIVISIONS + planet.Subdivisions)	
		{
			//TOAST_CORE_CRITICAL("LEAF Due to subdivision");
			nextFace = NextPlanetFace::LEAF;
		}
		else
		{
			//TOAST_CORE_CRITICAL("aDistance: %lf, bDistance: %lf, cDistance: %lf", aDistance, bDistance, cDistance);

			if (aDistance < planet.DistanceLUT[(uint32_t)subdivision - BASE_PLANET_SUBDIVISIONS] && bDistance < planet.DistanceLUT[(uint32_t)subdivision - BASE_PLANET_SUBDIVISIONS] && cDistance < planet.DistanceLUT[(uint32_t)subdivision - BASE_PLANET_SUBDIVISIONS])
				nextFace = NextPlanetFace::SPLIT;
			else 
			{
				//TOAST_CORE_CRITICAL("LEAF Due to distance");
				nextFace = NextPlanetFace::LEAF;
			}
		}

		if (nextFace == NextPlanetFace::SPLIT)
		{
			CPUVertex a, b, c;

			a.Position = B.Position + ((C.Position - B.Position) * 0.5);
			b.Position = C.Position + ((A.Position - C.Position) * 0.5);
			c.Position = A.Position + ((B.Position - A.Position) * 0.5);

			Vector3 aNormalized = Vector3::Normalize(a.Position);
			a.UV = GetUVFromPosition(aNormalized, (double)planet.TerrainData.Width, (double)planet.TerrainData.Height);
			height = GetHeight(a.UV, planet.TerrainData);
			a.Position = aNormalized * (planet.PlanetData.radius + height);

			Vector3 bNormalized = Vector3::Normalize(b.Position);
			b.UV = GetUVFromPosition(bNormalized, (double)planet.TerrainData.Width, (double)planet.TerrainData.Height);
			height = GetHeight(b.UV, planet.TerrainData);
			b.Position = bNormalized * (planet.PlanetData.radius + height);

			Vector3 cNormalized = Vector3::Normalize(c.Position);
			c.UV = GetUVFromPosition(cNormalized, (double)planet.TerrainData.Width, (double)planet.TerrainData.Height);
			height = GetHeight(c.UV, planet.TerrainData);
			c.Position = cNormalized * (planet.PlanetData.radius + height);

			int16_t nextSubdivision = subdivision + 1;

			SubdivideFace(a, b, c, cameraPosPlanetSpace, planet, planetTransform, nextSubdivision);
			SubdivideFace(c, b, A, cameraPosPlanetSpace, planet, planetTransform, nextSubdivision);
			SubdivideFace(B, a, c, cameraPosPlanetSpace, planet, planetTransform, nextSubdivision);
			SubdivideFace(b, a, C, cameraPosPlanetSpace, planet, planetTransform, nextSubdivision);
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

			if (!crackTriangle)
			{
				Vector3 vecA = planetTransform * A.Position;
				Vector3 vecB = planetTransform * B.Position;
				Vector3 vecC = planetTransform * C.Position;

				Vector3 normal = Vector3::Normalize(Vector3::Cross(vecB - vecA, vecC - vecA));

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
			else
			{
				// Calculate new vertex	
				CPUVertex additionalVertex;
				additionalVertex.Position = (closestVertex.Position + middleVertex.Position) * 0.5;
				Vector3 additionalVertexNormalized = Vector3::Normalize(additionalVertex.Position);
				additionalVertex.UV = GetUVFromPosition(additionalVertexNormalized, (double)planet.TerrainData.Width, (double)planet.TerrainData.Height);
				double height = GetHeight(additionalVertex.UV, planet.TerrainData);
				additionalVertex.Position = additionalVertexNormalized * (planet.PlanetData.radius + height);

				Vector3 additionalVertexPos = planetTransform * additionalVertex.Position;
				Vector3 closestVertexPos = planetTransform * closestVertex.Position;
				Vector3 middleVertexPos = planetTransform * middleVertex.Position;
				Vector3 furthestVertexPos = planetTransform * furthestVertex.Position;

				// First triangle
				Vector3 normal = Vector3::Normalize(Vector3::Cross(additionalVertexPos - closestVertexPos, additionalVertexPos - furthestVertexPos));

				if (normal.y < 0.0)
					normal = normal * -1.0;

				Vertex vertexA = Vertex(additionalVertexPos, additionalVertex.UV, normal);
				planet.BuildVertices.emplace_back(vertexA);
				planet.BuildIndices.emplace_back(planet.BuildVertices.size() - 1);

				Vertex vertexB = Vertex(closestVertexPos, closestVertex.UV, normal);
				planet.BuildVertices.emplace_back(vertexB);
				planet.BuildIndices.emplace_back(planet.BuildVertices.size() - 1);

				Vertex vertexC = Vertex(furthestVertexPos, furthestVertex.UV, normal);
				planet.BuildVertices.emplace_back(vertexC);
				planet.BuildIndices.emplace_back(planet.BuildVertices.size() - 1);

				// Second triangle
				normal = Vector3::Normalize(Vector3::Cross(additionalVertexPos - furthestVertexPos, additionalVertexPos - middleVertexPos));
				if (normal.y < 0.0)
					normal = normal * -1.0;

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

	void PlanetSystem::SubdivideFace(Vector3& cameraPosPlanetSpace, Matrix& planetScaleTransform, Ref<Frustum>& frustum, int& triangleAdded, Matrix planetTransform, Vector3& a, Vector3& b, Vector3& c, int16_t& subdivision, bool backfaceCull, bool frustumCullActivated, bool frustumCull, PlanetComponent& planet)
	{
		TOAST_PROFILE_FUNCTION();

		Vector3 A, B, C;

		NextPlanetFace nextPlanetFace = CheckFaceSplit(cameraPosPlanetSpace, planetScaleTransform, subdivision, a, b, c, planet);

		if (nextPlanetFace == NextPlanetFace::CULL)
			return;

		if (nextPlanetFace == NextPlanetFace::SPLIT || nextPlanetFace == NextPlanetFace::SPLITCULL)
		{
			A = b + ((c - b) * 0.5);
			B = c + ((a - c) * 0.5);
			C = a + ((b - a) * 0.5);

			//A = Vector3::Normalize(A) * planet.PlanetData.radius;
			//B = Vector3::Normalize(B) * planet.PlanetData.radius;
			//C = Vector3::Normalize(C) * planet.PlanetData.radius;

			int16_t nextSubdivision = subdivision + 1;

			SubdivideFace(cameraPosPlanetSpace, planetScaleTransform, frustum, triangleAdded, planetTransform, A, B, C, nextSubdivision, backfaceCull, frustumCullActivated, nextPlanetFace == NextPlanetFace::SPLITCULL, planet);
			SubdivideFace(cameraPosPlanetSpace, planetScaleTransform, frustum, triangleAdded, planetTransform, C, B, a, nextSubdivision, backfaceCull, frustumCullActivated, nextPlanetFace == NextPlanetFace::SPLITCULL, planet);
			SubdivideFace(cameraPosPlanetSpace, planetScaleTransform, frustum, triangleAdded, planetTransform, b, A, C, nextSubdivision, backfaceCull, frustumCullActivated, nextPlanetFace == NextPlanetFace::SPLITCULL, planet);
			SubdivideFace(cameraPosPlanetSpace, planetScaleTransform, frustum, triangleAdded, planetTransform, B, A, c, nextSubdivision, backfaceCull, frustumCullActivated, nextPlanetFace == NextPlanetFace::SPLITCULL, planet);
		}
		else
		{
			TOAST_PROFILE_SCOPE("New triangle added to the planet")
			bool crackTriangle = false;
			Vector3 closestVertex, furthestVertex, middleVertex;

			if (subdivision < 19 && subdivision < planet.Subdivisions)
			{
				// Copy to do changes on the on the copied vertices and not the original ones
				Vector3 testA = a;
				Vector3 testB = b;
				Vector3 testC = c;

				Vector2 uvCoord;
				if (!planet.TerrainData.HeightData.empty())
				{
					uvCoord = GetUVFromPosition(testA, planet.TerrainData.Width, planet.TerrainData.Height);
					testA = Vector3::Normalize(testA) * (planet.PlanetData.radius + planet.TerrainData.HeightData[((uint32_t)uvCoord.y * (planet.TerrainData.RowPitch / 2) + (uint32_t)uvCoord.x)]);
					uvCoord = GetUVFromPosition(testB, planet.TerrainData.Width, planet.TerrainData.Height);
					testB = Vector3::Normalize(testB) * (planet.PlanetData.radius + planet.TerrainData.HeightData[((uint32_t)uvCoord.y * (planet.TerrainData.RowPitch / 2) + (uint32_t)uvCoord.x)]);
					uvCoord = GetUVFromPosition(testC, planet.TerrainData.Width, planet.TerrainData.Height);
					testC = Vector3::Normalize(testC) * (planet.PlanetData.radius + planet.TerrainData.HeightData[((uint32_t)uvCoord.y * (planet.TerrainData.RowPitch / 2) + (uint32_t)uvCoord.x)]);
				}

				double cDistance = (testC - cameraPosPlanetSpace).MagnitudeSqrt();
				double aDistance = (testA - cameraPosPlanetSpace).MagnitudeSqrt();
				double bDistance = (testB - cameraPosPlanetSpace).MagnitudeSqrt();

				double closestDistance = (std::min)(aDistance, (std::min)(bDistance, cDistance));
				double furthestDistance = (std::max)(aDistance, (std::max)(bDistance, cDistance));
				double secondClosestDistance;

				if (closestDistance == aDistance)
					closestVertex = a;
				else if (closestDistance == bDistance)
					closestVertex = b;
				else
					closestVertex = c;

				if (furthestDistance == aDistance)
					furthestVertex = a;
				else if (furthestDistance == bDistance)
					furthestVertex = b;
				else
					furthestVertex = c;

				if (closestDistance == aDistance)
					secondClosestDistance = (furthestDistance == bDistance) ? cDistance : bDistance;
				else if (closestDistance == bDistance)
					secondClosestDistance = (furthestDistance == aDistance) ? cDistance : aDistance;
				else
					secondClosestDistance = (furthestDistance == aDistance) ? bDistance : aDistance;

				// Identify middle vertex based on distances
				if ((closestDistance != aDistance) && (furthestDistance != aDistance))
					middleVertex = a;
				else if ((closestDistance != bDistance) && (furthestDistance != bDistance))
					middleVertex = b;
				else
					middleVertex = c;

				if (closestDistance < planet.DistanceLUT[subdivision] && secondClosestDistance < planet.DistanceLUT[subdivision])
					crackTriangle = true;
			}

			// Back to normal code
			//if (!crackTriangle)
			//{
				planet.BuildVertices.emplace_back(a);
				planet.BuildIndices.emplace_back(planet.BuildVertices.size() - 1);
				//if (triangleAdded == 0 || triangleAdded == 3 || triangleAdded == 8 || triangleAdded == 9) {
				//a.ToString();
				//uint32_t currentIndexA = GetOrAddVertex(planet.VertexMap, a, planet.BuildVertices);
				//planet.BuildIndices.emplace_back(currentIndexA);

				//b.ToString();
				planet.BuildVertices.emplace_back(b);
				planet.BuildIndices.emplace_back(planet.BuildVertices.size() - 1);
				//uint32_t currentIndexB = GetOrAddVertex(planet.VertexMap, b, planet.BuildVertices);
				//planet.BuildIndices.emplace_back(currentIndexB);

				//c.ToString();
				planet.BuildVertices.emplace_back(c);
				planet.BuildIndices.emplace_back(planet.BuildVertices.size() - 1);
				//uint32_t currentIndexC = GetOrAddVertex(planet.VertexMap, c, planet.BuildVertices);
				//planet.BuildIndices.emplace_back(currentIndexC);
			//}
			//else
			//{
			//	// Calculate new vertex	
			//	Vertex newVertex = (closestVertex + middleVertex) * 0.5;

			//	// First triangle
			//	uint32_t currentIndexA = GetOrAddVertex(planet.VertexMap, newVertex, planet.BuildVertices);
			//	planet.BuildIndices.emplace_back(currentIndexA);

			//	//b.ToString();
			//	uint32_t currentIndexB = GetOrAddVertex(planet.VertexMap, closestVertex, planet.BuildVertices);
			//	planet.BuildIndices.emplace_back(currentIndexB);

			//	uint32_t currentIndexC = GetOrAddVertex(planet.VertexMap, furthestVertex, planet.BuildVertices);
			//	planet.BuildIndices.emplace_back(currentIndexC);

			//	// Second triangle
			//	uint32_t currentIndexD = GetOrAddVertex(planet.VertexMap, newVertex, planet.BuildVertices);
			//	planet.BuildIndices.emplace_back(currentIndexD);

			//	//b.ToString();
			//	uint32_t currentIndexE = GetOrAddVertex(planet.VertexMap, middleVertex, planet.BuildVertices);
			//	planet.BuildIndices.emplace_back(currentIndexE);

			//	uint32_t currentIndexF = GetOrAddVertex(planet.VertexMap, furthestVertex, planet.BuildVertices);
			//	planet.BuildIndices.emplace_back(currentIndexF);
			//}

			triangleAdded++;
		}
	}

	void PlanetSystem::TraverseNode(Ref<PlanetNode>& node, PlanetComponent& planet, Vector3& cameraPosPlanetSpace, bool backfaceCull, bool frustumCullActivated, Ref<Frustum>& frustum, Matrix& planetTransform)
	{
		Vector3 center = (node->A.Position + node->B.Position + node->C.Position) / 3.0;
		Vector3 viewVector = center - cameraPosPlanetSpace;
		double cameraDistance = viewVector.Magnitude();

		double dotProduct = Vector3::Dot(Vector3::Normalize(center), Vector3::Normalize(viewVector));

		double backFaceCullingIgnoreDistance = 50000.0;
		if (cameraDistance > backFaceCullingIgnoreDistance)
		{
			TOAST_PROFILE_SCOPE("Backface culling test");
			std::lock_guard<std::mutex> lock(planetDataMutex);
			if (backfaceCull && dotProduct >= planet.FaceLevelDotLUT[(uint32_t)node->SubdivisionLevel]) 
			{
				//TOAST_CORE_CRITICAL("BACKFACE CULLING");

				return;
			}
		}

		if (frustumCullActivated)
		{
			TOAST_PROFILE_SCOPE("Frustum culling test");
			auto intersect = frustum->ContainsTriangleVolume(Vector3::Normalize(node->A.Position) * planet.PlanetData.radius, Vector3::Normalize(node->B.Position) * planet.PlanetData.radius, Vector3::Normalize(node->C.Position) * planet.PlanetData.radius, planet.HeightMultLUT[node->SubdivisionLevel]);

			if (intersect == VolumeTri::OUTSIDE) 
			{
				//TOAST_CORE_CRITICAL("FRUSTUM CULLING");
				return;
			}
		}

		//TOAST_CORE_CRITICAL("node->SubdivisionLevel going to subdivision: %d", node->SubdivisionLevel);

		if (node->SubdivisionLevel >= BASE_PLANET_SUBDIVISIONS)
			SubdivideFace(node->A, node->B, node->C, cameraPosPlanetSpace, planet, planetTransform, BASE_PLANET_SUBDIVISIONS);
		else 
		{
			for (auto& child : node->ChildNodes)
				TraverseNode(child, planet, cameraPosPlanetSpace, backfaceCull, frustumCullActivated, frustum, planetTransform);
		}
	}

	void PlanetSystem::GeneratePlanet(Ref<Frustum>& frustum, DirectX::XMFLOAT3& scale, DirectX::XMMATRIX noScaleTransform, DirectX::XMVECTOR camPos, bool backfaceCull, bool frustumCullActivated,  PlanetComponent& planet, TerrainDetailComponent* terrainDetail)
	{
		TOAST_PROFILE_FUNCTION();

		auto start = std::chrono::high_resolution_clock::now();

		//TOAST_CORE_INFO("Planet build started on planet thread");

		planetGenerationOngoing.store(true);

		if (terrainDetail)
			const siv::PerlinNoise perlin{ (uint32_t)(terrainDetail->Seed) };

		int triangleAdded = 0;

		//DirectX::XMMATRIX scaleMatrix = DirectX::XMMatrixIdentity() * DirectX::XMMatrixScaling(scale.x, scale.y, scale.z);
		//Matrix scaleTransform = { scaleMatrix };

		Matrix planetTransform = { noScaleTransform };
		Vector3 cameraPos = { camPos };

		Vector3 cameraPosPlanetSpace = Matrix::Inverse(planetTransform) * cameraPos;
		
		{
			std::lock_guard<std::mutex> lock(planetDataMutex);

			planet.BuildVertices.clear();
			planet.BuildIndices.clear();
		}

		{
			TOAST_PROFILE_SCOPE("Looping through the tree structure!");

			for (auto& node : sPlanetNodes) 
				TraverseNode(node, planet, cameraPosPlanetSpace, backfaceCull, frustumCullActivated, frustum, planetTransform);
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

	void PlanetSystem::RegeneratePlanet(Ref<Frustum>& frustum, DirectX::XMFLOAT3& scale, DirectX::XMMATRIX noScaleTransform, DirectX::XMVECTOR camPos, bool backfaceCull, bool frustumCullActivated, PlanetComponent& planet, TerrainDetailComponent* terrainDetail)
	{
		if (generationFuture.valid() && generationFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
			return;

		if (!newPlanetReady.load() && !planetGenerationOngoing.load())
		{
			generationFuture = std::async(std::launch::async, &PlanetSystem::GeneratePlanet,
				std::ref(frustum),
				std::ref(scale),
				std::ref(noScaleTransform),
				camPos,
				backfaceCull,
				frustumCullActivated,
				std::ref(planet),
				terrainDetail);
		}

		return;
	}

	void PlanetSystem::UpdatePlanet(Ref<Mesh>& renderPlanet, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices)
	{
		std::lock_guard<std::mutex> lock(planetDataMutex);
		if (newPlanetReady.load())
		{
			renderPlanet->mVertices = vertices;
			renderPlanet->mIndices = indices;
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

		int i = 0;
		for (auto level : distanceLUT)
		{
			i++;
			TOAST_CORE_INFO("distanceLUT[%d]: %f", i, level);
		}
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

		center *= planetRadius / (center.Magnitude() + maxHeight);
		heightMultLUT.push_back(1.0 / Vector3::Dot(Vector3::Normalize(a), Vector3::Normalize(center)) - 1.0);
		double normMaxHeight = maxHeight / planetRadius;

		for (int i = 1; i <= MAX_SUBDIVISION; i++)
		{
			Vector3 A = b + ((c - b) * 0.5);
			Vector3 B = c + ((a - c) * 0.5);
			c = a + ((b - a) * 0.5);
			a = A * planetRadius / A.Magnitude();
			b = B * planetRadius / B.Magnitude();
			c *= planetRadius / c.Magnitude();
			heightMultLUT.push_back((1.0 / Vector3::Dot(Vector3::Normalize(a), Vector3::Normalize(center)) + normMaxHeight) - 1.0);
		}

		//for (auto level : heightMultLUT)
		//	TOAST_CORE_INFO("heightMultLUT: %lf", level);
	}

	PlanetSystem::NextPlanetFace PlanetSystem::CheckFaceSplit(Vector3& cameraPosPlanetSpace, Matrix& planetScaleTransform, int16_t subdivision, Vector3 a, Vector3 b, Vector3 c, PlanetComponent& planet)
	{
		TOAST_PROFILE_FUNCTION();

		// Gets the final scaled position, the points are copied not to effect the LOD transition, this is just the check for what to do with the face
		Vector2 uvCoord;
		Vector3 aNoHeight, bNoHeight, cNoHeight;
		Vector3 aHeight, bHeight, cHeight;

		aNoHeight = Vector3::Normalize(a) * planet.PlanetData.radius;
		bNoHeight = Vector3::Normalize(b) * planet.PlanetData.radius;
		cNoHeight = Vector3::Normalize(c) * planet.PlanetData.radius;

		if (!planet.TerrainData.HeightData.empty())
		{
			TOAST_PROFILE_SCOPE("Adding height to planet!");
			{
				TOAST_PROFILE_SCOPE("Getting uv coords!");
				uvCoord = GetUVFromPosition(a, planet.TerrainData.Width, planet.TerrainData.Height);
			}
			{
				TOAST_PROFILE_SCOPE("Adding height to aHeight!");
				aHeight = Vector3::Normalize(a) * (planet.PlanetData.radius + planet.TerrainData.HeightData[((uint32_t)uvCoord.y * (planet.TerrainData.RowPitch / 2) + (uint32_t)uvCoord.x)]);
			}

			uvCoord = GetUVFromPosition(b, planet.TerrainData.Width, planet.TerrainData.Height);
			bHeight = Vector3::Normalize(b) * (planet.PlanetData.radius + planet.TerrainData.HeightData[((uint32_t)uvCoord.y * (planet.TerrainData.RowPitch / 2) + (uint32_t)uvCoord.x)]);
			uvCoord = GetUVFromPosition(c, planet.TerrainData.Width, planet.TerrainData.Height);
			cHeight = Vector3::Normalize(c) * (planet.PlanetData.radius + planet.TerrainData.HeightData[((uint32_t)uvCoord.y * (planet.TerrainData.RowPitch / 2) + (uint32_t)uvCoord.x)]);
		}
		else
		{
			aHeight = aNoHeight;
			bHeight = bNoHeight;
			cHeight = cNoHeight;
		}

		double aDistance = (aHeight - cameraPosPlanetSpace).Magnitude();
		double bDistance = (bHeight - cameraPosPlanetSpace).Magnitude();
		double cDistance = (cHeight - cameraPosPlanetSpace).Magnitude();

		if (subdivision >= planet.Subdivisions)
			return NextPlanetFace::LEAF;

		if (aDistance < planet.DistanceLUT[(uint32_t)subdivision] && bDistance < planet.DistanceLUT[(uint32_t)subdivision] && cDistance < planet.DistanceLUT[(uint32_t)subdivision])
			return NextPlanetFace::SPLITCULL;

		return NextPlanetFace::LEAF;
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

}