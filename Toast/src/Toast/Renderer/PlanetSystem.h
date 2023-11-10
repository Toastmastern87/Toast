#pragma once

#include "Toast/Core/Timestep.h"
#include "Toast/Core/Math/Math.h"

#include "Toast/Renderer/Frustum.h"
#include "Toast/Renderer/Mesh.h"
#include "Toast/Renderer/RenderCommand.h"

#include "renderdoc_app.h"

#define M_PI 3.14159265358979323846f

using namespace DirectX;

namespace Toast {

	class PlanetSystem
	{
	public:
		const int16_t BASE_SUBDIVISION = 4;

		using Edge = std::pair<uint32_t, uint32_t>;

		struct EdgeHash {
			std::size_t operator()(const Edge& edge) const {
				// Hash combining based on Boost's hash_combine
				std::size_t hash = std::hash<uint32_t>()(edge.first);
				hash ^= std::hash<uint32_t>()(edge.second) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
				return hash;
			}
		};

		enum class NextPlanetFace
		{
			CULL, LEAF, SPLIT, SPLITCULL
		};
	public:
		struct VertexHasher {
			size_t operator()(const Vertex& v) const {
				return	static_cast<size_t>(v.Position.x * 73856093) ^
						static_cast<size_t>(v.Position.y * 19349663) ^
						static_cast<size_t>(v.Position.z * 83492791);
			}
		};

		struct VertexEquality {
			bool operator()(const Vertex& v1, const Vertex& v2) const {
				float threshold = 0.0001f;  // adjust based on your needs
				return	abs(v1.Position.x - v2.Position.x) < threshold &&
						abs(v1.Position.y - v2.Position.y) < threshold &&
						abs(v1.Position.z - v2.Position.z) < threshold;
			}
		};

