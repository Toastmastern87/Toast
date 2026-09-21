#include "tpch.h"
#include "Toast/Renderer/Renderer2D.h"

#include "Toast/Assets/AssetManager.h"

#include "Toast/Renderer/Shader.h"

#include "Toast/Renderer/RendererBuffer.h"
#include "Toast/Renderer/RenderCommand.h"

#include "Toast/Renderer/UI/Font.h"
#include "Toast/Renderer/UI/MSDFData.h"

namespace Toast {

	Scope<Renderer2D::Renderer2DData> Renderer2D::sRenderer2DData = CreateScope<Renderer2D::Renderer2DData>();

	// Sum of advances for one line, in the same units the pen uses.
	static double MeasureLine(const std::string& line, const msdf_atlas::FontGeometry& fontGeometry, double fsScale) 
	{
		double width = 0.0;

		for (size_t i = 0; i < line.size(); i++)
		{
			char32_t character = (char32_t)line[i];

			auto glyph = fontGeometry.getGlyph(character);
			if (!glyph)
				glyph = fontGeometry.getGlyph('?');
			if (!glyph)
				continue;

			double advance = glyph->getAdvance();

			char32_t nextChar = (i + 1 < line.size()) ? (char32_t)line[i + 1] : 0;
			fontGeometry.getAdvance(advance, character, nextChar);

			width += fsScale * advance;
		}

		return width;
	}

	static void BuildLines(const std::string& textString, const msdf_atlas::FontGeometry& fontGeometry, double fsScale, double maxWidth, std::vector<std::string>& outLines)
	{
		outLines.clear();

		size_t paragraphStart = 0;

		while (true)
		{
			const size_t newline = textString.find('\n', paragraphStart);
			const std::string paragraph = textString.substr(paragraphStart, newline == std::string::npos ? std::string::npos : newline - paragraphStart);

			//No wrapping
			if (maxWidth <= 0.0)
				outLines.push_back(paragraph);
			else 
			{
				size_t lineStart = 0;
				size_t lastSpace = std::string::npos;
				double width = 0.0;

				for (size_t i = 0; i < paragraph.size(); i++)
				{
					const char32_t character = (char32_t)paragraph[i];

					if (character == ' ')
						lastSpace = i;

					auto glyph = fontGeometry.getGlyph(character);
					if (!glyph)
						glyph = fontGeometry.getGlyph('?');

					double advance = 0.0;
					if (glyph)
					{
						advance = glyph->getAdvance();

						const char32_t nextChar = (i + 1 < paragraph.size()) ? (char32_t)paragraph[i + 1] : 0;
						fontGeometry.getAdvance(advance, character, nextChar);
					}

					width += fsScale * advance;

					if (width <= maxWidth)
						continue;

					// Over the limit. Break at the last space if there was one
					// after the line start; otherwise force-break here, because a
					// single word longer than the line would otherwise never fit
					// and the loop would never advance.
					if (lastSpace != std::string::npos && lastSpace > lineStart)
					{
						outLines.push_back(paragraph.substr(lineStart, lastSpace - lineStart));
						lineStart = lastSpace + 1;
						i = lastSpace;
					}
					else 
					{
						const size_t breakAt = (i > lineStart) ? i : lineStart + 1;

						outLines.push_back(paragraph.substr(lineStart, breakAt - lineStart));
						lineStart = breakAt;
						i = breakAt - 1;
					}

					lastSpace = std::string::npos;
					width = 0.0;
				}

				outLines.push_back(paragraph.substr(lineStart));
			}

			if (newline == std::string::npos)
				break;

			paragraphStart = newline + 1;
		}
	}

	static void ApplyImageFit(ImageFit fit, const DirectX::XMFLOAT2& boxPos, const DirectX::XMFLOAT2& boxSize, float textureAspect, DirectX::XMFLOAT2& outQuadPos, DirectX::XMFLOAT2& outQuadSize, DirectX::XMFLOAT4& outUV)
	{
		outQuadPos = boxPos;
		outQuadSize = boxSize;
		outUV = { 0.0f, 0.0f, 1.0f, 1.0f };

		if (boxSize.x <= 0.0f || boxSize.y <= 0.0f || textureAspect <= 0.0f)
			return;

		const float boxAspect = boxSize.x / boxSize.y;

		switch (fit)
		{
		case ImageFit::Stretch:
			break;

		case ImageFit::Contain:
			if (textureAspect > boxAspect)
			{
				outQuadSize.y = boxSize.x / textureAspect;
				outQuadPos.y = boxPos.y + (boxSize.y - outQuadSize.y) * 0.5f;
			}
			else 
			{
				outQuadSize.x = boxSize.y * textureAspect;
				outQuadPos.x = boxPos.x + (boxSize.x - outQuadSize.x) * 0.5f;
			}
			break;
			
		case ImageFit::Cover:
			if (textureAspect > boxAspect)
			{
				const float visible = boxAspect / textureAspect;
				outUV.x = (1.0f - visible) * 0.5f;
				outUV.z = visible;
			}
			else
			{
				const float visible = textureAspect / boxAspect;
				outUV.y = (1.0f - visible) * 0.5f;
				outUV.w = visible;
			}
			break;

		case ImageFit::None:
			break;
		}
	}

