#pragma once

#include "Toast/Renderer/EditorCamera.h"
#include "Toast/Renderer/Renderer.h"

#include "Toast/Scene/SceneCamera.h"

namespace Toast {

	class RendererDebug : Renderer
	{
	private:
		struct LineVertex
		{
			DirectX::XMFLOAT3 Position;
			DirectX::XMFLOAT4 Color;
		};
	public:
		static void Init();
		static void Shutdown();

		static void BeginScene(const EditorCamera& camera);
		static void EndScene(const bool debugActivated);

		static void SubmitCameraFrustum(const SceneCamera& camera, DirectX::XMMATRIX& transform, DirectX::XMFLOAT3& pos);
		static void SubmitLine(DirectX::XMFLOAT3& p1, DirectX::XMFLOAT3& p2);
		static void SubmitLine(DirectX::XMVECTOR& p1, DirectX::XMVECTOR& p2);
		static void SubmitGrid(const EditorCamera& camera);
	private:
		static void DebugRenderPass();
		static void OutlineRenderPass();
	private:
		struct DebugData
		{
			Ref<Shader> DebugShader, GridShader, SelectedMeshMaskShader, OutlineShader;

			Ref<ShaderLayout::ShaderInputElement> LineShaderInputLayout;
			Ref<VertexBuffer> LineVertexBuffer;

			LineVertex* LineVertexBufferBase = nullptr;
			LineVertex* LineVertexBufferPtr = nullptr;

			uint32_t LineVertexCount = 0;
			uint32_t MaxVertices = 200;

			Ref<ConstantBuffer> mDebugCBuffer, mGridCBuffer;
			Buffer mDebugBuffer, mGridBuffer;
		};

		static Scope<DebugData> mDebugData;
	};
}