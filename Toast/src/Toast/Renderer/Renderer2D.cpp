#include "tpch.h"
#include "Toast/Renderer/Renderer2D.h"

#include "Toast/Renderer/Shader.h"
#include "Toast/Renderer/RendererBuffer.h"
#include "Toast/Renderer/RenderCommand.h"
#include "Toast/Renderer/UI/Font.h"
#include "Toast/Renderer/UI/MSDFData.h"

namespace Toast {

	Scope<Renderer2D::Renderer2DData> Renderer2D::sRenderer2DData = CreateScope<Renderer2D::Renderer2DData>();

	void Renderer2D::Init()
	{
		TOAST_PROFILE_FUNCTION();

		sRenderer2DData->UIVertexBufferBase = new UIVertex[sRenderer2DData->MaxUIVertices];
		sRenderer2DData->UIVertexBufferPtr = sRenderer2DData->UIVertexBufferBase;
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

		// TODO Fix to use Asset handler in the future
		std::string UITextureFolder = "../Toaster/assets/textures/UI";
		std::vector<std::string> texturePaths;

		// Collect all file paths in the folder
		for (const auto& entry : std::filesystem::directory_iterator(UITextureFolder))
		{
			if (entry.is_regular_file())
				texturePaths.push_back(entry.path().string());
		}

		if (texturePaths.empty())
			return;

		std::vector<Texture2D*> loadedTextures;
		for (const auto& path : texturePaths)
		{
			Texture2D* texture = TextureLibrary::LoadTexture2D(path);
		
			loadedTextures.push_back(texture);
		}

		uint32_t width = loadedTextures[0]->GetWidth();
		uint32_t height = loadedTextures[0]->GetHeight();
		uint32_t arraySize = static_cast<uint32_t>(loadedTextures.size());
		DXGI_FORMAT format = loadedTextures[0]->GetFormat();

		std::vector<const void*> initialData;
		std::vector<UINT> rowPitches;
		for (auto& texture : loadedTextures)
		{
			const void* data = texture->GetInitialData(); 
			UINT rowPitch = texture->GetRowPitch(); 
			initialData.push_back(data);
			rowPitches.push_back(rowPitch);
		}

		sRenderer2DData->UITextureArray = CreateRef<Texture2DArray>(format,	width, height, arraySize, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 1, 0, initialData, rowPitches);

		sRenderer2DData->UITextureArray->SetSliceMapping(texturePaths);

		LoadFontTextures();
	}

	void Renderer2D::Shutdown()
	{
		TOAST_PROFILE_FUNCTION();
	}

	void Renderer2D::BeginScene(Camera& camera)
	{
		TOAST_PROFILE_FUNCTION();

		RenderCommand::SetPrimitiveTopology(PrimitiveTopology::TRIANGLELIST);

		sRendererData->CameraBuffer.Write((uint8_t*)&camera.GetViewMatrix(), 64, 64);
		sRendererData->CameraBuffer.Write((uint8_t*)&camera.GetOrthoProjection(), 64, 128);
		sRendererData->CameraCBuffer->Map(sRendererData->CameraBuffer);

		sRenderer2DData->UIVertexBufferPtr = sRenderer2DData->UIVertexBufferBase;
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
		uint32_t indexCount = quadCount * 6;

		ShaderLibrary::Get("assets/shaders/UI.hlsl")->Bind();

		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 6, sRenderer2DData->FontsTextureArray->GetSRV());
		RenderCommand::SetShaderResource(D3D11_PIXEL_SHADER, 8, sRenderer2DData->UITextureArray->GetSRV());

		sRenderer2DData->UIVertexBuffer->SetData(sRenderer2DData->UIVertexBufferBase, vertexDataSize);
		sRenderer2DData->UIVertexBuffer->Bind();
		sRenderer2DData->UIIndexBuffer->Bind();

		RenderCommand::DrawIndexed(0, 0, indexCount);
		
		RenderCommand::SetRenderTargets({ sRendererData->BackbufferRT->GetRTV().Get() }, nullptr);
		RenderCommand::ClearRenderTargets(sRendererData->BackbufferRT->GetRTV().Get(), { 0.0f, 0.0f, 0.0f, 1.0f });