	static bool GetNineSliceData(AssetHandle handle, DirectX::XMFLOAT4& outInsets, DirectX::XMFLOAT4& outContent)
	{
		if (handle == AssetHandle(0))
			return false;

		const AssetEntry* entry = AssetManager::GetEntry(handle);
		if (!entry)
			return false;

		if (entry->Metadata.Type != AssetType::Texture2D)
			return false;

		const auto& s = entry->Texture2DSettings;

		const bool hasInsets = s.SliceLeft || s.SliceTop || s.SliceRight || s.SliceBottom;
		const bool hasContent = s.ContentWidth || s.ContentHeight;

		if (!hasInsets && !hasContent)
			return false;

		auto texture = AssetManager::GetAsset<Texture2D>(handle);
		if (!texture)
			return false;

		outInsets = { (float)s.SliceLeft, (float)s.SliceTop, (float)s.SliceRight, (float)s.SliceBottom };

		outContent = { (float)s.ContentX, (float)s.ContentY, s.ContentWidth ? (float)s.ContentWidth : (float)texture->GetWidth(), s.ContentHeight ? (float)s.ContentHeight : (float)texture->GetHeight() };

		return true;
	}

	void Renderer2D::Init()
	{
		TOAST_PROFILE_FUNCTION();

		sRenderer2DData->UIVertexBufferBase = new UIVertex[sRenderer2DData->MaxUIVertices];
		sRenderer2DData->UIVertexBufferPtr = sRenderer2DData->UIVertexBufferBase;
		sRenderer2DData->UIVertexBufferEnd = sRenderer2DData->UIVertexBufferBase + sRenderer2DData->MaxUIVertices;
		uint32_t* UIIndices = new uint32_t[sRenderer2DData->MaxUIIndices];

		uint32_t offset = 0;
		for (uint32_t i = 0; i < sRenderer2DData->MaxUIIndices; i += 6)
		{
			UIIndices[i + 0] = offset + 0;
			UIIndices[i + 1] = offset + 2;
			UIIndices[i + 2] = offset + 1;
			
			UIIndices[i + 3] = offset + 2;
			UIIndices[i + 4] = offset + 0;
			UIIndices[i + 5] = offset + 3;

			offset += 4;
		}

		sRenderer2DData->UIVertexBuffer = CreateRef<VertexBuffer>(&sRenderer2DData->UIVertexBufferBase[0], (uint32_t)(sRenderer2DData->MaxUIVertices * sizeof(UIVertex)), sRenderer2DData->MaxUIVertices, 0, D3D11_USAGE_DYNAMIC);
		sRenderer2DData->UIIndexBuffer = CreateRef<IndexBuffer>(&UIIndices[0], sRenderer2DData->MaxUIIndices);

		LoadFontTextures();
	}

	void Renderer2D::Shutdown()
	{
		TOAST_PROFILE_FUNCTION();
	}

