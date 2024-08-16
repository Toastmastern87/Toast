#include "tpch.h"

#include "PlanetSystem.h"

#include "Toast/Scene/Components.h"

#include <chrono>

#define MAX_SUBDIVISION 9

namespace Toast {

	std::mutex PlanetSystem::planetDataMutex;
	std::future<void> PlanetSystem::generationFuture;
	std::atomic<bool> PlanetSystem::newPlanetReady{ false };
	std::atomic<bool> PlanetSystem::planetGenerationOngoing{ false };

	std::vector<std::tuple<double, double>> PlanetSystem::sBarycentricCoordinates;

	std::vector<Vector3> PlanetSystem::sBaseVertices;
	std::vector<uint32_t> PlanetSystem::sBaseIndices;
	std::unordered_map<Vector3, uint32_t, Vector3::Hasher, Vector3::Equal> PlanetSystem::sBaseVertexMap;

	Vector2 PlanetSystem::GetUVFromPosition(Vector3& pos, double width, double height)
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

	DirectX::XMFLOAT2 PlanetSystem::GetGPUUVFromPosition(Vector3& pos)
	{
		Vector3 posNormalized = Vector3::Normalize(pos);

		double theta = atan2(posNormalized.z, posNormalized.x);
		double phi = asin(posNormalized.y);

		Vector2 uv = Vector2(theta / M_PI, phi / M_PIDIV2);
		uv.x = uv.x * 0.5 + 0.5;
		uv.y = uv.y * 0.5 + 0.5;

		DirectX::XMFLOAT2 uvFloat = { (float)uv.x, (float)uv.y };

		return uvFloat;
	}