		static void GetBasePlanet(std::vector<Vector3>& vertices, std::vector<uint32_t>& indices)
		{
			double ratio = ((1.0 + sqrt(5.0)) / 2.0);

			vertices = std::vector<Vector3>{
				Vector3::Normalize({ ratio, 0.0, -1.0 }),
				Vector3::Normalize({ -ratio, 0.0, -1.0 }),
				Vector3::Normalize({ ratio, 0.0, 1.0 }),
				Vector3::Normalize({ -ratio, 0.0, 1.0 }),
				Vector3::Normalize({ 0.0, -1.0, ratio }),
				Vector3::Normalize({ 0.0, -1.0, -ratio }),
				Vector3::Normalize({ 0.0, 1.0, ratio }),
				Vector3::Normalize({ 0.0, 1.0, -ratio }),
				Vector3::Normalize({ -1.0, ratio, 0.0 }),
				Vector3::Normalize({ -1.0, -ratio, 0.0 }),
				Vector3::Normalize({ 1.0, ratio, 0.0 }),
				Vector3::Normalize({ 1.0, -ratio, 0.0 })
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

		//	// Return the highest subdivision level detected
		//	return subDivisionX > subDivisionY ? subDivisionX : subDivisionY;
		//}

		//static NextPlanetFace CheckPlanetFaceSplit(Frustum* frustum, Matrix planetTransform, Vector3 a, Vector3 b, Vector3 c, int16_t subdivision, int16_t maxSubdivisions, std::vector<float>& distanceLUT, std::vector<float>& faceLevelDotLUT, std::vector<float>& heightMultLUT, Vector3& cameraPos, DirectX::XMVECTOR& cameraForward, bool backfaceCull, bool frustumCullActivated, bool frustumCull)
		//{
		//	//DirectX::XMVECTOR planetTranslation, planetRotation, planetScale;

		//	//DirectX::XMMatrixDecompose(&planetScale, &planetRotation, &planetTranslation, planetTransform);

		//	double aDistance, bDistance, cDistance;

		//	Vector3 center = (a + b + c) / 3.0;

		//	double dotProduct = Vector3::Dot(Vector3::Normalize(center), Vector3::Normalize(center - cameraPos));//DirectX::XMVector3Dot(center, center - cameraPos);

		//	if (backfaceCull && dotProduct >= (faceLevelDotLUT[(uint32_t)subdivision] + 0.1))
		//		return NextPlanetFace::CULL;

		//	if (frustumCullActivated && frustumCull)
		//	{
		//		auto intersect = frustum->ContainsTriangleVolume(a, b, c, heightMultLUT[(uint32_t)subdivision]);

		//		if (intersect == VolumeTri::OUTSIDE) 
		//			return NextPlanetFace::CULL;

		//		if (intersect == VolumeTri::CONTAINS)//stop frustum culling -> all children are also inside the frustum
		//		{
		//			//check if new splits are allowed
		//			if (subdivision >= maxSubdivisions)
		//				return NextPlanetFace::LEAF;		

		//			//split according to distance
		//			aDistance = (a - cameraPos).Magnitude();
		//			bDistance = (b - cameraPos).Magnitude();
		//			cDistance = (c - cameraPos).Magnitude();

		//			if ((std::min)(aDistance, (std::min)(bDistance, cDistance)) < (double)distanceLUT[(uint32_t)subdivision])
		//				return NextPlanetFace::SPLIT;

		//			return NextPlanetFace::LEAF;
		//		}
		//	}

		//	if (subdivision >= maxSubdivisions) 
		//		return NextPlanetFace::LEAF;

		//	//TOAST_CORE_INFO("a vector: %f, %f, %f", DirectX::XMVectorGetX(a), DirectX::XMVectorGetY(a), DirectX::XMVectorGetZ(a));
		//	//TOAST_CORE_INFO("b vector: %f, %f, %f", DirectX::XMVectorGetX(b), DirectX::XMVectorGetY(b), DirectX::XMVectorGetZ(b));
		//	//TOAST_CORE_INFO("c vector: %f, %f, %f", DirectX::XMVectorGetX(c), DirectX::XMVectorGetY(c), DirectX::XMVectorGetZ(c));
		//	//TOAST_CORE_CRITICAL("cameraPos: %f, %f, %f", DirectX::XMVectorGetX(cameraPos), DirectX::XMVectorGetY(cameraPos), DirectX::XMVectorGetZ(cameraPos));
		//	aDistance = (a - cameraPos).Magnitude();
		//	bDistance = (b - cameraPos).Magnitude();
		//	cDistance = (c - cameraPos).Magnitude();

		//	//TOAST_CORE_INFO("aDistance: %f", aDistance);
		//	//TOAST_CORE_INFO("bDistance: %f", bDistance);
		//	//TOAST_CORE_INFO("cDistance: %f", cDistance);

		//	if ((std::min)(aDistance, (std::min)(bDistance, cDistance)) < (double)distanceLUT[(uint32_t)subdivision]) 
		//		return NextPlanetFace::SPLITCULL;

		//	return NextPlanetFace::LEAF;
		//}

		//static void RecursiveFace(int& numberOfPatches, Frustum* frustum, Matrix planetTransform, Vector3& a, Vector3& b, Vector3& c, int16_t subdivision, int16_t maxSubdivisions, std::vector<PlanetPatch>& patches, std::vector<float>& distanceLUT, std::vector<float>& faceLevelDotLUT, std::vector<float>& heightMultLUT, Vector3& cameraPos, DirectX::XMVECTOR& cameraForward, float radius, bool backfaceCull, bool frustumCullActivated, bool frustumCull)
		//{
		//	Vector3 A, B, C;

		//	NextPlanetFace nextPlanetFace = CheckPlanetFaceSplit(frustum, planetTransform, a, b, c, subdivision, maxSubdivisions, distanceLUT, faceLevelDotLUT, heightMultLUT, cameraPos, cameraForward, backfaceCull, frustumCullActivated, frustumCull);
		//	
		//	if (nextPlanetFace == NextPlanetFace::CULL)
		//		return;

		//	if (subdivision < maxSubdivisions && (nextPlanetFace == NextPlanetFace::SPLIT || nextPlanetFace == NextPlanetFace::SPLITCULL)) {
		//		A = b + ((c - b) * 0.5);
		//		B = c + ((a - c) * 0.5);
		//		C = a + ((b - a) * 0.5);

		//		A = Vector3::Normalize(A);
		//		B = Vector3::Normalize(B);
		//		C = Vector3::Normalize(C);

		//		int16_t nextSubdivision = subdivision + 1;

		//		RecursiveFace(numberOfPatches, frustum, planetTransform, C, B, a, nextSubdivision, maxSubdivisions, patches, distanceLUT, faceLevelDotLUT, heightMultLUT, cameraPos, cameraForward, radius, backfaceCull, frustumCullActivated, nextPlanetFace == NextPlanetFace::SPLITCULL);
		//		RecursiveFace(numberOfPatches, frustum, planetTransform, b, A, C, nextSubdivision, maxSubdivisions, patches, distanceLUT, faceLevelDotLUT, heightMultLUT, cameraPos, cameraForward, radius, backfaceCull, frustumCullActivated, nextPlanetFace == NextPlanetFace::SPLITCULL);
		//		RecursiveFace(numberOfPatches, frustum, planetTransform, B, A, c, nextSubdivision, maxSubdivisions, patches, distanceLUT, faceLevelDotLUT, heightMultLUT, cameraPos, cameraForward, radius, backfaceCull, frustumCullActivated, nextPlanetFace == NextPlanetFace::SPLITCULL);
		//		RecursiveFace(numberOfPatches, frustum, planetTransform, A, B, C, nextSubdivision, maxSubdivisions, patches, distanceLUT, faceLevelDotLUT, heightMultLUT, cameraPos, cameraForward, radius, backfaceCull, frustumCullActivated, nextPlanetFace == NextPlanetFace::SPLITCULL);
		//	}
		//	else
		//	{
		//		//DirectX::XMFLOAT3 testP1 = { (float)a.x, (float)a.y, (float)a.z };
		//		//DirectX::XMVECTOR testP1V = DirectX::XMLoadFloat3(&testP1);
		//		//testP1V = DirectX::XMVector3Transform(testP1V, transform);

		//		//TOAST_CORE_INFO("DIRECTXMATH p1 after transform: %f, %f, %f, %f", DirectX::XMVectorGetX(testP1V), DirectX::XMVectorGetY(testP1V), DirectX::XMVectorGetZ(testP1V), DirectX::XMVectorGetW(testP1V));

		//		//TOAST_CORE_INFO("Adding patch: %d", patches.size());
		//		//TOAST_CORE_INFO("patchesAdded: %d", patchesAdded);

		//		//planetTransform.ToString();
		//		numberOfPatches++;
		//		//TOAST_CORE_CRITICAL("Number of patches added! %d", numberOfPatches);
		//		//planetTransform = planetTransform.Transpose();
		//		//if (numberOfPatches == 17) {
		//			Vector3 aTransformed = planetTransform * a;
		//			Vector3 bTransformed = planetTransform * b;
		//			Vector3 cTransformed = planetTransform * c;

		//			//TOAST_CORE_INFO("P1 Before Transform: %lf, %lf, %lf", a.x, a.y, a.z);
		//			Vector3 p1 = aTransformed;
		//			//TOAST_CORE_INFO("P1 After Transform: %lf, %lf, %lf", p1.x, p1.y, p1.z);
		//			//TOAST_CORE_INFO("p1 after transform: %lf, %lf, %lf, %lf", p1.x, p1.y, p1.z, p1.w);
		//			DirectX::XMFLOAT3 firstCorner = { (float)p1.x, (float)p1.y, (float)p1.z };

		//			Vector3 p2 = (bTransformed - aTransformed);
		//			//TOAST_CORE_INFO("P2 Before Transform: %lf, %lf, %lf", p2.x, p2.y, p2.z);
		//			//p2 = planetTransform * p2;
		//			//TOAST_CORE_INFO("P2 After Transform: %lf, %lf, %lf", p2.x, p2.y, p2.z);
		//			DirectX::XMFLOAT3 secondCorner = { (float)p2.x, (float)p2.y, (float)p2.z };

		//			Vector3 p3 = (cTransformed - aTransformed);
		//			//TOAST_CORE_INFO("P3 Before Transform: %lf, %lf, %lf", p3.x, p3.y, p3.z);
		//			//p3 = planetTransform * p3;
		//			//TOAST_CORE_INFO("P3 After Transform: %lf, %lf, %lf", p3.x, p3.y, p3.z);
		//			DirectX::XMFLOAT3 thirdCorner = { (float)p3.x, (float)p3.y, (float)p3.z };

		//			patches.emplace_back(PlanetPatch(subdivision, firstCorner, thirdCorner, secondCorner));
		//		//}
		//	}
		//}

		static void GeneratePlanet(std::vector<PlanetSystem::Edge>& planetEdges, std::unordered_map<Vertex, uint32_t, VertexHasher, VertexEquality>& vertexMap, Frustum* frustum, DirectX::XMMATRIX transform, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, std::vector<double>& distanceLUT, std::vector<float>& faceLevelDotLUT, std::vector<float>& heightMultLUT, std::vector<double>& subdivisionLUT, DirectX::XMVECTOR camPos, int16_t subdivisions, float radius, bool backfaceCull, bool frustumCullActivated)
		{
			int triangleAdded = 0;

			Matrix planetTransform = { transform };
			Vector3 cameraPos = { camPos };

			Vector3 cameraPosPlanetSpace = Matrix::Inverse(planetTransform) * cameraPos;
			
			std::vector<Vector3> startVertices;
			std::vector<uint32_t> startIndices;
			GetBasePlanet(startVertices, startIndices);

			vertexMap.clear();
			planetEdges.clear();

			vertices.clear();
			indices.clear();

			for (int i = 0; i < startIndices.size() - 2; i += 3)
			{
				int16_t firstSubdivision = 0;
				SubdivideFace(planetEdges, triangleAdded, vertexMap, frustum, planetTransform, vertices, indices, startVertices.at(startIndices.at(i)), startVertices.at(startIndices.at(i+1)), startVertices.at(startIndices.at(i + 2)), firstSubdivision, subdivisions, cameraPosPlanetSpace, distanceLUT, faceLevelDotLUT, backfaceCull);
			}

			//TOAST_CORE_CRITICAL("planetEdges.size(): %d", planetEdges.size());

			FindLODGapsAndCover(planetEdges, indices, vertices, subdivisionLUT, subdivisions);

			for (auto& vertex : vertices)
			{
				Vector3 vertexDouble = Vector3::Normalize({ vertex.Position.x, vertex.Position.y, vertex.Position.z });
				vertexDouble = planetTransform * vertexDouble;
				vertex.Position = { (float)vertexDouble.x, (float)vertexDouble.y, (float)vertexDouble.z };
			}
		}

		static bool IsPointOnLineSegment(const Vector3& point, const Vector3& lineStart, const Vector3& lineEnd, double tolerance = 0.00001) {
			Vector3 lineVector = lineEnd - lineStart;
			Vector3 pointVector = point - lineStart;

			// Calculate the projection of pointVector onto lineVector using dot product
			double dotProduct = Vector3::Dot(pointVector, lineVector);
			double lineLengthSquared = lineVector.Magnitude() * lineVector.Magnitude();  // Use MagnitudeSquared to avoid a square root for efficiency

			// Check if the projection is between 0 and the length of the line vector
			if (dotProduct < 0.0 || dotProduct > lineLengthSquared) 
				return false;

			// Calculate the closest point on the line to 'point'
			Vector3 closestPoint = lineStart + (lineVector * (dotProduct / lineLengthSquared));

			// Check if the distance between 'point' and the closest point on the line is within tolerance
			Vector3 distanceVector = point - closestPoint;
			return distanceVector.Magnitude() <= tolerance;
		}

		// Function to check if two edges are coincidental
		static int32_t AreEdgesCoincidental(const Edge& edge1, const Edge& edge2, std::vector<uint32_t>& indices, const std::vector<Vertex>& vertices, std::vector<double>& subdivisionLUT, float maxSubdivision) {
			// Now check if the vertices of the edges are the same or coincidental
			const Vector3& v1A = vertices[edge1.first].Position;  // Assuming Vertex has a Vector3 called position
			const Vector3& v1B = vertices[edge1.second].Position;
			const Vector3& v2A = vertices[edge2.first].Position;
			const Vector3& v2B = vertices[edge2.second].Position;

			if (v1A == v2A || v1A == v2B || v1B == v2A || v1B == v2B) 
			{
				double lenEdge1 = (v1B - v1A).Magnitude();
				double lenEdge2 = (v2B - v2A).Magnitude();

				uint32_t uniqueVertexIndex;
				Edge shortEdge, longEdge;

				if (fabs(lenEdge1 - lenEdge2) > 0.00001) {
					if (lenEdge1 < lenEdge2)
					{
						uniqueVertexIndex = (v1A == v2A || v1A == v2B) ? edge1.second : edge1.first;
						shortEdge = edge1;
						longEdge = edge2;
					}
					else 
					{
						uniqueVertexIndex = (v2A == v1A || v2A == v1B) ? edge2.second : edge2.first;
						shortEdge = edge2;
						longEdge = edge1;
					}

					// Check if the unique vertex from the shorter edge is on the line segment defined by the longer edge
					const Vector3& uniqueVertex = vertices[uniqueVertexIndex].Position;
					const Vector3& longEdgeStart = (lenEdge1 < lenEdge2) ? v2A : v1A;
					const Vector3& longEdgeEnd = (lenEdge1 < lenEdge2) ? v2B : v1B;

					if (IsPointOnLineSegment(uniqueVertex, longEdgeStart, longEdgeEnd)) 
					{
						indices.emplace_back(uniqueVertexIndex);
						indices.emplace_back(longEdge.first);
						indices.emplace_back(longEdge.second);

						return uniqueVertexIndex;
					}
				}
			}	

			return -1;
		}

		static void FindLODGapsAndCover(const std::vector<Edge>& planetEdges, std::vector<uint32_t>& indices, const std::vector<Vertex>& vertices, std::vector<double>& subdivisionLUT, float maxSubdivision) {
			std::unordered_set<uint32_t> coincidentalEdges;

			for (size_t i = 0; i < planetEdges.size(); i++) {
				for (size_t j = i + 1; j < planetEdges.size(); j++) {
					int32_t vertexIndex = AreEdgesCoincidental(planetEdges[i], planetEdges[j], indices, vertices, subdivisionLUT, maxSubdivision);
					if (vertexIndex >= 0 && !coincidentalEdges.count(vertexIndex)) {
						coincidentalEdges.emplace(vertexIndex);
						break;  // Once we find a coincidental edge, we don't need to check this edge against others
					}
				}
			}

			return;
		}

		static void GenerateSubdivisionLUT(std::vector<double>& subdivisionLUT, float maxSubdivisions)
		{
			subdivisionLUT.clear();

			double ratio = ((1.0 + sqrt(5.0)) / 2.0);

			std::vector<Vector3> startValues = std::vector<Vector3>{
				Vector3::Normalize({ ratio, 0.0, -1.0 }),
				Vector3::Normalize({ ratio, 0.0, 1.0 }),
			};

			double magnitude = (startValues.at(0) - startValues.at(1)).Magnitude();
			subdivisionLUT.emplace_back(magnitude);

			for (size_t i = 1; i <= maxSubdivisions; ++i) {
				magnitude *= 0.5;
				subdivisionLUT.emplace_back(magnitude);
			}

			//for (auto level : subdivisionLUT)
			//	TOAST_CORE_INFO("subdivisionLUT: %lf", level);
		}

		static void GenerateDistanceLUT(std::vector<double>& distanceLUT, float maxSubdivisions, float radius, float FoV, float viewportSizeX)
		{
			distanceLUT.clear();

			double startLog = log(20000000.0 / 33790000.0);  // Starting log value for 1,000,000
			double endLog = log(50.0 / 33790000.0);  // Ending log value for 50

			for (int level = 0; level < maxSubdivisions; level++)
			{
				// Interpolate between startLog and endLog based on current level
				double t = static_cast<double>(level) / (maxSubdivisions);
				double currentLog = (1 - t) * startLog + t * endLog;

				// Convert the logarithmic value back to linear space
				double currentDistance = exp(currentLog);

				distanceLUT.emplace_back(currentDistance);
			}

			//for (auto level : distanceLUT)
			//	TOAST_CORE_INFO("distanceLUT: %lf", level);

			//for (auto level : distanceLUT)
			//	TOAST_CORE_INFO("distanceLUT: %lf", level * 33790000.0f);
		}

		static void GenerateFaceDotLevelLUT(std::vector<float>& faceLevelDotLUT, float scale, float maxSubdivisions, float maxHeight)
		{
			float cullingAngle = acosf(scale / (scale + maxHeight));

			faceLevelDotLUT.clear();
			faceLevelDotLUT.emplace_back(0.5f + sinf(cullingAngle));
			float angle = acosf(0.5f);
			for (int i = 1; i <= maxSubdivisions; i++)
			{
				angle *= 0.5f;
				faceLevelDotLUT.emplace_back(sinf(angle + cullingAngle));
			}

			for (auto level : faceLevelDotLUT)
				TOAST_CORE_INFO("FacelevelDotLUT: %f", level);
		}

		static void GenerateHeightMultLUT(std::vector<float>& heightMultLUT, float scale, float maxSubdivisions, float maxHeight)
		{
		//	heightMultLUT.clear();
		//	Vector3 a = faces[0].A;
		//	Vector3 b = faces[0].B;
		//	Vector3 c = faces[0].C;

		//	Vector3 center = (a + b + c) / 3.0;
		//	center *= scale / center.Magnitude();//+maxHeight 
		//	heightMultLUT.push_back(1.0 / Vector3::Dot(Vector3::Normalize(a), Vector3::Normalize(center)));
		//	float normMaxHeight = maxHeight / scale;
		//	for (int i = 1; i <= maxSubdivisions; i++)
		//	{
		//		Vector3 A = b + ((c - b) * 0.5);
		//		Vector3 B = c + ((a - c) * 0.5);
		//		c = a + ((b - a) * 0.5);
		//		a = A * scale / A.Magnitude();
		//		b = B * scale / B.Magnitude();
		//		c *= scale / c.Magnitude();
		//		heightMultLUT.push_back((float)(1.0 / Vector3::Dot(Vector3::Normalize(a), Vector3::Normalize(center)) + normMaxHeight));
		//	}

		//	//for (auto level : heightMultLUT)
		//	//	TOAST_CORE_INFO("heightMultLUT: %f", level);
		}

		static void SubdivideFace(std::vector<PlanetSystem::Edge>& planetEdges, int& triangleAdded, std::unordered_map<Vertex, uint32_t, VertexHasher, VertexEquality>& vertexMap, Frustum* frustum, Matrix planetTransform, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, Vector3& a, Vector3& b, Vector3& c, int16_t& subdivision, int16_t maxSubdivisions, Vector3& cameraPosUnitSpace, std::vector<double>& distanceLUT, std::vector<float>& faceLevelDotLUT, bool backfaceCull)
		{
			Vector3 A, B, C;

			NextPlanetFace nextPlanetFace = CheckFaceSplit(subdivision, maxSubdivisions, a, b, c, cameraPosUnitSpace, distanceLUT, faceLevelDotLUT, backfaceCull);

			if (nextPlanetFace == NextPlanetFace::CULL) 
				return;

			if (nextPlanetFace == NextPlanetFace::SPLIT || nextPlanetFace == NextPlanetFace::SPLITCULL) {
				A = b + ((c - b) * 0.5);
				B = c + ((a - c) * 0.5);
				C = a + ((b - a) * 0.5);

				//A = Vector3::Normalize(A);
				//B = Vector3::Normalize(B);
				//C = Vector3::Normalize(C);

				int16_t nextSubdivision = subdivision + 1;
				
				SubdivideFace(planetEdges, triangleAdded, vertexMap, frustum, planetTransform, vertices, indices, A, B, C, nextSubdivision, maxSubdivisions, cameraPosUnitSpace, distanceLUT, faceLevelDotLUT, backfaceCull);
				SubdivideFace(planetEdges, triangleAdded, vertexMap, frustum, planetTransform, vertices, indices, C, B, a, nextSubdivision, maxSubdivisions, cameraPosUnitSpace, distanceLUT, faceLevelDotLUT, backfaceCull);
				SubdivideFace(planetEdges, triangleAdded, vertexMap, frustum, planetTransform, vertices, indices, b, A, C, nextSubdivision, maxSubdivisions, cameraPosUnitSpace, distanceLUT, faceLevelDotLUT, backfaceCull);
				SubdivideFace(planetEdges, triangleAdded, vertexMap, frustum, planetTransform, vertices, indices, B, A, c, nextSubdivision, maxSubdivisions, cameraPosUnitSpace, distanceLUT, faceLevelDotLUT, backfaceCull);
			}
			else
			{
				//if (triangleAdded == 12) {
				uint32_t currentIndexA = GetOrAddVertex(vertexMap, a, vertices);
				indices.emplace_back(currentIndexA);

				uint32_t currentIndexB = GetOrAddVertex(vertexMap, b, vertices);
				indices.emplace_back(currentIndexB);

				uint32_t currentIndexC = GetOrAddVertex(vertexMap, c, vertices);
				indices.emplace_back(currentIndexC);

				planetEdges.emplace_back(currentIndexA, currentIndexB);
				planetEdges.emplace_back(currentIndexA, currentIndexC);
				planetEdges.emplace_back(currentIndexC, currentIndexB);
				//}

				triangleAdded++;
			}
		}

		static NextPlanetFace CheckFaceSplit(int16_t subdivision, int16_t maxSubdivisions, Vector3 a, Vector3 b, Vector3 c, Vector3& cameraPos, std::vector<double>& distanceLUT, std::vector<float>& faceLevelDotLUT, bool backfaceCull)
		{
			Vector3 center = (a + b + c) / 3.0;

			double dotProduct = Vector3::Dot(Vector3::Normalize(center), Vector3::Normalize(center - cameraPos));

			if (backfaceCull && dotProduct >= (faceLevelDotLUT[(uint32_t)subdivision] + 0.1))
				return NextPlanetFace::CULL;

			if (subdivision >= maxSubdivisions)
				return NextPlanetFace::LEAF;

			double aDistance = (a - cameraPos).Magnitude();
			double bDistance = (b - cameraPos).Magnitude();
			double cDistance = (c - cameraPos).Magnitude();

			//TOAST_CORE_INFO("aDistance: %f", aDistance);
			//TOAST_CORE_INFO("bDistance: %f", bDistance);
			//TOAST_CORE_INFO("cDistance: %f", cDistance);

			if ((std::min)(aDistance, (std::min)(bDistance, cDistance)) < distanceLUT[(uint32_t)subdivision])
				return NextPlanetFace::SPLITCULL;

			return NextPlanetFace::LEAF;
		}

		static uint32_t GetOrAddVertex(std::unordered_map<Vertex, uint32_t, VertexHasher, VertexEquality>& vertexMap, const Vertex& vertex, std::vector<Vertex>& vertices) {
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
	};
}