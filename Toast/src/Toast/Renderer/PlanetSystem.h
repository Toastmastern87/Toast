#pragma once

#include "Toast/Core/Timestep.h"
#include "Toast/Core/Math.h"

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
		static void GenerateBasePlanet(std::vector<PlanetFace>& faces, DirectX::XMMATRIX& planetTransform)
		{
			float ratio = ((1.0f + sqrt(5.0f)) / 2.0f);

			std::vector<DirectX::XMVECTOR> startVertices = std::vector<DirectX::XMVECTOR>{
				DirectX::XMVector3Normalize({ ratio, 0.0f, -1.0f }) * 0.5f,
				DirectX::XMVector3Normalize({ -ratio, 0.0f, -1.0f }) * 0.5f,
				DirectX::XMVector3Normalize({ ratio, 0.0f, 1.0f }) * 0.5f,
				DirectX::XMVector3Normalize({ -ratio, 0.0f, 1.0f }) * 0.5f,
				DirectX::XMVector3Normalize({ 0.0f, -1.0f, ratio }) * 0.5f,
				DirectX::XMVector3Normalize({ 0.0f, -1.0f, -ratio }) * 0.5f,
				DirectX::XMVector3Normalize({ 0.0f, 1.0f, ratio }) * 0.5f,
				DirectX::XMVector3Normalize({ 0.0f, 1.0f, -ratio }) * 0.5f,
				DirectX::XMVector3Normalize({ -1.0f, ratio, 0.0f }) * 0.5f,
				DirectX::XMVector3Normalize({ -1.0f, -ratio, 0.0f }) * 0.5f,
				DirectX::XMVector3Normalize({ 1.0f , ratio, 0.0f }) * 0.5f,
				DirectX::XMVector3Normalize({ 1.0f , -ratio, 0.0f }) * 0.5f
			};

			std::vector<uint32_t> startIndices = std::vector<uint32_t>{
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

			faces.clear();

			for (uint32_t i = 0; i < startIndices.size(); i += 3)
				faces.emplace_back(PlanetFace(startVertices[startIndices[i]], startVertices[startIndices[i + (size_t)1]], startVertices[startIndices[i + (size_t)2]], nullptr, (short)0));
		}

		static void GeneratePatchGeometry(std::vector<PlanetVertex>& vertices, std::vector<uint32_t>& indices, int16_t patchLevel)
		{
			vertices.clear();
			indices.clear();

			uint32_t mRC = 1 + (uint32_t)pow(2, patchLevel);

			float delta = 1.0f / ((float)mRC - 1.0f);

			uint32_t rowIndex = 0;
			uint32_t nextIndex = 0;

			for (uint32_t row = 0; row < mRC; row++)
			{
				uint32_t numCols = mRC - row;

				nextIndex += numCols;

				for (uint32_t column = 0; column < numCols; column++)
				{
					// calculate position
					DirectX::XMFLOAT2 pos = { column / ((float)mRC - 1.0f), row / ((float)mRC - 1.0f) };

					//create vertex
					vertices.emplace_back(PlanetVertex(pos));

					//calculate index
					if (row < mRC - 1 && column < numCols - 1)
					{
						indices.emplace_back(rowIndex + column);
						indices.emplace_back(nextIndex + column);
						indices.emplace_back(1 + rowIndex + column);

						if (column < numCols - 2)
						{
							indices.emplace_back(nextIndex + column);
							indices.emplace_back(1 + nextIndex + column);
							indices.emplace_back(1 + rowIndex + column);
						}
					}
				}

				rowIndex = nextIndex;
			}
		}

		static NextPlanetFace CheckPlanetFaceSplit(Frustum* frustum, DirectX::XMMATRIX planetTransform, DirectX::XMVECTOR a, DirectX::XMVECTOR b, DirectX::XMVECTOR c, int16_t subdivision, int16_t maxSubdivisions, std::vector<float>& distanceLUT, std::vector<float>& faceLevelDotLUT, std::vector<float>& heightMultLUT, DirectX::XMVECTOR& cameraPos, DirectX::XMVECTOR& cameraForward, bool backfaceCull, bool frustumCullActivated, bool frustumCull)
		{
			DirectX::XMVECTOR planetTranslation, planetRotation, planetScale;

			DirectX::XMMatrixDecompose(&planetScale, &planetRotation, &planetTranslation, planetTransform);

			XMVECTOR aBeforeTransform = a;
			XMVECTOR bBeforeTransform = b;
			XMVECTOR cBeforeTransform = c;

			float aDistance, bDistance, cDistance;

			DirectX::XMVECTOR center = (a + b + c) / 3.0f;

			DirectX::XMVECTOR dotProduct = DirectX::XMVector3Dot(DirectX::XMVector3Normalize(center), DirectX::XMVector3Normalize(center - cameraPos));//DirectX::XMVector3Dot(center, center - cameraPos);

			if (backfaceCull && DirectX::XMVectorGetX(dotProduct) >= (faceLevelDotLUT[(uint32_t)subdivision] + 0.1f)) 
				return NextPlanetFace::CULL;

			if (frustumCullActivated && frustumCull)
			{
				auto intersect = frustum->ContainsTriangleVolume(a, b, c, heightMultLUT[(uint32_t)subdivision]);

				if (intersect == VolumeTri::OUTSIDE) 
					return NextPlanetFace::CULL;

				if (intersect == VolumeTri::CONTAINS)//stop frustum culling -> all children are also inside the frustum
				{
					//check if new splits are allowed
					if (subdivision >= maxSubdivisions)
						return NextPlanetFace::LEAF;

					//split according to distance
					aDistance = DirectX::XMVectorGetX(DirectX::XMVector3Length(a - cameraPos));
					bDistance = DirectX::XMVectorGetX(DirectX::XMVector3Length(b - cameraPos));
					cDistance = DirectX::XMVectorGetX(DirectX::XMVector3Length(c - cameraPos));

					if (std::fminf(aDistance, std::fminf(bDistance, cDistance)) < distanceLUT[(uint32_t)subdivision])
						return NextPlanetFace::SPLIT;

					return NextPlanetFace::LEAF;
				}
			}

			if (subdivision >= maxSubdivisions) 
				return NextPlanetFace::LEAF;

			aDistance = DirectX::XMVectorGetX(DirectX::XMVector3Length(a - cameraPos));
			bDistance = DirectX::XMVectorGetX(DirectX::XMVector3Length(b - cameraPos));
			cDistance = DirectX::XMVectorGetX(DirectX::XMVector3Length(c - cameraPos));			

			if (fminf(aDistance, fminf(bDistance, cDistance)) < distanceLUT[(uint32_t)subdivision]) 
				return NextPlanetFace::SPLITCULL;

			return NextPlanetFace::LEAF;
		}

		static void RecursiveFace(Frustum* frustum, DirectX::XMMATRIX planetTransform, DirectX::XMVECTOR& a, DirectX::XMVECTOR& b, DirectX::XMVECTOR& c, int16_t subdivision, int16_t maxSubdivisions, std::vector<PlanetPatch>& patches, std::vector<float>& distanceLUT, std::vector<float>& faceLevelDotLUT, std::vector<float>& heightMultLUT, DirectX::XMVECTOR& cameraPos, DirectX::XMVECTOR& cameraForward, bool backfaceCull, bool frustumCullActivated, bool frustumCull)
		{
			DirectX::XMVECTOR A, B, C;

			NextPlanetFace nextPlanetFace = CheckPlanetFaceSplit(frustum, planetTransform, a, b, c, subdivision, maxSubdivisions, distanceLUT, faceLevelDotLUT, heightMultLUT, cameraPos, cameraForward, backfaceCull, frustumCullActivated, frustumCull);
			
			if (nextPlanetFace == NextPlanetFace::CULL)
				return;

			if (subdivision < maxSubdivisions && (nextPlanetFace == NextPlanetFace::SPLIT || nextPlanetFace == NextPlanetFace::SPLITCULL)) {
				A = b + ((c - b) * 0.5f);
				B = c + ((a - c) * 0.5f);
				C = a + ((b - a) * 0.5f);

				A = DirectX::XMVector3Normalize(A) * 0.5f;
				B = DirectX::XMVector3Normalize(B) * 0.5f;
				C = DirectX::XMVector3Normalize(C) * 0.5f;

				int16_t nextSubdivision = subdivision + 1;

				RecursiveFace(frustum, planetTransform, C, B, a, nextSubdivision, maxSubdivisions, patches, distanceLUT, faceLevelDotLUT, heightMultLUT, cameraPos, cameraForward, backfaceCull, frustumCullActivated, nextPlanetFace == NextPlanetFace::SPLITCULL);
				RecursiveFace(frustum, planetTransform, b, A, C, nextSubdivision, maxSubdivisions, patches, distanceLUT, faceLevelDotLUT, heightMultLUT, cameraPos, cameraForward, backfaceCull, frustumCullActivated, nextPlanetFace == NextPlanetFace::SPLITCULL);
				RecursiveFace(frustum, planetTransform, B, A, c, nextSubdivision, maxSubdivisions, patches, distanceLUT, faceLevelDotLUT, heightMultLUT, cameraPos, cameraForward, backfaceCull, frustumCullActivated, nextPlanetFace == NextPlanetFace::SPLITCULL);
				RecursiveFace(frustum, planetTransform, A, B, C, nextSubdivision, maxSubdivisions, patches, distanceLUT, faceLevelDotLUT, heightMultLUT, cameraPos, cameraForward, backfaceCull, frustumCullActivated, nextPlanetFace == NextPlanetFace::SPLITCULL);
			}
			else
			{
				DirectX::XMFLOAT3 firstCorner, secondCorner, thirdCorner;

				DirectX::XMStoreFloat3(&firstCorner, a);
				DirectX::XMStoreFloat3(&secondCorner, b - a);
				DirectX::XMStoreFloat3(&thirdCorner, c - a);

				patches.emplace_back(PlanetPatch(subdivision, firstCorner, thirdCorner, secondCorner));
			}
		}

		static void GeneratePlanet(Frustum* frustum, DirectX::XMMATRIX planetTransform, std::vector<PlanetFace>& faces, std::vector<PlanetPatch>& patches, std::vector<float>& distanceLUT, std::vector<float>& faceLevelDotLUT, std::vector<float>& heightMultLUT, DirectX::XMVECTOR cameraPos, DirectX::XMVECTOR& cameraForward, int16_t subdivisions, bool backfaceCull, bool frustumCullActivated)
		{
			patches.clear();

			cameraPos = DirectX::XMVector3Transform(cameraPos, DirectX::XMMatrixInverse(nullptr, planetTransform));

			for (auto& face : faces)
				RecursiveFace(frustum, planetTransform, face.A, face.B, face.C, face.Level, subdivisions, patches, distanceLUT, faceLevelDotLUT, heightMultLUT, cameraPos, cameraForward, backfaceCull, frustumCullActivated, true);
		}

		static void GenerateDistanceLUT(std::vector<float>& distanceLUT, float maxSubdivisions, float scale)
		{
			distanceLUT.clear();

			for (int subdivision = 0; subdivision < maxSubdivisions; subdivision++)
				distanceLUT.emplace_back(25123.8186f * exp(-0.9725f * (subdivision + 1)) / scale);

			//for (auto level : distanceLUT)
			//	TOAST_CORE_INFO("distanceLUT: %f", level);
		}

		static void GenerateFaceDotLevelLUT(std::vector<float>& faceLevelDotLUT, float scale, float maxSubdivisions, float maxHeight)
		{
			float cullingAngle = acosf((scale * 0.5f) / ((scale * 0.5f) + maxHeight));

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

		static void GenerateHeightMultLUT(std::vector<PlanetFace> faces, std::vector<float>& heightMultLUT, float scale, float maxSubdivisions, float maxHeight, DirectX::XMMATRIX planetTransform)
		{
			DirectX::XMVECTOR planetTranslation, planetRotation, planetScale;

			DirectX::XMMatrixDecompose(&planetScale, &planetRotation, &planetTranslation, planetTransform);

			heightMultLUT.clear();
			DirectX::XMVECTOR a = faces[0].A;
			DirectX::XMVECTOR b = faces[0].B;
			DirectX::XMVECTOR c = faces[0].C;

			DirectX::XMVECTOR center = (a + b + c) / 3.0f;
			center *= (scale * 0.5f) / DirectX::XMVectorGetX(DirectX::XMVector3Length(center));//+maxHeight 
			heightMultLUT.push_back(1.0f / DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMVector3Normalize(a), DirectX::XMVector3Normalize(center))));
			float normMaxHeight = maxHeight / (scale * 0.5f);
			for (int i = 1; i <= maxSubdivisions; i++)
			{
				DirectX::XMVECTOR A = b + ((c - b) * 0.5f);
				DirectX::XMVECTOR B = c + ((a - c) * 0.5f);
				c = a + ((b - a) * 0.5f);
				a = A * (scale * 0.5f) / DirectX::XMVectorGetX(DirectX::XMVector3Length(A));
				b = B * (scale * 0.5f) / DirectX::XMVectorGetX(DirectX::XMVector3Length(B));
				c *= (scale * 0.5f) / DirectX::XMVectorGetX(DirectX::XMVector3Length(c));
				heightMultLUT.push_back(1.0f / DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMVector3Normalize(a), DirectX::XMVector3Normalize(center))) + normMaxHeight);
			}

			//for (auto level : heightMultLUT)
			//	TOAST_CORE_INFO("heightMultLUT: %f", level);
		}
	};
}