	void PlanetSystem::CalculateBasePlanet(double scale)
	{
		TOAST_PROFILE_FUNCTION();

		auto start = std::chrono::high_resolution_clock::now();

		sBaseVertexMap.clear();
		sBaseVertices.clear();
		sBaseIndices.clear();

		sBaseVertices.reserve(20 * std::pow(4, MAX_SUBDIVISION));
		sBaseIndices.reserve(20 * std::pow(4, MAX_SUBDIVISION));
		sBaseVertexMap.reserve(20 * std::pow(4, MAX_SUBDIVISION));

		DirectX::XMMATRIX scaleMatrix = DirectX::XMMatrixIdentity() * DirectX::XMMatrixScaling(scale, scale, scale);
		Matrix scaleTransform = { scaleMatrix };

		double ratio = ((1.0 + sqrt(5.0)) / 2.0);

		std::vector<Vector3> vertices;
		std::vector<uint32_t> indices;

		vertices = std::vector<Vector3>{
			scaleTransform * Vector3::Normalize({ ratio, 0.0, -1.0 }),
			scaleTransform * Vector3::Normalize({ -ratio, 0.0, -1.0 }),
			scaleTransform * Vector3::Normalize({ ratio, 0.0, 1.0 }),
			scaleTransform * Vector3::Normalize({ -ratio, 0.0, 1.0 }),
			scaleTransform * Vector3::Normalize({ 0.0, -1.0, ratio }),
			scaleTransform * Vector3::Normalize({ 0.0, -1.0, -ratio }),
			scaleTransform * Vector3::Normalize({ 0.0, 1.0, ratio }),
			scaleTransform * Vector3::Normalize({ 0.0, 1.0, -ratio }),
			scaleTransform * Vector3::Normalize({ -1.0, ratio, 0.0 }),
			scaleTransform * Vector3::Normalize({ -1.0, -ratio, 0.0 }),
			scaleTransform * Vector3::Normalize({ 1.0, ratio, 0.0 }),
			scaleTransform * Vector3::Normalize({ 1.0, -ratio, 0.0 })
		};

		indices = std::vector<uint32_t>{
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

		int subdivisionType = 1;

		// NORMAL SUBDIVISION
		if (subdivisionType == 0) {
			std::vector<uint32_t> newIndices;
			for (int subdivision = 0; subdivision < MAX_SUBDIVISION; subdivision++)
			{
				for (int i = 0; i < indices.size() - 2; i += 3)
				{
					//std::vector<uint32_t> faceIndices;

					Vector3 A = vertices[indices[i]];
					Vector3 B = vertices[indices[i + 1]];
					Vector3 C = vertices[indices[i + 2]];

					int vertexIndexM1 = GetOrAddVector3(sBaseVertexMap, Vector3::Normalize((A + B) * 0.5f), sBaseVertices);
					int vertexIndexM2 = GetOrAddVector3(sBaseVertexMap, Vector3::Normalize((B + C) * 0.5f), sBaseVertices);
					int vertexIndexM3 = GetOrAddVector3(sBaseVertexMap, Vector3::Normalize((C + A) * 0.5f), sBaseVertices);

					newIndices.emplace_back(indices[i]); newIndices.emplace_back(vertexIndexM1); newIndices.emplace_back(vertexIndexM3);
					newIndices.emplace_back(vertexIndexM1); newIndices.emplace_back(indices[i + 1]); newIndices.emplace_back(vertexIndexM2);
					newIndices.emplace_back(vertexIndexM1); newIndices.emplace_back(vertexIndexM2); newIndices.emplace_back(vertexIndexM3);
					newIndices.emplace_back(vertexIndexM3); newIndices.emplace_back(vertexIndexM2); newIndices.emplace_back(indices[i + 2]);
				}

				indices = newIndices;
			}

			sBaseIndices = std::move(indices);
		}
		// BILINEAR SUBDIVISION
		else if (subdivisionType == 1)
		{
			uint32_t RC = 1 + (uint32_t)pow(2, (uint32_t)MAX_SUBDIVISION);

			std::vector<Vector2> barycentricCoordinates;
			barycentricCoordinates.reserve(RC * (RC + 1) / 2);

			double delta = 1.0 / (double)(RC - 1.0);

			uint32_t rowIdx = 0;
			uint32_t nextIdx = 0;
			for (uint32_t row = 0; row < RC; row++)
			{
				uint32_t numCols = RC - row;
				nextIdx += numCols;
				for (uint32_t column = 0; column < numCols; column++)
				{
					// calculate internal patch position
					barycentricCoordinates.emplace_back(column / (float)(RC - 1), row / (float)(RC - 1));
				}
				rowIdx = nextIdx;
			}

			for (int i = 0; i < indices.size() - 2; i += 3)
			{
				Vector3 A = vertices[indices[i]];
				Vector3 B = vertices[indices[i + 1]];
				Vector3 C = vertices[indices[i + 2]];

				Vector3 R = B - A;
				Vector3 S = C - A;

				for (auto& patchPos : barycentricCoordinates) 
				{
					Vector3 finalVec = A + R * patchPos.x + S * patchPos.y;
					uint32_t vecIndex;

					auto it = sBaseVertexMap.find(finalVec);
					if (it != sBaseVertexMap.end()) {
						vecIndex = it->second;
					}
					else {
						sBaseVertices.emplace_back(finalVec);
						uint32_t newIndex = sBaseVertices.size() - 1;
						sBaseVertexMap[finalVec] = newIndex;
						vecIndex = newIndex;
					}

					sBaseIndices.emplace_back(vecIndex);
				}
			}

			//for (auto& vec : barycentricCoordinates)
			//	vec.ToString("Barycentric Coordinate: ");
		}

		// Stop timing
		auto end = std::chrono::high_resolution_clock::now();

		// Calculate the duration
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

		TOAST_CORE_INFO("Base planet created with %d number of vertices and %d number indices, time: %dms", sBaseVertices.size(), sBaseIndices.size(), duration.count());
	}

	void PlanetSystem::GetBasePlanet(std::vector<Vector3>& vertices, std::vector<uint32_t>& indices, DirectX::XMFLOAT3& scale)
	{
		DirectX::XMMATRIX scaleMatrix = DirectX::XMMatrixIdentity() * DirectX::XMMatrixScaling(scale.x, scale.y, scale.z);
		Matrix scaleTransform = { scaleMatrix };

		double ratio = ((1.0 + sqrt(5.0)) / 2.0);

		vertices = std::vector<Vector3>{
			scaleTransform * Vector3::Normalize({ ratio, 0.0, -1.0 }),
			scaleTransform * Vector3::Normalize({ -ratio, 0.0, -1.0 }),
			scaleTransform * Vector3::Normalize({ ratio, 0.0, 1.0 }),
			scaleTransform * Vector3::Normalize({ -ratio, 0.0, 1.0 }),
			scaleTransform * Vector3::Normalize({ 0.0, -1.0, ratio }),
			scaleTransform * Vector3::Normalize({ 0.0, -1.0, -ratio }),
			scaleTransform * Vector3::Normalize({ 0.0, 1.0, ratio }),
			scaleTransform * Vector3::Normalize({ 0.0, 1.0, -ratio }),
			scaleTransform * Vector3::Normalize({ -1.0, ratio, 0.0 }),
			scaleTransform * Vector3::Normalize({ -1.0, -ratio, 0.0 }),
			scaleTransform * Vector3::Normalize({ 1.0, ratio, 0.0 }),
			scaleTransform * Vector3::Normalize({ 1.0, -ratio, 0.0 })
		};

		indices = std::vector<uint32_t>{
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
	}

	void PlanetSystem::SubdivideFace(Vector3& cameraPosPlanetSpace, Matrix& planetScaleTransform, Ref<Frustum>& frustum, int& triangleAdded, Matrix planetTransform, Vector3& a, Vector3& b, Vector3& c, int16_t& subdivision, bool backfaceCull, bool frustumCullActivated, bool frustumCull, PlanetComponent& planet)
	{
		TOAST_PROFILE_FUNCTION();

		Vector3 A, B, C;

		NextPlanetFace nextPlanetFace = CheckFaceSplit(cameraPosPlanetSpace, planetScaleTransform, frustum, subdivision, a, b, c, backfaceCull, frustumCullActivated, frustumCull, planet);

		if (nextPlanetFace == NextPlanetFace::CULL)
			return;

		if (nextPlanetFace == NextPlanetFace::SPLIT || nextPlanetFace == NextPlanetFace::SPLITCULL)
		{
			A = b + ((c - b) * 0.5);
			B = c + ((a - c) * 0.5);
			C = a + ((b - a) * 0.5);

			//A = Vector3::Normalize(A);
			//B = Vector3::Normalize(B);
			//C = Vector3::Normalize(C);

			int16_t nextSubdivision = subdivision + 1;

			SubdivideFace(cameraPosPlanetSpace, planetScaleTransform, frustum, triangleAdded, planetTransform, A, B, C, nextSubdivision, backfaceCull, frustumCullActivated, nextPlanetFace == NextPlanetFace::SPLITCULL, planet);
			SubdivideFace(cameraPosPlanetSpace, planetScaleTransform, frustum, triangleAdded, planetTransform, C, B, a, nextSubdivision, backfaceCull, frustumCullActivated, nextPlanetFace == NextPlanetFace::SPLITCULL, planet);
			SubdivideFace(cameraPosPlanetSpace, planetScaleTransform, frustum, triangleAdded, planetTransform, b, A, C, nextSubdivision, backfaceCull, frustumCullActivated, nextPlanetFace == NextPlanetFace::SPLITCULL, planet);
			SubdivideFace(cameraPosPlanetSpace, planetScaleTransform, frustum, triangleAdded, planetTransform, B, A, c, nextSubdivision, backfaceCull, frustumCullActivated, nextPlanetFace == NextPlanetFace::SPLITCULL, planet);
		}
		else
		{
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

				double cDistance = (testC - cameraPosPlanetSpace).Magnitude();
				double aDistance = (testA - cameraPosPlanetSpace).Magnitude();
				double bDistance = (testB - cameraPosPlanetSpace).Magnitude();

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
			if (!crackTriangle)
			{
				//if (triangleAdded == 0 || triangleAdded == 3 || triangleAdded == 8 || triangleAdded == 9) {
				//a.ToString();
				uint32_t currentIndexA = GetOrAddVertex(planet.VertexMap, a, planet.BuildVertices);
				planet.BuildIndices.emplace_back(currentIndexA);

				//b.ToString();
				uint32_t currentIndexB = GetOrAddVertex(planet.VertexMap, b, planet.BuildVertices);
				planet.BuildIndices.emplace_back(currentIndexB);

				//c.ToString();
				uint32_t currentIndexC = GetOrAddVertex(planet.VertexMap, c, planet.BuildVertices);
				planet.BuildIndices.emplace_back(currentIndexC);
			}
			else
			{
				// Calculate new vertex	
				Vertex newVertex = (closestVertex + middleVertex) * 0.5;

				// First triangle
				uint32_t currentIndexA = GetOrAddVertex(planet.VertexMap, newVertex, planet.BuildVertices);
				planet.BuildIndices.emplace_back(currentIndexA);

				//b.ToString();
				uint32_t currentIndexB = GetOrAddVertex(planet.VertexMap, closestVertex, planet.BuildVertices);
				planet.BuildIndices.emplace_back(currentIndexB);

				uint32_t currentIndexC = GetOrAddVertex(planet.VertexMap, furthestVertex, planet.BuildVertices);
				planet.BuildIndices.emplace_back(currentIndexC);

				// Second triangle
				uint32_t currentIndexD = GetOrAddVertex(planet.VertexMap, newVertex, planet.BuildVertices);
				planet.BuildIndices.emplace_back(currentIndexD);

				//b.ToString();
				uint32_t currentIndexE = GetOrAddVertex(planet.VertexMap, middleVertex, planet.BuildVertices);
				planet.BuildIndices.emplace_back(currentIndexE);

				uint32_t currentIndexF = GetOrAddVertex(planet.VertexMap, furthestVertex, planet.BuildVertices);
				planet.BuildIndices.emplace_back(currentIndexF);
			}

			triangleAdded++;
		}
	}

	void PlanetSystem::GeneratePlanet(Ref<Frustum>& frustum, DirectX::XMFLOAT3& scale, DirectX::XMMATRIX noScaleTransform, DirectX::XMVECTOR camPos, bool backfaceCull, bool frustumCullActivated,  PlanetComponent& planet, TerrainDetailComponent* terrainDetail)
	{
		TOAST_PROFILE_FUNCTION();

		planetGenerationOngoing.store(true);

		if (terrainDetail)
			const siv::PerlinNoise perlin{ (uint32_t)(terrainDetail->Seed) };

		int triangleAdded = 0;

		DirectX::XMMATRIX scaleMatrix = DirectX::XMMatrixIdentity() * DirectX::XMMatrixScaling(scale.x, scale.y, scale.z);
		Matrix scaleTransform = { scaleMatrix };

		Matrix planetTransform = { noScaleTransform };
		Vector3 cameraPos = { camPos };

		Vector3 cameraPosPlanetSpace = Matrix::Inverse(planetTransform) * cameraPos;
		//TOAST_CORE_INFO("cameraPosPlanetSpace.Magnitude(): %lf", cameraPosPlanetSpace.Magnitude());

		std::vector<Vector3> startVertices;
		std::vector<uint32_t> startIndices;
		GetBasePlanet(startVertices, startIndices, scale);
		
		{
			std::lock_guard<std::mutex> lock(planetDataMutex);

			planet.VertexMap.clear();

			planet.BuildVertices.clear();
			planet.BuildIndices.clear();
		}

		//for (int i = 0; i < startIndices.size() - 2; i += 3)
		//{
		//	int16_t firstSubdivision = 0;
		//	SubdivideFace(cameraPosPlanetSpace, scaleTransform, frustum, triangleAdded, planetTransform, startVertices.at(startIndices.at(i)), startVertices.at(startIndices.at(i + 1)), startVertices.at(startIndices.at(i + 2)), firstSubdivision, backfaceCull, frustumCullActivated, true, planet);
		//}

		for (int i = 0; i < sBaseIndices.size() - 2; i += 3)
		{
			int16_t firstSubdivision = 0;
			SubdivideFace(cameraPosPlanetSpace, scaleTransform, frustum, triangleAdded, planetTransform, sBaseVertices.at(sBaseIndices.at(i)), sBaseVertices.at(sBaseIndices.at(i + 1)), sBaseVertices.at(sBaseIndices.at(i + 2)), firstSubdivision, backfaceCull, frustumCullActivated, true, planet);
		}

		int i = 0;
		for (auto& vertex : planet.BuildVertices)
		{
			Vector3 normalizedPos = Vector3::Normalize({ vertex.Position.x, vertex.Position.y, vertex.Position.z });

			//TOAST_CORE_CRITICAL("vertex.Texcoord: %lf, %lf", vertex.Texcoord.x, vertex.Texcoord.y);
			Vector3 scaledPos;// = scaleTransform * normalizedPos;

			//Apply height to the planet
			if (!planet.TerrainData.HeightData.empty())
			{
				Vector2 uvCoord = GetUVFromPosition(normalizedPos, (double)planet.TerrainData.Width, (double)planet.TerrainData.Height);
				vertex.Texcoord = GetGPUUVFromPosition(normalizedPos);

				uint32_t x1 = (uint32_t)(uvCoord.x);
				uint32_t y1 = (uint32_t)(uvCoord.y);

				uint32_t x2 = x1 == (planet.TerrainData.Width - 1) ? 0 : x1 + 1;
				uint32_t y2 = y1 == (planet.TerrainData.Height - 1) ? 0 : y1 + 1;

				double Q11 = static_cast<double>(planet.TerrainData.HeightData[y1 * (planet.TerrainData.RowPitch / 2) + x1]);
				double Q12 = static_cast<double>(planet.TerrainData.HeightData[y2 * (planet.TerrainData.RowPitch / 2) + x1]);
				double Q21 = static_cast<double>(planet.TerrainData.HeightData[y1 * (planet.TerrainData.RowPitch / 2) + x2]);
				double Q22 = static_cast<double>(planet.TerrainData.HeightData[y2 * (planet.TerrainData.RowPitch / 2) + x2]);

				double terrainDataValue = Math::BilinearInterpolation(uvCoord, Q11, Q12, Q21, Q22);

				scaledPos = normalizedPos * (planet.PlanetData.radius + terrainDataValue);
			}

			scaledPos = planetTransform * scaledPos;

			vertex.Position = { (float)scaledPos.x, (float)scaledPos.y, (float)scaledPos.z };

			vertex.Color = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
			i++;
		}

		Vector3 cameraPosOnPlanetSurface = Vector3::Normalize(cameraPosPlanetSpace);
		Matrix inversePlanetTransform = Matrix::Inverse(planetTransform);

		newPlanetReady.store(true);
		planetGenerationOngoing.store(false);

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

	void PlanetSystem::Shutdown()
	{
		if (generationFuture.valid()) {
			generationFuture.wait();
		}
	}

	void PlanetSystem::GenerateDistanceLUT(std::vector<double>& distanceLUT, float radius, float FoV, float viewportSizeX)
	{
		distanceLUT.clear();

		double startLog = log(radius * 6.0);  // Starting log value for 1,000,000
		double endLog = log(radius * 0.00001475143826523086);  // Ending log value for 50

		for (int level = 0; level < MAX_SUBDIVISION; level++)
		{
			// Interpolate between startLog and endLog based on current level
			double t = static_cast<double>(level) / MAX_SUBDIVISION;
			double currentLog = (1 - t) * startLog + t * endLog;

			// Convert the logarithmic value back to linear space
			double currentDistance = exp(currentLog);

			distanceLUT.emplace_back(currentDistance);
		}

		//int i = 0;
		//for (auto level : distanceLUT) 
		//{
		//	i++;
		//	TOAST_CORE_INFO("distanceLUT[%d]: %lf", i, level);
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

	PlanetSystem::NextPlanetFace PlanetSystem::CheckFaceSplit(Vector3& cameraPosPlanetSpace, Matrix& planetScaleTransform, Ref<Frustum>& frustum, int16_t subdivision, Vector3 a, Vector3 b, Vector3 c, bool backfaceCull, bool frustumCullActivated, bool frustumCull, PlanetComponent& planet)
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
			uvCoord = GetUVFromPosition(a, planet.TerrainData.Width, planet.TerrainData.Height);
			aHeight = Vector3::Normalize(a) * (planet.PlanetData.radius + planet.TerrainData.HeightData[((uint32_t)uvCoord.y * (planet.TerrainData.RowPitch / 2) + (uint32_t)uvCoord.x)]);
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

		Vector3 center = ((aHeight + bHeight + cHeight) / 3.0);// *(1.0 - heightMultLUT[subdivision]);

		double dotProduct = Vector3::Dot(Vector3::Normalize(center), Vector3::Normalize(center - cameraPosPlanetSpace));

		{
			std::lock_guard<std::mutex> lock(planetDataMutex);
			if (backfaceCull && dotProduct >= (planet.FaceLevelDotLUT[(uint32_t)subdivision]))
				return NextPlanetFace::CULL;
		}

		double aDistance = (aHeight - cameraPosPlanetSpace).Magnitude();
		double bDistance = (bHeight - cameraPosPlanetSpace).Magnitude();
		double cDistance = (cHeight - cameraPosPlanetSpace).Magnitude();

		if (frustumCullActivated && frustumCull)
		{
			// Need to get ContainTriangle to work correctly, something is off here
			//auto intersect = frustum->ContainsTriangle(aHeight, bHeight, cHeight);
			auto intersect = frustum->ContainsTriangleVolume(aNoHeight, bNoHeight, cNoHeight, planet.HeightMultLUT[subdivision]);

			if (intersect == VolumeTri::OUTSIDE)
				return NextPlanetFace::CULL;

			if (intersect == VolumeTri::CONTAINS)//stop frustum culling -> all children are also inside the frustum
			{
				//check if new splits are allowed
				if (subdivision >= planet.Subdivisions)
					return NextPlanetFace::LEAF;

				if (aDistance < planet.DistanceLUT[(uint32_t)subdivision] && bDistance < planet.DistanceLUT[(uint32_t)subdivision] && cDistance < planet.DistanceLUT[(uint32_t)subdivision])
					return NextPlanetFace::SPLIT;

				return NextPlanetFace::LEAF;
			}
		}

		if (subdivision >= planet.Subdivisions)
			return NextPlanetFace::LEAF;

		if (aDistance < planet.DistanceLUT[(uint32_t)subdivision] && bDistance < planet.DistanceLUT[(uint32_t)subdivision] && cDistance < planet.DistanceLUT[(uint32_t)subdivision])
			return NextPlanetFace::SPLITCULL;

		return NextPlanetFace::LEAF;
	}

	uint32_t PlanetSystem::GetOrAddVertex(std::unordered_map<Vertex, uint32_t, Vertex::Hasher, Vertex::Equal>& vertexMap, const Vertex& vertex, std::vector<Vertex>& vertices) {
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