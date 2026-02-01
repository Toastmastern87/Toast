#pragma once

#include "Toast/Renderer/OrthographicCamera.h"

#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/Camera.h"

#include "Toast/Renderer/Texture.h"

namespace Toast {

	class Renderer2D : Renderer
	{
	private:
		struct UIVertex
		{
			DirectX::XMFLOAT4 Position; // z component holds what type of UI Element this is, w component holds if the element is textured or not
			DirectX::XMFLOAT4 Size; // z component holds the corner radius for a panel, w component holds the size of the border
			DirectX::XMFLOAT4 Color;
			DirectX::XMFLOAT3 Texcoord;
			uint32_t EntityID;
			uint32_t TextureIndex;

			UIVertex() = default;

			UIVertex(DirectX::XMFLOAT4 pos, DirectX::XMFLOAT4 size, DirectX::XMFLOAT4 color, DirectX::XMFLOAT3 uv, uint32_t id, uint32_t texIdx)
			{
				Position = pos;
				Size = size;
				Color = color;
				Texcoord = uv;
				TextureIndex = texIdx;
			}
		};

		enum ElementType 
		{
			Panel = 0,
			Text = 1,
			Button = 2
		};

		struct DrawCommand
		{
		public:
			DrawCommand(const DirectX::XMFLOAT2& position, const DirectX::XMFLOAT2& size, ElementType type, const int entityID = 0, const bool targetable = false)
				: Position(position), Size(size), Type(type), EntityID(entityID), Targetable(targetable) {}
		public:
			DirectX::XMFLOAT2 Position;
			DirectX::XMFLOAT2 Size;
			ElementType Type;
			const int EntityID;
			const bool Targetable;
		};

		struct Renderer2DData
		{
			std::vector<DrawCommand> ElementDrawList;
			std::string shaderNameBound = "";
			bool UIBuffersBound = false;
			bool UITextBuffersBound = false;

			const uint32_t MaxUIElements = 256;
			const uint32_t MaxUIVertices = MaxUIElements * 4;
			const uint32_t MaxUIIndices = MaxUIElements * 6;
			UIVertex* UIVertexBufferBase = nullptr;
			UIVertex* UIVertexBufferPtr = nullptr;
			Ref<VertexBuffer> UIVertexBuffer;
			Ref<IndexBuffer> UIIndexBuffer;

			std::vector<Ref<Font>> TextFonts;

			Ref<Texture2DArray> UITextureArray;
			Ref<Texture2DArray> FontsTextureArray;
		};

	protected:
		static Scope<Renderer2DData> sRenderer2DData;
	public:
		static void Init();
		static void Shutdown();

		static void BeginScene(Camera& camera);
		static void EndScene();

		static void SubmitPanel(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT4& size, DirectX::XMFLOAT4& color, const int entityID, const bool textured, const bool targetable, uint32_t textureIndex);
		static void SubmitConnector(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, float thicknessPx, float aaPx, const DirectX::XMFLOAT4& color, int entityID);
		static void SubmitButton(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT4& size, DirectX::XMFLOAT4& color, DirectX::XMFLOAT4& clickColor, const int entityID, const bool textured, const bool clicked, uint32_t textureIndex, uint32_t clickTextureIndex);
		static void SubmitText(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT4& size, DirectX::XMFLOAT4& color, const std::string& textString, const uint32_t fontTextureIndex, const int entityID, const bool targetable);

		static Renderer2DData* GetRendererData() { return sRenderer2DData.get(); }
	private:
		static void LoadFontTextures();
	};
}