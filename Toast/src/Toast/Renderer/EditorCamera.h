#pragma once

#include "Toast/Renderer/Camera.h"

#include "Toast/Core/Timestep.h"

#include "Toast/Events/Event.h"
#include "Toast/Events/MouseEvent.h"

namespace Toast {

	class EditorCamera : public Camera
	{
	public:
		EditorCamera() = default;
		EditorCamera(float fov, float aspectRatio, float nearClip, float farClip);
		virtual ~EditorCamera() = default;

		void OnUpdate(Timestep ts);
		void OnEvent(Event& e);

		void SetViewportSize(float width, float height) { mViewportWidth = width; mViewportHeight = height; UpdateProjection(); };

		void SetVerticalFOV(float verticalFOV) { mFOV = DirectX::XMConvertToRadians(verticalFOV); UpdateProjection(); }
		const float GetVerticalFOV() const { return DirectX::XMConvertToDegrees(mFOV); }
		void SetNearClip(float nearClip) { mNearClip = (std::max)(nearClip, 0.001f); UpdateProjection(); }
		float& GetNearClip() { return mNearClip; }
		void SetFarClip(float farClip) { mFarClip = farClip; UpdateProjection(); }
		float& GetFarClip() { return mFarClip; }

		const float GetAspecRatio() const { return mAspectRatio; }
		void SetAspectRatio(float aspectRatio) { mAspectRatio = aspectRatio; UpdateProjection(); }

		DirectX::XMFLOAT4& GetForwardDirection() override;
		DirectX::XMVECTOR GetUpDirection() const;
		DirectX::XMVECTOR GetRightDirection() const;
		DirectX::XMVECTOR GetPosition() const { return mPosition; }
		DirectX::XMVECTOR GetOrientation() const;

		void UpdateProjection();
		void UpdateView();

		void UpdateFocalPoint(DirectX::XMVECTOR& newFocalPoint);
	private:
		bool OnMouseScroll(MouseScrolledEvent& e);

		void MousePan(const DirectX::XMVECTOR& delta);
		void MouseRotate(const DirectX::XMVECTOR& delta);
		void MouseZoom(float delta);

		std::pair<float, float> PanSpeed() const;
		float RotationSpeed() const;
		float ZoomSpeed() const;
	private:
		DirectX::XMVECTOR mFocalPoint = { 0.0f, 1.0f, 7.0f };
		DirectX::XMVECTOR mPosition = { 0.0f, 1.0f, -3.0f };
		// For planet testing
		//DirectX::XMVECTOR mFocalPoint = { 0.0f, 0.0f, 1.0f };
		//DirectX::XMVECTOR mPosition = { 0.0f, 0.0f, -15000.0f };
		
		DirectX::XMVECTOR mInitialCursorPosition = { 0.0f, 0.0f };

		/*float mPitch = 0.28f, mYaw = 0.0f;*/
		float mPitch = 0.0f, mYaw = 0.0f;

		float mFOV = DirectX::XMConvertToRadians(45.0f), mAspectRatio = 1.778f;

		float mViewportWidth = 1280, mViewportHeight = 720;

		float mRotationSpeed = 0.003f;
		float mTranslationSpeed = 1.0f;
	};
}