	void Renderer2D::LoadUITextures()
	{
		std::filesystem::path assetDir = AssetManager::GetAssetDirectory();
		std::filesystem::path uiTextureDir = assetDir / UI_TEXTURE_DIRECTORY;

		if (!std::filesystem::exists(uiTextureDir) || std::filesystem::is_empty(uiTextureDir))
		{
			TOAST_CORE_WARN("Renderer2D::LoadUITextures: No UI textures found in '%s'", uiTextureDir.string().c_str());
			return;
		}

		std::vector<AssetHandle> textureHandles;
		std::vector<Ref<Texture2D>> loadedTextures;

		for (const auto& entry : std::filesystem::directory_iterator(uiTextureDir))
		{
			if (!entry.is_regular_file())
				continue;

			auto relativePath = std::filesystem::relative(entry.path(), assetDir);
			AssetHandle handle = AssetManager::ImportAsset(relativePath);
			auto texture = AssetManager::GetAsset<Texture2D>(handle);
			if (texture)
			{
				loadedTextures.push_back(texture);
				textureHandles.push_back(handle);
			}
		}

		if (textureHandles.empty())
			return;

		uint32_t width = loadedTextures[0]->GetWidth();
		uint32_t height = loadedTextures[0]->GetHeight();
		DXGI_FORMAT format = loadedTextures[0]->GetFormat();

		std::vector<const void*> initialData;
		std::vector<UINT> rowPitches;

		std::vector<AssetHandle> validHandles;

		for (size_t i = 0; i < loadedTextures.size(); i++)
		{
			const auto& texture = loadedTextures[i];

			if (texture->GetWidth() != width || texture->GetHeight() != height)
			{
				TOAST_CORE_ERROR("Renderer2D::LoadUITextures: '%s' is %ux%u but the array is %ux%u. All UI textures must match. Skipping.", texture->GetFilePath().c_str(), texture->GetWidth(), texture->GetHeight(), width, height);
				continue;
			}

			if (!texture->GetInitialData() || texture->GetRowPitch() == 0)
			{
				TOAST_CORE_ERROR("Renderer2D::LoadUITextures: '%s' has no initial data. Skipping.", texture->GetFilePath().c_str());
				continue;
			}

			initialData.push_back(texture->GetInitialData());
			rowPitches.push_back(texture->GetRowPitch());
			validHandles.push_back(textureHandles[i]);
		}

		if (initialData.empty())
		{
			TOAST_CORE_ERROR("Renderer2D::LoadUITextures: no UI textures matched %ux%u, array not created.", width, height);
			return;
		}

		sRenderer2DData->UITextureArray = CreateRef<Texture2DArray>(format, width, height, (uint32_t)initialData.size(), D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 1, 0, initialData, rowPitches);
		sRenderer2DData->UITextureArray->SetSliceMapping(validHandles);

		TOAST_CORE_INFO("Renderer2D::LoadUITextures: %zu of %zu UI textures loaded at %ux%u", initialData.size(), loadedTextures.size(), width, height);
	}

	void Renderer2D::BeginScene(Camera& camera)
	{
		TOAST_PROFILE_FUNCTION();

		RenderCommand::SetPrimitiveTopology(PrimitiveTopology::TRIANGLELIST);

		sRendererData->CameraBuffer.Write((uint8_t*)&camera.GetViewMatrix(), 64, 64);
		sRendererData->CameraBuffer.Write((uint8_t*)&camera.GetOrthoProjection(), 64, 128);
		sRendererData->CameraCBuffer->Map(sRendererData->CameraBuffer);

		sRenderer2DData->UIVertexBufferPtr = sRenderer2DData->UIVertexBufferBase;
		sRenderer2DData->UIBufferOverflowed = false;

		sRenderer2DData->UIImageDraws.clear();
		sRenderer2DData->UIBatchQuadCount = 0;
	}

	void Renderer2D::EndScene()
	{
		TOAST_PROFILE_FUNCTION();

#ifdef TOAST_DEBUG
		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation = nullptr;
		RenderCommand::GetAnnotation(annotation);
		if (annotation)
			annotation->BeginEvent(L"2D Render Pass");
#endif

		RenderCommand::SetRenderTargets({ sRendererData->FinalRT->GetRTV().Get(), sRendererData->FinalEditorRT->GetRTV().Get(), sRendererData->GPassPickingRT->GetRTV().Get() }, sRendererData->DepthStencilView);
		RenderCommand::ClearDepthStencilView(sRendererData->DepthStencilView);
		RenderCommand::SetDepthStencilState(sRendererData->DepthEnabledStencilState);
		RenderCommand::SetBlendState(sRendererData->UIBlendState, { 0.0f, 0.0f, 0.0f, 0.0f });

		// New way of rendering UI with one big vertex buffer
		uint32_t vertexCount = sRenderer2DData->UIVertexBufferPtr - sRenderer2DData->UIVertexBufferBase;
		uint32_t vertexDataSize = vertexCount * sizeof(UIVertex);
		uint32_t quadCount = vertexCount / 4; 

		const uint32_t batchQuads = sRenderer2DData->UIImageDraws.empty() ? quadCount : sRenderer2DData->UIBatchQuadCount;

		auto shader = AssetManager::GetAsset<Shader>(sRendererData->UIShaderHandle);
		if (shader)
			shader->Bind();

		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 6, sRenderer2DData->FontsTextureArray->GetSRV());
		if(sRenderer2DData->UITextureArray)
			RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 8, sRenderer2DData->UITextureArray->GetSRV());

		sRenderer2DData->UIVertexBuffer->SetData(sRenderer2DData->UIVertexBufferBase, vertexDataSize);
		sRenderer2DData->UIVertexBuffer->Bind();
		sRenderer2DData->UIIndexBuffer->Bind();

		if (batchQuads > 0)
			RenderCommand::DrawIndexed(0, 0, batchQuads * 6);

		// Images
		for (const auto& imageDraw : sRenderer2DData->UIImageDraws)
		{
			RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 9, imageDraw.Texture->GetSRV());
			RenderCommand::DrawIndexed(0, imageDraw.FirstIndex, 6);
		}
		
		RenderCommand::SetRenderTargets({ sRendererData->BackbufferRT->GetRTV().Get() }, nullptr);
		RenderCommand::ClearRenderTargets(sRendererData->BackbufferRT->GetRTV().Get(), { 0.0f, 0.0f, 0.0f, 1.0f });

		RenderCommand::ClearShaderResources();
		RenderCommand::SetViewport(sRendererData->EditorViewport);

