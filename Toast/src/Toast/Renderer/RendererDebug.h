#pragma once

#include "Toast/Renderer/EditorCamera.h"
#include "Toast/Renderer/Renderer.h"

#include "Toast/Scene/SceneCamera.h"

namespace Toast {

	class RendererDebug : Renderer
	{
	public:
		static void Init(uint32_t width, uint32_t height);
		static void Shutdown();

		static void OnWindowResize(uint32_t width, uint32_t height);

		static void BeginScene(Camera& camera);
		static void EndScene(const bool debugActivated, const bool runtime, bool renderUI, const bool renderGrid);

		static void SubmitCameraFrustum(Ref<Frustum> frustum);
		static void SubmitLine(DirectX::XMFLOAT3& p1, DirectX::XMFLOAT3& p2, DirectX::XMFLOAT3& color);
		static void SubmitLine(Vector3& p1, Vector3& p2, DirectX::XMFLOAT3& color);
		static void SubmitLine(DirectX::XMVECTOR& p1, DirectX::XMVECTOR& p2, DirectX::XMFLOAT3& color);
		static void SubmitMesh(const Ref<Mesh> mesh, const DirectX::XMMATRIX& transform, bool wireframe = true);

	private:
		static void DebugRenderPass(const bool runtime, const bool renderGrid);
		static void OutlineRenderPass();
	private:
		struct DebugData
		{
			Ref<Shader> DebugShader, GridShader, ObjectMaskShader, OutlineShader;

			Ref<ShaderLayout::ShaderInputElement> LineShaderInputLayout;
			Ref<VertexBuffer> LineVertexBuffer;

			Vertex* LineVertexBufferBase = nullptr;
			Vertex* LineVertexBufferPtr = nullptr;

			uint32_t LineVertexCount = 0;
			uint32_t MaxVertices = 200;

			Ref<ConstantBuffer> mDebugCBuffer;
			Buffer mDebugBuffer;

			Ref<RenderTarget> SelectedMeshMaskRT;
		};

		static Scope<DebugData> mDebugData;
	};
}