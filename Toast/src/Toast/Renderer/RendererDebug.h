#pragma once

#include "Toast/Renderer/EditorCamera.h"
#include "Toast/Scene/SceneCamera.h"

namespace Toast {

	class RendererDebug
	{
	public:
		static void Init();
		static void Shutdown();

		static void BeginScene(const EditorCamera& camera);
		static void EndScene();
		static void Flush();

		static void SubmitCameraFrustum(const SceneCamera& camera, DirectX::XMMATRIX& transform, DirectX::XMFLOAT3& pos);
		static void SubmitLine(DirectX::XMFLOAT3& p1, DirectX::XMFLOAT3& p2);
		static void SubmitLine(DirectX::XMVECTOR& p1, DirectX::XMVECTOR& p2);
		static void DrawGrid(const EditorCamera& camera);
	private:
		static void StartBatch();
		static void NextBatch();
	private:
		struct SceneData
		{
			Ref<ConstantBuffer> mDebugCBuffer, mGridCBuffer;
			Buffer mDebugBuffer, mGridBuffer;
		};

		static Scope<SceneData> mSceneData;
	};
}