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

		static void BeginScene(Camera& camera);
		static void EndScene(const bool debugActivated, const bool runtime, bool renderUI);

		static void SubmitCameraFrustum(Ref<Frustum> frustum);
		static void SubmitLine(DirectX::XMFLOAT3& p1, DirectX::XMFLOAT3& p2, DirectX::XMFLOAT4& color);
		static void SubmitLine(Vector3& p1, Vector3& p2, DirectX::XMFLOAT4& color);
		static void SubmitLine(DirectX::XMVECTOR& p1, DirectX::XMVECTOR& p2, DirectX::XMFLOAT4& color);
		static void SubmitGrid(EditorCamera& camera);
		static void SubmitCollider(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, bool wireframe = true);

	private:
		static void DebugRenderPass(const bool runtime);
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