		RenderCommand::ClearShaderResources();
		RenderCommand::SetViewport(sRendererData->EditorViewport);

#ifdef TOAST_DEBUG
		if (annotation)
			annotation->EndEvent();
#endif
	}

	void Renderer2D::SubmitPanel(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT4& size, DirectX::XMFLOAT4& color, const int entityID, const bool textured, const bool targetable, uint32_t textureIndex)
	{
		TOAST_PROFILE_FUNCTION();

		DirectX::XMFLOAT4 UIVertexPositions[4];

		float texturedF = textured == true ? 1.0f : 0.0f;

		DirectX::XMFLOAT3 textureCoords[] = { DirectX::XMFLOAT3(0.0f, 1.0f, 1.0f), DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f), DirectX::XMFLOAT3(1.0f, 0.0f, 1.0f), DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f) };

		UIVertexPositions[0] = DirectX::XMFLOAT4(pos.x, pos.y, pos.z, texturedF);
		UIVertexPositions[1] = DirectX::XMFLOAT4(pos.x + size.x, pos.y, pos.z, texturedF);
		UIVertexPositions[2] = DirectX::XMFLOAT4(pos.x + size.x, pos.y + size.y, pos.z, texturedF);
		UIVertexPositions[3] = DirectX::XMFLOAT4(pos.x, pos.y + size.y, pos.z, texturedF);

		for (size_t i = 0; i < 4; i++)
		{
			sRenderer2DData->UIVertexBufferPtr->Position = UIVertexPositions[i];
			sRenderer2DData->UIVertexBufferPtr->Size = size;
			sRenderer2DData->UIVertexBufferPtr->Color = color;
			sRenderer2DData->UIVertexBufferPtr->Texcoord = textureCoords[i];
			sRenderer2DData->UIVertexBufferPtr->EntityID = entityID;
			sRenderer2DData->UIVertexBufferPtr->TextureIndex = textureIndex;
			sRenderer2DData->UIVertexBufferPtr++;
		}
	}

	void Renderer2D::SubmitConnector(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& size, const float borderRadius, const DirectX::XMFLOAT3& parentPos, const float connectorThickness)
	{
		DirectX::XMFLOAT4 UIVertexPositions[4];

		UIVertexPositions[0] = DirectX::XMFLOAT4(parentPos.x, parentPos.y, parentPos.z, 0.0f);
		UIVertexPositions[1] = DirectX::XMFLOAT4(pos.x, pos.y + size.y - connectorThickness, parentPos.z, 0.0f);
		UIVertexPositions[2] = DirectX::XMFLOAT4(pos.x, pos.y + size.y, parentPos.z, 0.0f);
		UIVertexPositions[3] = DirectX::XMFLOAT4(parentPos.x, parentPos.y - connectorThickness, 1.0f, 0.0f);

		for (size_t i = 0; i < 4; i++)
		{
			sRenderer2DData->UIVertexBufferPtr->Position = UIVertexPositions[i];
			sRenderer2DData->UIVertexBufferPtr->Size = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
			sRenderer2DData->UIVertexBufferPtr->Color = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
			sRenderer2DData->UIVertexBufferPtr->Texcoord = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
			sRenderer2DData->UIVertexBufferPtr->EntityID = 0;
			sRenderer2DData->UIVertexBufferPtr->TextureIndex = 1;
			sRenderer2DData->UIVertexBufferPtr++;
		}
	}

	void Renderer2D::SubmitButton(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT4& size, DirectX::XMFLOAT4& color, DirectX::XMFLOAT4& clickColor, const int entityID, const bool textured, const bool clicked, uint32_t textureIndex, uint32_t clickTextureIndex)
	{
		TOAST_PROFILE_FUNCTION();

		DirectX::XMFLOAT4 UIVertexPositions[4];

		float texturedF = textured == true ? 1.0f : 0.0f;

		uint32_t finalTexIndex = clicked == true ? clickTextureIndex : textureIndex;

		DirectX::XMFLOAT4 finalColor = clicked == true ? clickColor : color;

		constexpr DirectX::XMFLOAT3 textureCoords[] = { DirectX::XMFLOAT3(0.0f, 1.0f, 3.0f), DirectX::XMFLOAT3(1.0f, 1.0f, 3.0f), DirectX::XMFLOAT3(1.0f, 0.0f, 3.0f), DirectX::XMFLOAT3(0.0f, 0.0f, 3.0f) };

		UIVertexPositions[0] = DirectX::XMFLOAT4(pos.x, pos.y, pos.z, texturedF);
		UIVertexPositions[1] = DirectX::XMFLOAT4(pos.x + size.x, pos.y, pos.z, texturedF);
		UIVertexPositions[2] = DirectX::XMFLOAT4(pos.x + size.x, pos.y + size.y, pos.z, texturedF);
		UIVertexPositions[3] = DirectX::XMFLOAT4(pos.x, pos.y + size.y, pos.z, texturedF);

		for (size_t i = 0; i < 4; i++)
		{
			sRenderer2DData->UIVertexBufferPtr->Position = UIVertexPositions[i];
			sRenderer2DData->UIVertexBufferPtr->Size = size;
			sRenderer2DData->UIVertexBufferPtr->Color = finalColor;
			sRenderer2DData->UIVertexBufferPtr->Texcoord = textureCoords[i];
			sRenderer2DData->UIVertexBufferPtr->EntityID = entityID;
			sRenderer2DData->UIVertexBufferPtr->TextureIndex = finalTexIndex;
			sRenderer2DData->UIVertexBufferPtr++;
		}
	}

	void Renderer2D::SubmitText(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT4& size, DirectX::XMFLOAT4& color, const std::string& textString, const uint32_t fontTextureIndex, const int entityID, const bool targetable)
	{
		TOAST_PROFILE_FUNCTION();

		auto& textFont = sRenderer2DData->TextFonts[fontTextureIndex];

		if (textString.empty())
			return;

		Ref<Texture2D> texAtlas = textFont->GetFontAtlas();
		TOAST_CORE_ASSERT(texAtlas, "");

		auto& fontGeometry = textFont->GetMSDFData()->FontGeometry;
		const auto& metrics = fontGeometry.getMetrics();

		// Calculate font scale based on desired size
		double fontHeight = metrics.ascenderY - metrics.descenderY;
		double fsScale = 1 / fontHeight;

		fsScale *= size.y;

		// Initialize positions
		double x = pos.x;
		double y = pos.y + fsScale * (-metrics.descenderY); // Adjust y to align baseline

		for (int i = 0; i < textString.size(); i++)
		{
			char32_t character = textString[i];
			// New row
			if (character == '\n')
			{
				x = pos.x;
				y -= fsScale * metrics.lineHeight;
				continue;
			}

			auto glyph = fontGeometry.getGlyph(character);
			if (!glyph)
				glyph = fontGeometry.getGlyph('?');
			if (!glyph)
				continue;

			double l, b, r, t;
			glyph->getQuadAtlasBounds(l, b, r, t);

			double pl, pb, pr, pt;
			glyph->getQuadPlaneBounds(pl, pb, pr, pt);

			pl *= fsScale, pb *= fsScale, pr *= fsScale, pt *= fsScale;
			pl += x, pb += y, pr += x, pt += y;

			double texelWidth = 1. / texAtlas->GetWidth();
			double texelHeight = 1. / texAtlas->GetHeight();
			l *= texelWidth, b *= texelHeight, r *= texelWidth, t *= texelHeight;

			// Set vertex data
			sRenderer2DData->UIVertexBufferPtr->Position = { (float)pl, (float)pb, pos.z, 0.0f }; // Bottom-Left
			sRenderer2DData->UIVertexBufferPtr->Size = size;
			sRenderer2DData->UIVertexBufferPtr->Texcoord = { (float)l, (float)b, 2.0f };
			sRenderer2DData->UIVertexBufferPtr->Color = color;// Assuming text has a color
			sRenderer2DData->UIVertexBufferPtr->EntityID = entityID;
			sRenderer2DData->UIVertexBufferPtr->TextureIndex = fontTextureIndex;
			sRenderer2DData->UIVertexBufferPtr++;

			sRenderer2DData->UIVertexBufferPtr->Position = { (float)pr, (float)pb, pos.z, 0.0f }; // Bottom-Right
			sRenderer2DData->UIVertexBufferPtr->Size = size;
			sRenderer2DData->UIVertexBufferPtr->Texcoord = { (float)r, (float)b, 2.0f };
			sRenderer2DData->UIVertexBufferPtr->Color = color;
			sRenderer2DData->UIVertexBufferPtr->EntityID = entityID;
			sRenderer2DData->UIVertexBufferPtr->TextureIndex = fontTextureIndex;
			sRenderer2DData->UIVertexBufferPtr++;

			sRenderer2DData->UIVertexBufferPtr->Position = { (float)pr, (float)pt, pos.z, 0.0f }; // Top-Right
			sRenderer2DData->UIVertexBufferPtr->Size = size;
			sRenderer2DData->UIVertexBufferPtr->Texcoord = { (float)r, (float)t, 2.0f };
			sRenderer2DData->UIVertexBufferPtr->Color = color;
			sRenderer2DData->UIVertexBufferPtr->EntityID = entityID;
			sRenderer2DData->UIVertexBufferPtr->TextureIndex = fontTextureIndex;
			sRenderer2DData->UIVertexBufferPtr++;

			sRenderer2DData->UIVertexBufferPtr->Position = { (float)pl, (float)pt, pos.z, 0.0f }; // Top-Left
			sRenderer2DData->UIVertexBufferPtr->Size = size;
			sRenderer2DData->UIVertexBufferPtr->Texcoord = { (float)l, (float)t, 2.0f };
			sRenderer2DData->UIVertexBufferPtr->Color = color;
			sRenderer2DData->UIVertexBufferPtr->EntityID = entityID;
			sRenderer2DData->UIVertexBufferPtr->TextureIndex = fontTextureIndex;
			sRenderer2DData->UIVertexBufferPtr++;

			double advance = glyph->getAdvance();
			fontGeometry.getAdvance(advance, character, textString[i + 1]);
			x += fsScale * advance;
		 }
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
				continue;

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
		sRenderer2DData->FontsTextureArray->SetSliceMapping(fontPaths);

		TOAST_CORE_CRITICAL("Loaded %d number of fonts", fontAtlases.size());
	}

}