#ifdef TOAST_DEBUG
		if (annotation)
			annotation->EndEvent();
#endif
	}

	void Renderer2D::SubmitPanel(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT4& size, DirectX::XMFLOAT4& color, const int entityID, const bool textured, const bool targetable, uint32_t textureIndex, AssetHandle textureHandle, float borderWidth, const DirectX::XMFLOAT4& borderColor)
	{
		TOAST_PROFILE_FUNCTION();

		if (!HasRoomForQuad())
			return;

		DirectX::XMFLOAT4 insets, content;
		const bool hasNineSlice = textured && GetNineSliceData(textureHandle, insets, content);

		const DirectX::XMFLOAT4 params = hasNineSlice ? insets : DirectX::XMFLOAT4{ borderWidth, borderColor.x, borderColor.y, borderColor.z };
		const DirectX::XMFLOAT4 params2 = hasNineSlice ? content : DirectX::XMFLOAT4{ 0.0f, 0.0f, 0.0f, 0.0f };

		const DirectX::XMFLOAT4 sizeField = { size.x, size.y, size.z, hasNineSlice ? 1.0f : 0.0f};

		float texturedF = textured == true ? 1.0f : 0.0f;

		DirectX::XMFLOAT3 textureCoords[] = { DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f), DirectX::XMFLOAT3(1.0f, 0.0f, 1.0f), DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f), DirectX::XMFLOAT3(0.0f, 1.0f, 1.0f) };

		DirectX::XMFLOAT4 UIVertexPositions[4];
		UIVertexPositions[0] = { pos.x,          pos.y,          pos.z, texturedF };
		UIVertexPositions[1] = { pos.x + size.x, pos.y,          pos.z, texturedF };
		UIVertexPositions[2] = { pos.x + size.x, pos.y + size.y, pos.z, texturedF };
		UIVertexPositions[3] = { pos.x,          pos.y + size.y, pos.z, texturedF };

		for (size_t i = 0; i < 4; i++)
		{
			sRenderer2DData->UIVertexBufferPtr->Position = UIVertexPositions[i];
			sRenderer2DData->UIVertexBufferPtr->Size = sizeField;
			sRenderer2DData->UIVertexBufferPtr->Color = color;
			sRenderer2DData->UIVertexBufferPtr->Texcoord = textureCoords[i];
			sRenderer2DData->UIVertexBufferPtr->EntityID = entityID;
			sRenderer2DData->UIVertexBufferPtr->TextureIndex = textureIndex;
			sRenderer2DData->UIVertexBufferPtr->Params = params;
			sRenderer2DData->UIVertexBufferPtr->Params2 = params2;
			sRenderer2DData->UIVertexBufferPtr++;
		}
	}

	void Renderer2D::SubmitConnector(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, float thicknessPx, ConnectorStyle style, float cornerRadiusPx, const DirectX::XMFLOAT4& color, float outlineWidthPx, const DirectX::XMFLOAT4& outlineColor, int entityID)
	{
		TOAST_PROFILE_FUNCTION();

		if (!HasRoomForQuad())
			return;

		thicknessPx = std::max(thicknessPx, 1.0f);
		cornerRadiusPx = std::max(cornerRadiusPx, 0.0f);

		// The quad must contain the stroke, the smin bulge at an elbow joint, and
		// a pixel of anti aliasing.
		float pad = 0.5f * thicknessPx + cornerRadiusPx + outlineWidthPx + 2.0f;

		float minX = std::min(a.x, b.x) - pad;
		float minY = std::min(a.y, b.y) - pad;
		float maxX = std::max(a.x, b.x) + pad;
		float maxY = std::max(a.y, b.y) + pad;

		// Quad in UI pixel space (0,0 top-left)
		// Force the depth(z) to be the same as b.z to avoid z-fighting with panels
		// Position.w is the 'textured' channel, which connectors never use - it
		// carries the elbow corner radius instead. Named for its original purpose
		// in the shader, so both ends are commented.
		DirectX::XMFLOAT4 p0{ minX, minY, b.z, cornerRadiusPx };
		DirectX::XMFLOAT4 p1{ maxX, minY, b.z, cornerRadiusPx };
		DirectX::XMFLOAT4 p2{ maxX, maxY, b.z, cornerRadiusPx };
		DirectX::XMFLOAT4 p3{ minX, maxY, b.z, cornerRadiusPx };

		// Pack endpoints into Size
		DirectX::XMFLOAT4 packedAB{ a.x, a.y, b.x, b.y };

		// Pack thickness/Style/UIType into Texcoord
		DirectX::XMFLOAT3 tc{ thicknessPx, (float)style, 4.0f };

		auto push = [&](const DirectX::XMFLOAT4& pos, const DirectX::XMFLOAT3& tex)
			{
				sRenderer2DData->UIVertexBufferPtr->Position = pos;
				sRenderer2DData->UIVertexBufferPtr->Size = packedAB;     // A.xy B.zw
				sRenderer2DData->UIVertexBufferPtr->Color = color;
				sRenderer2DData->UIVertexBufferPtr->Texcoord = tex;          // thickness, style, UIType
				sRenderer2DData->UIVertexBufferPtr->EntityID = entityID;
				sRenderer2DData->UIVertexBufferPtr->TextureIndex = 0;
				sRenderer2DData->UIVertexBufferPtr->Params = { outlineWidthPx, outlineColor.x, outlineColor.y, outlineColor.z };
				sRenderer2DData->UIVertexBufferPtr++;
			};

		push(p0, tc);
		push(p1, tc);
		push(p2, tc);
		push(p3, tc);
	}

	void Renderer2D::SubmitButton(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT4& size, const DirectX::XMFLOAT4& color, const int entityID, const bool textured, uint32_t textureIndex, AssetHandle textureHandle, float borderWidth, const DirectX::XMFLOAT4& borderColor)
	{
		TOAST_PROFILE_FUNCTION();

		if (!HasRoomForQuad())
			return;

		DirectX::XMFLOAT4 insets, content;
		const bool hasMapping = textured && GetNineSliceData(textureHandle, insets, content);

		const DirectX::XMFLOAT4 params = hasMapping ? insets : DirectX::XMFLOAT4{ borderWidth, borderColor.x, borderColor.y, borderColor.z };
		const DirectX::XMFLOAT4 params2 = hasMapping ? content : DirectX::XMFLOAT4{ 0.0f, 0.0f, 0.0f, 0.0f };

		const DirectX::XMFLOAT4 sizeField = { size.x, size.y, size.z, hasMapping ? 1.0f : 0.0f };

		DirectX::XMFLOAT4 UIVertexPositions[4];

		float texturedF = textured == true ? 1.0f : 0.0f;

		DirectX::XMFLOAT3 textureCoords[] = { DirectX::XMFLOAT3(0.0f, 0.0f, 3.0f), DirectX::XMFLOAT3(1.0f, 0.0f, 3.0f), DirectX::XMFLOAT3(1.0f, 1.0f, 3.0f), DirectX::XMFLOAT3(0.0f, 1.0f, 3.0f) };

		UIVertexPositions[0] = { pos.x,          pos.y,          pos.z, texturedF };
		UIVertexPositions[1] = { pos.x + size.x, pos.y,          pos.z, texturedF };
		UIVertexPositions[2] = { pos.x + size.x, pos.y + size.y, pos.z, texturedF };
		UIVertexPositions[3] = { pos.x,          pos.y + size.y, pos.z, texturedF };

		for (size_t i = 0; i < 4; i++)
		{
			sRenderer2DData->UIVertexBufferPtr->Position = UIVertexPositions[i];
			sRenderer2DData->UIVertexBufferPtr->Size = sizeField;
			sRenderer2DData->UIVertexBufferPtr->Color = color;
			sRenderer2DData->UIVertexBufferPtr->Texcoord = textureCoords[i];
			sRenderer2DData->UIVertexBufferPtr->EntityID = entityID;
			sRenderer2DData->UIVertexBufferPtr->TextureIndex = textureIndex;
			sRenderer2DData->UIVertexBufferPtr->Params = params;
			sRenderer2DData->UIVertexBufferPtr->Params2 = params2;
			sRenderer2DData->UIVertexBufferPtr++;
		}
	}

	void Renderer2D::SubmitText(const TextSubmitParams& params, const std::string& textString)
	{
		TOAST_PROFILE_FUNCTION();

		auto& textFont = sRenderer2DData->TextFonts[params.FontTextureIndex];

		if (textString.empty())
			return;

		Ref<Texture2D> texAtlas = textFont->GetFontAtlas();
		TOAST_CORE_ASSERT(texAtlas, "");

		auto& fontGeometry = textFont->GetMSDFData()->FontGeometry;
		const auto& metrics = fontGeometry.getMetrics();

		// Calculate font scale based on desired size
		double fontHeight = metrics.ascenderY - metrics.descenderY;
		double fsScale = 1 / fontHeight;

		fsScale *= params.FontSize;

		const double maxWidth = params.WordWrap ? params.BoxSize.x : 0.0;

		std::vector<std::string> lines;
		BuildLines(textString, fontGeometry, fsScale, maxWidth, lines);

		const double lineAdvance = metrics.lineHeight * params.LineHeight * fsScale;

		double blockHeight = (double)lines.size() * lineAdvance;

		double blockY = params.Position.y;
		switch (params.AlignV)
		{
		case TextAlignV::Middle:
			blockY += (params.BoxSize.y - blockHeight) * 0.5;
			break;
		case TextAlignV::Bottom:
			blockY += params.BoxSize.y - blockHeight;
			break;
		}

		double y = blockY + fsScale * metrics.ascenderY;

		for (const auto& line : lines)
		{
			double x = params.Position.x;

			if (params.AlignH != TextAlignH::Left)
			{
				double lineWidth = MeasureLine(line, fontGeometry, fsScale);

				switch (params.AlignH)
				{
				case TextAlignH::Center:
					x += (params.BoxSize.x - lineWidth) * 0.5;
					break;
				case TextAlignH::Right:
					x += params.BoxSize.x - lineWidth;
					break;
				}
			}

			for (int i = 0; i < line.size(); i++)
			{
				if (!HasRoomForQuad())
					return;

				char32_t character = line[i];

				auto glyph = fontGeometry.getGlyph(character);
				if (!glyph)
					glyph = fontGeometry.getGlyph('?');
				if (!glyph)
					continue;

				double l, b, r, t;
				glyph->getQuadAtlasBounds(l, b, r, t);

				double pl, pb, pr, pt;
				glyph->getQuadPlaneBounds(pl, pb, pr, pt);

				//pl *= fsScale, pb *= fsScale, pr *= fsScale, pt *= fsScale;
				//pl += x, pb += y, pr += x, pt += y;

				pl *= fsScale; pr *= fsScale;
				pb *= fsScale; pt *= fsScale;

				// X is unchanged
				pl += x;
				pr += x;

				// Y: font plane is Y-up, UI is Y-down -> subtract offsets from baseline y
				double pbOld = pb;
				double ptOld = pt;

				// In Y-down space:
				// top    = y - ptOld
				// bottom = y - pbOld
				pb = y - ptOld;   // becomes "top Y"
				pt = y - pbOld;   // becomes "bottom Y"

				double texelWidth = 1. / texAtlas->GetWidth();
				double texelHeight = 1. / texAtlas->GetHeight();
				l *= texelWidth, b *= texelHeight, r *= texelWidth, t *= texelHeight;

				// Set vertex data
				// Top-Left
				sRenderer2DData->UIVertexBufferPtr->Position = { (float)pl, (float)pb, params.Position.z, 0.0f };
				sRenderer2DData->UIVertexBufferPtr->Size = { params.BoxSize.x, params.BoxSize.y, 1.0f, 1.0f };
				sRenderer2DData->UIVertexBufferPtr->Texcoord = { (float)l,  (float)t, 2.0f };
				sRenderer2DData->UIVertexBufferPtr->Color = params.Color;// Assuming text has a color
				sRenderer2DData->UIVertexBufferPtr->EntityID = params.EntityID;
				sRenderer2DData->UIVertexBufferPtr->TextureIndex = params.FontTextureIndex;
				sRenderer2DData->UIVertexBufferPtr++;

				// Top-Right
				sRenderer2DData->UIVertexBufferPtr->Position = { (float)pr, (float)pb, params.Position.z, 0.0f };
				sRenderer2DData->UIVertexBufferPtr->Size = { params.BoxSize.x, params.BoxSize.y, 1.0f, 1.0f };;
				sRenderer2DData->UIVertexBufferPtr->Texcoord = { (float)r,  (float)t, 2.0f };
				sRenderer2DData->UIVertexBufferPtr->Color = params.Color;
				sRenderer2DData->UIVertexBufferPtr->EntityID = params.EntityID;
				sRenderer2DData->UIVertexBufferPtr->TextureIndex = params.FontTextureIndex;
				sRenderer2DData->UIVertexBufferPtr++;

				// Bottom-Right
				sRenderer2DData->UIVertexBufferPtr->Position = { (float)pr, (float)pt, params.Position.z, 0.0f };
				sRenderer2DData->UIVertexBufferPtr->Size = { params.BoxSize.x, params.BoxSize.y, 1.0f, 1.0f };
				sRenderer2DData->UIVertexBufferPtr->Texcoord = { (float)r,  (float)b, 2.0f };
				sRenderer2DData->UIVertexBufferPtr->Color = params.Color;
				sRenderer2DData->UIVertexBufferPtr->EntityID = params.EntityID;
				sRenderer2DData->UIVertexBufferPtr->TextureIndex = params.FontTextureIndex;
				sRenderer2DData->UIVertexBufferPtr++;

				// Bottom-Left
				sRenderer2DData->UIVertexBufferPtr->Position = { (float)pl, (float)pt, params.Position.z, 0.0f };
				sRenderer2DData->UIVertexBufferPtr->Size = { params.BoxSize.x, params.BoxSize.y, 1.0f, 1.0f };
				sRenderer2DData->UIVertexBufferPtr->Texcoord = { (float)l,  (float)b, 2.0f };
				sRenderer2DData->UIVertexBufferPtr->Color = params.Color;
				sRenderer2DData->UIVertexBufferPtr->EntityID = params.EntityID;
				sRenderer2DData->UIVertexBufferPtr->TextureIndex = params.FontTextureIndex;
				sRenderer2DData->UIVertexBufferPtr++;

				double advance = glyph->getAdvance();
				char32_t nextChar = (i + 1 < (int)line.size()) ? (char32_t)line[i + 1] : 0;
				fontGeometry.getAdvance(advance, character, nextChar);
				x += fsScale * advance;
			}

			y += lineAdvance;
		}
	}

	void Renderer2D::SubmitImage(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT2& boxSize, const Ref<Texture2D>& texture, const DirectX::XMFLOAT4& sourceRect, const DirectX::XMFLOAT4& tint, float cornerRadius, ImageFit fit, bool flipX, bool flipY, int entityID)
	{
		TOAST_PROFILE_FUNCTION();

		if (!HasRoomForQuad() || !texture)
			return;

		const float textureAspect = (float)texture->GetWidth() / (float)texture->GetHeight();

		DirectX::XMFLOAT2 quadPos, quadSize;
		DirectX::XMFLOAT4 fitUV;
		ApplyImageFit(fit, { pos.x, pos.y }, boxSize, textureAspect, quadPos, quadSize, fitUV);

		DirectX::XMFLOAT4 uv;
		uv.x = fitUV.x + sourceRect.x * fitUV.z;
		uv.y = fitUV.y + sourceRect.y * fitUV.w;
		uv.z = sourceRect.z * fitUV.z;
		uv.w = sourceRect.w * fitUV.w;

		float u0 = uv.x;
		float v0 = uv.y;
		float u1 = uv.x + uv.z;
		float v1 = uv.y + uv.w;

		if (flipX)
			std::swap(u0, u1);
		if (flipY)
			std::swap(v0, v1);

		const uint32_t quadIndex = (uint32_t)(sRenderer2DData->UIVertexBufferPtr - sRenderer2DData->UIVertexBufferBase) / 4;

		if (sRenderer2DData->UIImageDraws.empty())
			sRenderer2DData->UIBatchQuadCount = quadIndex;

		sRenderer2DData->UIImageDraws.push_back({ texture, quadIndex * 6 });

		const DirectX::XMFLOAT4 sizeField = { quadSize.x, quadSize.y, cornerRadius, 0.0f };

		const DirectX::XMFLOAT4 positions[4] =
		{
			{ quadPos.x, quadPos.y, pos.z, 1.0f },
			{ quadPos.x + quadSize.x, quadPos.y, pos.z, 1.0f },
			{ quadPos.x + quadSize.x, quadPos.y + quadSize.y, pos.z, 1.0f },
			{ quadPos.x, quadPos.y + quadSize.y, pos.z, 1.0f },
		};

		// UI Type 5 = UI Image
		const DirectX::XMFLOAT3 texCoords[4] =
		{
			{ u0, v0, 5.0f },
			{ u1, v0, 5.0f },
			{ u1, v1, 5.0f },
			{ u0, v1, 5.0f },
		};

		for (size_t i = 0; i < 4; i++)
		{
			sRenderer2DData->UIVertexBufferPtr->Position = positions[i];
			sRenderer2DData->UIVertexBufferPtr->Size = sizeField;
			sRenderer2DData->UIVertexBufferPtr->Texcoord = texCoords[i];
			sRenderer2DData->UIVertexBufferPtr->Color = tint;
			sRenderer2DData->UIVertexBufferPtr->EntityID = entityID;
			sRenderer2DData->UIVertexBufferPtr->TextureIndex = 0;
			sRenderer2DData->UIVertexBufferPtr++;
		}
	}

	void Renderer2D::SubmitUIBounds(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT2& size, const DirectX::XMFLOAT4& color)
	{
		const DirectX::XMFLOAT3 topLeft = { pos.x,          pos.y,          1.0f };
		const DirectX::XMFLOAT3 topRight = { pos.x + size.x, pos.y,          1.0f };
		const DirectX::XMFLOAT3 bottomRight = { pos.x + size.x, pos.y + size.y, 1.0f };
		const DirectX::XMFLOAT3 bottomLeft = { pos.x,          pos.y + size.y, 1.0f };

		Renderer2D::SubmitConnector(topLeft, topRight, 1.0f, ConnectorStyle::Straight, 0.0f, color, 0.0f, { 0.0f, 0.0f, 0.0f, 0.0f }, -1);
		Renderer2D::SubmitConnector(topRight, bottomRight, 1.0f, ConnectorStyle::Straight, 0.0f, color, 0.0f, { 0.0f, 0.0f, 0.0f, 0.0f }, -1);
		Renderer2D::SubmitConnector(bottomRight, bottomLeft, 1.0f, ConnectorStyle::Straight, 0.0f, color, 0.0f, { 0.0f, 0.0f, 0.0f, 0.0f }, -1);
		Renderer2D::SubmitConnector(bottomLeft, topLeft, 1.0f, ConnectorStyle::Straight, 0.0f, color, 0.0f, { 0.0f, 0.0f, 0.0f, 0.0f }, -1);
	}

	void Renderer2D::LoadFontTextures()
	{
		// Path to your fonts folder – each font is in its own folder inside this directory.
		std::string fontsFolder = "../Toaster/assets/fonts";

		// Use recursive_directory_iterator to find font files in subdirectories.
		std::vector<std::string> fontPaths;
		for (auto it = std::filesystem::recursive_directory_iterator(fontsFolder);
			it != std::filesystem::recursive_directory_iterator(); ++it)
		{
			// If this is a directory and its name is "fontAwesome", skip recursing into it.
			if (it->is_directory() && it->path().filename() == "FontAwesome")
			{
				it.disable_recursion_pending(); // Do not traverse this folder.
				continue;
			}

			// Otherwise, if this is a regular file, add it to your list.
			if (it->is_regular_file())
			{
				// Check that the file extension is not ".txt".
				if (it->path().extension() != ".txt")
					fontPaths.push_back(it->path().string());
			}
		}

		// If no font files were found, exit early.
		if (fontPaths.empty())
			return;

		// Load each font using your Font class.
		std::vector<Ref<Font>> loadedFonts;
		for (const auto& path : fontPaths)
			loadedFonts.push_back(CreateRef<Font>(path));

		// Ensure we have loaded some fonts.
		if (loadedFonts.empty())
			return;

		// Collect valid texture atlases from the loaded fonts.
		std::vector<Texture2D*> fontAtlases;
		// Also, rebuild the fontPaths vector to contain only those that have a valid atlas.
		std::vector<std::string> validFontPaths;
		for (const auto& font : loadedFonts)
		{
			Texture2D* atlas = font->GetFontAtlas().get();
			if (!atlas)
			{
				TOAST_CORE_WARN("Renderer2D::LoadFontTextures: Font '%s' produced no atlas, skipping.", font->GetFilePath().c_str());
				continue;
			}

			// Check initial data and row pitch before adding.
			const void* data = atlas->GetInitialData();
			UINT rowPitch = atlas->GetRowPitch();
			if (data == nullptr || rowPitch == 0)
				continue;

			fontAtlases.push_back(atlas);
			validFontPaths.push_back(font->GetFilePath());

			sRenderer2DData->TextFonts.push_back(font);
		}

		if (fontAtlases.empty())
			return;

		// Assume all font atlases are created with the same dimensions and DXGI_FORMAT.
		uint32_t width = fontAtlases[0]->GetWidth();
		uint32_t height = fontAtlases[0]->GetHeight();
		uint32_t arraySize = static_cast<uint32_t>(fontAtlases.size());
		DXGI_FORMAT format = fontAtlases[0]->GetFormat();

		// Prepare vectors to hold initial texture data and row pitch for each font atlas.
		std::vector<const void*> initialData;
		std::vector<UINT> rowPitches;
		for (auto texture : fontAtlases)
		{
			initialData.push_back(texture->GetInitialData());
			rowPitches.push_back(texture->GetRowPitch());
		}

		// Create the Texture2DArray containing all font atlases.
		sRenderer2DData->FontsTextureArray = CreateRef<Texture2DArray>(
			format,
			width, height,
			arraySize,
			D3D11_USAGE_DEFAULT,
			D3D11_BIND_SHADER_RESOURCE,
			1, // samples
			0, // cpuAccessFlags
			initialData,
			rowPitches
		);

		// Optionally, map each array slice back to its originating file for later reference.
		sRenderer2DData->FontsTextureArray->SetSliceMappingOLD(validFontPaths);

		TOAST_CORE_CRITICAL("Loaded %d number of fonts", fontAtlases.size());
	}

	bool Renderer2D::HasRoomForQuad()
	{
		if (sRenderer2DData->UIVertexBufferPtr + 4 > sRenderer2DData->UIVertexBufferEnd)
		{
			if (!sRenderer2DData->UIBufferOverflowed)
			{
				sRenderer2DData->UIBufferOverflowed = true;
				TOAST_CORE_WARN("Renderer2D: UI vertex buffer full at %d elements, dropping the rest of this frame. Raise MaxUIElements.", sRenderer2DData->MaxUIElements);
			}

			return false;
		}

		return true;
	}

}