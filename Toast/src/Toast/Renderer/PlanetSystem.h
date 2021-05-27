#pragma once

#include "Toast/Core/Timestep.h"
#include "Toast/Core/Math.h"

#include "Toast/Renderer/Mesh.h"
#include "Toast/Renderer/RenderCommand.h"

#include <../vendor/directxtk/Inc/ScreenGrab.h>

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
		static void GenerateBasePlanet(std::vector<PlanetFace>& faces)
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

			for (uint32_t i = 0; i < startIndices.size(); i += 3)
			{
				faces.push_back(PlanetFace(startVertices[startIndices[i]], startVertices[startIndices[i + 1]], startVertices[startIndices[i + 2]], nullptr, (short)0));
			}
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

					// calculate morph factor
					DirectX::XMFLOAT2 morph = { 0.0f, 0.0f };

					if (row % 2 == 0)
					{
						if (column % 2 == 1)
							morph = { -delta, 0.0f };
					}
					else
					{
						if (column % 2 == 0)
							morph = { 0, delta };
						else
							morph = { delta, -delta };
					}

					//create vertex
					vertices.push_back(PlanetVertex(pos, morph));

					//calculate index
					if (row < mRC - 1 && column < numCols - 1)
					{
						indices.push_back(rowIndex + column);
						indices.push_back(nextIndex + column);
						indices.push_back(1 + rowIndex + column);

						if (column < numCols - 2)
						{
							indices.push_back(nextIndex + column);
							indices.push_back(1 + nextIndex + column);
							indices.push_back(1 + rowIndex + column);
						}
					}
				}

				rowIndex = nextIndex;
			}
		}

		static NextPlanetFace CheckPlanetFaceSplit(DirectX::XMMATRIX planetTransform, DirectX::XMVECTOR& a, DirectX::XMVECTOR& b, DirectX::XMVECTOR& c, int16_t subdivision, int16_t maxSubdivisions, std::vector<float>& distanceLUT, std::vector<float>& faceLevelDotLUT, DirectX::XMVECTOR& cameraPos)
		{
			float aDistance, bDistance, cDistance;

			DirectX::XMVECTOR center = (a + b + c) / 3.0f;

			DirectX::XMVECTOR dotNV = DirectX::XMVector3Dot(DirectX::XMVector3Normalize(center), DirectX::XMVector3Normalize(center - cameraPos));

			if (DirectX::XMVectorGetX(dotNV) >= faceLevelDotLUT[subdivision + 3]) 
				return NextPlanetFace::CULL;

			if (subdivision >= maxSubdivisions + 3)
				return NextPlanetFace::LEAF;

			aDistance = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVector3Transform(a, planetTransform) - cameraPos));
			bDistance = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVector3Transform(b, planetTransform) - cameraPos));
			cDistance = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVector3Transform(c, planetTransform) - cameraPos));			

			if (fminf(aDistance, fminf(bDistance, cDistance)) < distanceLUT[subdivision+3]) 
				return NextPlanetFace::SPLITCULL;

			return NextPlanetFace::LEAF;
		}

		static void RecursiveFace(DirectX::XMMATRIX planetTransform, DirectX::XMVECTOR& a, DirectX::XMVECTOR& b, DirectX::XMVECTOR& c, int16_t subdivision, int16_t maxSubdivisions, std::vector<PlanetPatch>& patches, std::vector<float>& distanceLUT, std::vector<float>& faceLevelDotLUT, DirectX::XMVECTOR& cameraPos)
		{
			DirectX::XMVECTOR A, B, C;

			NextPlanetFace nextSphereFace = CheckPlanetFaceSplit(planetTransform, a, b, c, subdivision, maxSubdivisions, distanceLUT, faceLevelDotLUT, cameraPos);
			
			if (nextSphereFace == NextPlanetFace::CULL) 
				return;

			if (subdivision < maxSubdivisions && (nextSphereFace == NextPlanetFace::SPLIT || nextSphereFace == NextPlanetFace::SPLITCULL)) {
				A = b + ((c - b) * 0.5f);
				B = c + ((a - c) * 0.5f);
				C = a + ((b - a) * 0.5f);

				A = DirectX::XMVector3Normalize(A) * 0.5f;
				B = DirectX::XMVector3Normalize(B) * 0.5f;
				C = DirectX::XMVector3Normalize(C) * 0.5f;

				int16_t nextSubdivision = subdivision + 1;

				RecursiveFace(planetTransform, C, B, a, nextSubdivision, maxSubdivisions, patches, distanceLUT, faceLevelDotLUT, cameraPos);
				RecursiveFace(planetTransform, b, A, C, nextSubdivision, maxSubdivisions, patches, distanceLUT, faceLevelDotLUT, cameraPos);
				RecursiveFace(planetTransform, B, A, c, nextSubdivision, maxSubdivisions, patches, distanceLUT, faceLevelDotLUT, cameraPos);
				RecursiveFace(planetTransform, A, B, C, nextSubdivision, maxSubdivisions, patches, distanceLUT, faceLevelDotLUT, cameraPos);
			}
			else
			{
				DirectX::XMFLOAT3 firstCorner, secondCorner, thirdCorner;

				DirectX::XMStoreFloat3(&firstCorner, a);
				DirectX::XMStoreFloat3(&secondCorner, b - a);
				DirectX::XMStoreFloat3(&thirdCorner, c - a);

				patches.push_back(PlanetPatch(subdivision+3, firstCorner, thirdCorner, secondCorner));
			}
		}

		static void GeneratePlanet(DirectX::XMMATRIX planetTransform, std::vector<PlanetFace>& faces, std::vector<PlanetPatch>& patches, std::vector<float>& distanceLUT, std::vector<float>& faceLevelDotLUT, DirectX::XMVECTOR& cameraPos, int16_t subdivisions)
		{
			patches.clear();

			for (auto& face : faces)
				RecursiveFace(planetTransform, face.A, face.B, face.C, face.Level, subdivisions, patches, distanceLUT, faceLevelDotLUT, cameraPos);
		}

		static void GenerateDistanceLUT(std::vector<float>& distanceLUT, float scale, float fov, float width, float maxTriangleSize, float maxSubdivisions)
		{
			float frac = tanf((maxTriangleSize * DirectX::XMConvertToRadians(fov)) / width);

			for (int subdivision = 0; subdivision < maxSubdivisions+6; subdivision++) 
				distanceLUT.push_back((GetPlanetVertexDistance(scale) / frac) * powf(0.5f, (float)subdivision));
		}

		static void GenerateFaceDotLevelLUT(std::vector<float>& faceLevelDotLUT, float scale, float maxSubdivisions, float maxHeight)
		{
			// TODO, add height in the future + m_pPlanet->GetMaxHeight())
			float cullingAngle = acosf((scale * 0.5f) / ((scale * 0.5f) + maxHeight));
			
			faceLevelDotLUT.clear();
			faceLevelDotLUT.push_back(0.5f + sinf(cullingAngle));
			float angle = acosf(0.5f);
			for (int i = 1; i <= maxSubdivisions+6; i++)
			{
				angle *= 0.5f;
				faceLevelDotLUT.push_back(sinf(angle + cullingAngle));
			}
		}

		static float GetPlanetVertexDistance(const float planetScale) 
		{
			float ratio = ((1.0f + sqrt(5.0f)) / 2.0f);

			DirectX::XMVECTOR firstPoint = DirectX::XMVector3Normalize({ ratio, 0.0f, -1.0f }) * 0.5f;
			DirectX::XMVECTOR secondPoint = DirectX::XMVector3Normalize({ -ratio, 0.0f, -1.0f }) * 0.5f;
			
			firstPoint = DirectX::XMVectorScale(firstPoint, planetScale);
			secondPoint = DirectX::XMVectorScale(secondPoint, planetScale);

			return DirectX::XMVectorGetX(DirectX::XMVector3Length(firstPoint - secondPoint));
		}

		static void CraterDetailIncrease(Ref<Material> material) 
		{
			RendererAPI* API = RenderCommand::sRendererAPI.get();
			ID3D11DeviceContext* deviceContext = API->GetDeviceContext();

			RENDERDOC_API_1_1_2* rdoc_api = NULL;

			// At init, on windows
			if (HMODULE mod = GetModuleHandleA("renderdoc.dll"))
			{
				pRENDERDOC_GetAPI RENDERDOC_GetAPI =
					(pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
				int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void**)&rdoc_api);
				assert(ret == 1);
			}

			Ref<Shader> averageHeightShader, lowerThenAverageShader, craterFilteringShader;
			Ref<Texture2D> heightMap = std::static_pointer_cast<Texture2D>(material->GetTexture("HeightMapTexture"));
			Ref<Texture2D> averageHeightMap = CreateRef<Texture2D>(DXGI_FORMAT_R32G32B32A32_FLOAT, heightMap->GetWidth(), heightMap->GetHeight());
			Ref<Texture2D> rawCraterMap = CreateRef<Texture2D>(DXGI_FORMAT_R32G32B32A32_FLOAT, heightMap->GetWidth(), heightMap->GetHeight());
			Ref<TextureSampler> defaultSampler = TextureLibrary::GetSampler("Default");

			averageHeightMap->CreateUAV(0);
			rawCraterMap->CreateUAV(0);

			if (!averageHeightShader)
				averageHeightShader = CreateRef<Shader>("assets/shaders/Planet/AverageHeight.hlsl");

			averageHeightShader->Bind();
			heightMap->Bind();
			defaultSampler->Bind(0, D3D11_COMPUTE_SHADER);
			averageHeightMap->BindForReadWrite(0, D3D11_COMPUTE_SHADER);

			if (rdoc_api) rdoc_api->StartFrameCapture(NULL, NULL);
			RenderCommand::DispatchCompute(heightMap->GetWidth() / 32, heightMap->GetHeight() / 32, 1);
			if (rdoc_api) rdoc_api->EndFrameCapture(NULL, NULL);

			averageHeightMap->UnbindUAV();

			if (!lowerThenAverageShader)
				lowerThenAverageShader = CreateRef<Shader>("assets/shaders/Planet/LowerThenAverage.hlsl");

			lowerThenAverageShader->Bind();
			heightMap->Bind(0, D3D11_COMPUTE_SHADER);
			averageHeightMap->Bind(1, D3D11_COMPUTE_SHADER);
			defaultSampler->Bind(0, D3D11_COMPUTE_SHADER);
			rawCraterMap->BindForReadWrite(0, D3D11_COMPUTE_SHADER);

			if (rdoc_api) rdoc_api->StartFrameCapture(NULL, NULL);
			RenderCommand::DispatchCompute(heightMap->GetWidth() / 32, heightMap->GetHeight() / 32, 1);
			if (rdoc_api) rdoc_api->EndFrameCapture(NULL, NULL);

			rawCraterMap->UnbindUAV();

			DirectX::SaveDDSTextureToFile(deviceContext, rawCraterMap->GetTexture().Get(), L"RawCraterMap.dds");

			TOAST_CORE_WARN("Planet crater detailed increased");
		}

		static void GaborAnnulus() 
		{
		}
	};
}