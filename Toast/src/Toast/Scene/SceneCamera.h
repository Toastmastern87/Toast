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

		void SetPerspective(float verticalFOV, float nearClip = 0.01f, float farClip = 10.0f);
		void SetOrthographic(float width, float height, float nearClip, float farClip); 

		void SetViewportSize(uint32_t width, uint32_t height);

		void SetPerspectiveVerticalFOV(float verticalFOV) {	mPerspectiveFOV = DirectX::XMConvertToRadians(verticalFOV); RecalculateProjection(); }
		const float GetPerspectiveVerticalFOV() const { return DirectX::XMConvertToDegrees(mPerspectiveFOV); }
		void SetNearClip(float nearClip) { mNearClip = (std::max)(nearClip, 0.001f); RecalculateProjection(); }
		float& GetNearClip() { return mNearClip; }
		void SetFarClip(float farClip) { mFarClip = farClip; RecalculateProjection();	}
		float& GetFarClip() { return mFarClip; }

		void SetOrthographicSize(float width, float height) { mOrthographicWidth = (std::max)(width, 0.1f); mOrthographicHeight = (std::max)(height, 0.1f);  RecalculateProjection(); }
		const float GetOrthographicWidth() const { return mOrthographicWidth; }
		const float GetOrthographicHeight() const { return mOrthographicHeight; }

		ProjectionType GetProjectionType() const { return mProjectionType; }
		void SetProjectionType(ProjectionType type) { mProjectionType = type; RecalculateProjection();	}

		const float GetAspecRatio() const { return mAspectRatio; }
		void SetAspectRatio(float aspectRatio) { mAspectRatio = aspectRatio; RecalculateProjection(); }

		DirectX::XMFLOAT4& GetForwardDirection() override;
	private:
		void RecalculateProjection();
	private:
		ProjectionType mProjectionType = ProjectionType::Perspective;

		float mPerspectiveFOV = DirectX::XMConvertToRadians(45.0f);

		float mOrthographicWidth = 10.0f;
		float mOrthographicHeight = 10.0f;

		float mAspectRatio = 1.0f;

		friend class Scene;
	};
}