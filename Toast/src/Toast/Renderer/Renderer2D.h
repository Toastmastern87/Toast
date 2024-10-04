#pragma once

#include "Toast/Renderer/OrthographicCamera.h"

#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/Camera.h"

#include "Toast/Renderer/Texture.h"

namespace Toast {

	class Renderer2D : Renderer
	{
	private:
		enum ElementType 
		{
			Panel = 0,
			Text = 1,
			Button = 2
		};

		struct DrawCommand
		{
		public:
			DrawCommand(const Ref<UIElement> element, const DirectX::XMFLOAT2& position, const DirectX::XMFLOAT2& size, ElementType type, const int entityID = 0, const bool targetable = false)
				: Element(element), Position(position), Size(size), Type(type), EntityID(entityID), Targetable(targetable) {}
		public:
			Ref<UIElement> Element;
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
		};

	protected:
		static Scope<Renderer2DData> sRenderer2DData;

	public:
		static void Init();
		static void Shutdown();

		static void BeginScene(Camera& camera);
		static void EndScene();

		static void ClearDrawList();

		static void SubmitPanel(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& size, DirectX::XMFLOAT4& color, const int entityID, const bool targetable);
		static void SubmitButton(const DirectX::XMFLOAT2& pos, const DirectX::XMFLOAT2& size, const Ref<UIButton>& button, const int entityID, const bool targetable);
		static void SubmitText(const DirectX::XMFLOAT2& pos, const DirectX::XMFLOAT2& size, const Ref<UIText>& text, const int entityID, const bool targetable);
	private:
	};
}