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
		enum class NextPlanetFace
		{
			CULL, LEAF, SPLIT, SPLITCULL
		};
	public:
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

		//static int32_t GetSubdivisionLevel(DirectX::XMFLOAT4& pos, int16_t nrOfSubDivisions)
		//{
		//	int32_t subDivisionX = -1, subDivisionY = -1;

		//	// X evaluation
		//	if (pos.x == 0.0f || pos.x == 1.0f)
		//		subDivisionX = 0;
		//	else
		//	{
		//		for (int i = nrOfSubDivisions; i > 0; i--)
		//		{
		//			bool valueFound = false;
		//			float startValue = 1.0f * std::powf(0.5f, i);
		//			float delta = startValue * 2.0f;

		//			// Step through the line to see if the x value is at this subdivision level
		//			float p = startValue;
		//			while (p < 1.0f)
		//			{
		//				//TOAST_CORE_INFO("Finding X testing: %f", p);
		//				// Value found
		//				if (pos.x == p)
		//				{
		//					valueFound = true;
		//					break;
		//				}

		//				p += delta;
		//			}

		//			if (valueFound) 
		//			{
		//				subDivisionX = i;
		//				break;
		//			}
		//		}
		//	}

		//	// Y evaluation
		//	if (pos.y == 0.0f || pos.y == 1.0f)
		//		subDivisionY = 0;
		//	else
		//	{
		//		for (int i = nrOfSubDivisions; i > 0; i--)
		//		{
		//			bool valueFound = false;
		//			float startValue = 1.0f * std::powf(0.5f, i);
		//			float delta = startValue * 2.0f;

		//			// Step through the line to see if the y value is at this subdivision level
		//			float p = startValue;
		//			while (p < 1.0f)
		//			{
		//				//TOAST_CORE_INFO("Finding Y testing: %f", p);
		//				// Value found
		//				if (pos.y == p)
		//				{
		//					valueFound = true;
		//					break;
		//				}

		//				p += delta;
		//			}

		//			if (valueFound)
		//			{
		//				subDivisionY = i;
		//				break;
		//			}
		//		}
		//	}

		//	// Return the highest subdivision level detected
		//	return subDivisionX > subDivisionY ? subDivisionX : subDivisionY;
		//}

		//static void GeneratePatchGeometry(std::vector<PlanetVertex>& vertices, std::vector<uint32_t>& indices, int16_t patchLevel)
		//{
		//	vertices.clear();
		//	indices.clear();

		//	double ratio = ((1.0 + sqrt(5.0)) / 2.0);

		//	//TOAST_CORE_CRITICAL("Ratio: %f, scale: %f", ratio, scale);

		//	std::vector<Vector3> startFaceVertices = std::vector<Vector3>{
		//																 Vector3::Normalize({ ratio, 0.0, -1.0 }),
		//																 Vector3::Normalize({ ratio, 0.0, 1.0 }),
		//	};

		//	Vector3 midPt = (startFaceVertices[0] + startFaceVertices[1]) / 2.0;
		//	double midPtLength = midPt.Magnitude();
		//	// 1.0 is the radius
		//	double scaleFactor = 1.0 / midPtLength;
		//	midPt = Vector3::Normalize(midPt);
		//	//TOAST_CORE_CRITICAL("startFaceVertices[0].Magnitude: %lf", startFaceVertices[0].Magnitude());
		//	//TOAST_CORE_CRITICAL("Subdivision 1: scaleFactorF: %lf", scaleFactor);

		//	Vector3 secondMidPt = (startFaceVertices[0] + midPt) / 2.0;
		//	double secondMidPtLength = secondMidPt.Magnitude();
		//	double secondScaleFactor = 1.0 / secondMidPtLength;
		//	secondMidPt = Vector3::Normalize(secondMidPt);
		//	//TOAST_CORE_CRITICAL("from first scaleFactorF: %lf", (((scaleFactor - 1.0) * 0.5) + 1.0));
		//	//TOAST_CORE_CRITICAL("Subdivision 2: scaleFactorF: %lf", secondScaleFactor * (((scaleFactor - 1.0) * 0.5) + 1.0));


		//	Vector3 thirdMidPt = (startFaceVertices[0] + secondMidPt) / 2.0;
		//	double thirdMidPtLength = secondMidPt.Magnitude();
		//	double thirdScaleFactor = 1.0 / thirdMidPtLength;
		//	thirdMidPt = Vector3::Normalize(thirdMidPt);
		//	//TOAST_CORE_CRITICAL("from first scaleFactorF: %lf", (((scaleFactor - 1.0) * 0.25) + 1.0));
		//	//TOAST_CORE_CRITICAL("from second scaleFactorF: %lf", (((secondScaleFactor - 1.0) * 0.5) + 1.0));
		//	//TOAST_CORE_CRITICAL("Subdivision 3: scaleFactorF: %lf", thirdScaleFactor * (((secondScaleFactor - 1.0) * 0.5) + 1.0) * (((scaleFactor - 1.0) * 0.25) + 1.0));
		//	//DirectX::XMVECTOR thirdMidPt = (startFaceVertices[0] + secondMidPt) / 2.0f;
		//	//double thirdMidPtLength = (double)DirectX::XMVectorGetX(DirectX::XMVector3Length(thirdMidPt));
		//	//double thirdScaleFactor = (double)radius / thirdMidPtLength;
		//	//thirdMidPt = DirectX::XMVector3Normalize(thirdMidPt);
		//	////TOAST_CORE_CRITICAL("Subdivision 3: scaleFactorF: %lf", thirdScaleFactor);

		//	//DirectX::XMVECTOR fourthMidPt = (startFaceVertices[0] + thirdMidPt) / 2.0f;
		//	//double fourthMidPtLength = (double)DirectX::XMVectorGetX(DirectX::XMVector3Length(fourthMidPt));
		//	//double fourthScaleFactor = (double)radius / fourthMidPtLength;
		//	////TOAST_CORE_CRITICAL("Subdivision 4: scaleFactorF: %lf", fourthScaleFactor);

		//	uint32_t mRC = 1 + (uint32_t)pow(2, patchLevel);

		//	float delta = 1.0f / ((float)mRC - 1.0f);

		//	//TOAST_CORE_CRITICAL("Patch levels: %d", patchLevel);
		//	//TOAST_CORE_CRITICAL("delta: %f", delta);

		//	uint32_t rowIndex = 0;
		//	uint32_t nextIndex = 0;

		//	for (uint32_t row = 0; row < mRC; row++)
		//	{
		//		uint32_t numCols = mRC - row;

		//		nextIndex += numCols;

		//		for (uint32_t column = 0; column < numCols; column++)
		//		{
		//			//TOAST_CORE_CRITICAL("Column: %d", column);
		//			int32_t currentPatchLevel = column / patchLevel;
		//			//TOAST_CORE_CRITICAL("Position patch level: %d", currentPatchLevel);

		//			// calculate position
		//			DirectX::XMFLOAT4 pos = { column / ((float)mRC - 1.0f), row / ((float)mRC - 1.0f), 0.0f, 0.0f };
		//			pos.z = (float)GetSubdivisionLevel(pos, patchLevel);
		//			//TOAST_CORE_INFO("Patch position: %f, %f", pos.x, pos.y);
		//			//TOAST_CORE_INFO("Position Subdivision: %f", (float)GetSubdivisionLevel(pos, patchLevel));
		//			//create vertex
		//			vertices.emplace_back(PlanetVertex(pos));

		//			//calculate index
		//			if (row < mRC - 1 && column < numCols - 1)
		//			{
		//				//TOAST_CORE_CRITICAL("Row: %d", row);

		//				indices.emplace_back(rowIndex + column);
		//				indices.emplace_back(nextIndex + column);
		//				indices.emplace_back(1 + rowIndex + column);

		//				if (column < numCols - 2)
		//				{
		//					indices.emplace_back(nextIndex + column);
		//					indices.emplace_back(1 + nextIndex + column);
		//					indices.emplace_back(1 + rowIndex + column);
		//				}
		//			}
		//		}

		//		rowIndex = nextIndex;
		//	}
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

		static void GeneratePlanet(Frustum* frustum, DirectX::XMMATRIX transform, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, std::vector<float>& distanceLUT, std::vector<float>& faceLevelDotLUT, std::vector<float>& heightMultLUT, DirectX::XMVECTOR camPos, DirectX::XMVECTOR& cameraForward, int16_t subdivisions, float radius, bool backfaceCull, bool frustumCullActivated)
		{
			Matrix planetTransform = { transform };
			Vector3 cameraPos = { camPos };
			//TOAST_CORE_INFO("Recreating planet!");
			//planetTransform.ToString();

			int numberOfPatches = 0;

			//DirectX::XMVECTOR row = transform.r[0];
			//DirectX::XMFLOAT4 elements;
			//DirectX::XMStoreFloat4(&elements, row);
			//TOAST_CORE_INFO("XMMATRIX Planet Transform: ");
			//TOAST_CORE_INFO("						   %f, %f, %f, %f", elements.x, elements.y, elements.z, elements.w);
			//row = transform.r[1];
			//DirectX::XMStoreFloat4(&elements, row);
			//TOAST_CORE_INFO("						   %f, %f, %f, %f", elements.x, elements.y, elements.z, elements.w);
			//row = transform.r[2];
			//DirectX::XMStoreFloat4(&elements, row);
			//TOAST_CORE_INFO("						   %f, %f, %f, %f", elements.x, elements.y, elements.z, elements.w);
			//row = transform.r[3];
			//DirectX::XMStoreFloat4(&elements, row);
			//TOAST_CORE_INFO("						   %f, %f, %f, %f", elements.x, elements.y, elements.z, elements.w);

			//vertices.clear();

			cameraPos = Matrix::Inverse(planetTransform) * cameraPos;

			std::vector<Vector3> startVertices;
			std::vector<uint32_t> startIndices;
			GetBasePlanet(startVertices, startIndices);

			//TOAST_CORE_CRITICAL("cameraPos: %f, %f, %f", DirectX::XMVectorGetX(cameraPos), DirectX::XMVectorGetY(cameraPos), DirectX::XMVectorGetZ(cameraPos));

			for (int i = 0; i < startVertices.size(); i++)
			{
				vertices.emplace_back(startVertices.at(i));
			}

			for (int i = 0; i < startIndices.size(); i++)
			{
				indices.emplace_back(startIndices.at(i));

				//RecursiveFace(numberOfPatches, frustum, planetTransform, vertices.at(i), vertices.at(i+1), vertices.at(i+2), face.Level, subdivisions, patches, distanceLUT, faceLevelDotLUT, heightMultLUT, cameraPos, cameraForward, radius, backfaceCull, frustumCullActivated, true);
			}
		}


		static void GenerateDistanceLUT(std::vector<float>& distanceLUT, float maxSubdivisions, float radius, float FoV, float viewportSizeX)
		{
			// TODO add Vector2 class
			distanceLUT.clear();

			float ratio = ((1.0f + sqrt(5.0f)) / 2.0f);
			float scale = radius / DirectX::XMVectorGetX(DirectX::XMVector2Length(DirectX::XMLoadFloat2(&DirectX::XMFLOAT2(ratio, 1.0f))));
			ratio *= scale;

			float sizeL = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract({ ratio, 0.0f, -scale }, { -ratio, 0.0f, -scale })));
			float frac = tanf((2000.0f * DirectX::XMConvertToRadians(FoV)) / viewportSizeX);

			for (int level = 0; level < maxSubdivisions + 5; level++)
			{
				distanceLUT.emplace_back(sizeL / frac);
				sizeL *= 0.5f;
			}

			//for (auto level : distanceLUT)
			//	TOAST_CORE_INFO("distanceLUT: %f", level);
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

			//for (auto level : faceLevelDotLUT)
			//	TOAST_CORE_INFO("FacelevelDotLUT: %f", level);
		}

		//static void GenerateHeightMultLUT(std::vector<PlanetFace> faces, std::vector<float>& heightMultLUT, float scale, float maxSubdivisions, float maxHeight)
		//{
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
		//}
	};
}