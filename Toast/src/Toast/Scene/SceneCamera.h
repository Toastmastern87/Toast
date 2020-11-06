#pragma once

#include "Toast/Renderer/Camera.h"

#include "Toast/Renderer/Mesh.h"

namespace Toast {

	class SceneCamera : public Camera 
	{
	public:
		enum class ProjectionType { Perspective = 0, Orthographic = 1 };
	public:
		SceneCamera();
		virtual ~SceneCamera() = default;

		void SetPerspective(float verticalFOV, float nearClip = 0.01f, float farClip = 1000.0f);
		void SetOrthographic(float size, float nearClip, float farClip); 

		void SetViewportSize(uint32_t width, uint32_t height);

		void SetPerspectiveVerticalFOV(float verticalFOV) {	mPerspectiveFOV = DirectX::XMConvertToRadians(verticalFOV); RecalculateProjection(); }
		const float GetPerspectiveVerticalFOV() const { return DirectX::XMConvertToDegrees(mPerspectiveFOV); }
		void SetPerspectiveNearClip(float nearClip) { mPerspectiveNear = std::max(nearClip, 0.001f); RecalculateProjection(); }
		const float GetPerspectiveNearClip() const { return mPerspectiveNear; }
		void SetPerspectiveFarClip(float farClip) {	mPerspectiveFar = farClip; RecalculateProjection();	}
		const float GetPerspectiveFarClip() const { return mPerspectiveFar; }

		void SetOrthographicSize(float size) { mOrthographicSize = std::max(size, 0.1f); RecalculateProjection(); }
		const float GetOrthographicSize() const { return mOrthographicSize; }
		void SetOrthographicNearClip(float nearClip) { mOrthographicNear = nearClip; RecalculateProjection(); }
		const float GetOrthographicNearClip() const { return mOrthographicNear; }
		void SetOrthographicFarClip(float farClip) { mOrthographicFar = farClip; RecalculateProjection(); }
		const float GetOrthographicFarClip() const { return mOrthographicFar; }

		ProjectionType GetProjectionType() const { return mProjectionType; }
		void SetProjectionType(ProjectionType type) { mProjectionType = type; RecalculateProjection();	}

		const float GetAspecRatio() const { return mAspectRatio; }
		void SetAspectRatio(float aspectRatio) { mAspectRatio = aspectRatio; RecalculateProjection(); }

		const Ref<Mesh> GetMesh() const { return mMesh; }
	private:
		void RecalculateProjection();
	private:
		ProjectionType mProjectionType = ProjectionType::Perspective;

		float mPerspectiveFOV = DirectX::XMConvertToRadians(45.0f);
		float mPerspectiveNear = 0.01f, mPerspectiveFar = 1000.0f;

		float mOrthographicSize = 10.0f;
		float mOrthographicNear = -1.0f, mOrthographicFar = 1.0f;

		float mAspectRatio = 1.0f;

		Ref<Mesh> mMesh;

		friend class Scene;
	};
}