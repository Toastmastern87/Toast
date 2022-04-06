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

		inline void SetViewportSize(float width, float height) { mViewportWidth = width; mViewportHeight = height; UpdateProjection(); };

		void SetVerticalFOV(float verticalFOV) { mFOV = DirectX::XMConvertToRadians(verticalFOV); UpdateProjection(); }
		const float GetVerticalFOV() const { return DirectX::XMConvertToDegrees(mFOV); }
		void SetNearClip(float nearClip) { mNearClip = (std::max)(nearClip, 0.001f); UpdateProjection(); }
		const float& GetNearClip() const { return mNearClip; }
		void SetFarClip(float farClip) { mFarClip = farClip; UpdateProjection(); }
		const float& GetFarClip() const { return mFarClip; }

		const float GetAspecRatio() const { return mAspectRatio; }
		void SetAspectRatio(float aspectRatio) { mAspectRatio = aspectRatio; UpdateProjection(); }

		DirectX::XMVECTOR GetForwardDirection() const;
		DirectX::XMVECTOR GetUpDirection() const;
		DirectX::XMVECTOR GetRightDirection() const;
		DirectX::XMVECTOR GetPosition() const { return mPosition; }
		DirectX::XMVECTOR GetOrientation() const;
	private:
		void UpdateProjection();
		void UpdateView();

		bool OnMouseScroll(MouseScrolledEvent& e);

		void MousePan(const DirectX::XMVECTOR& delta);
		void MouseRotate(const DirectX::XMVECTOR& delta);
		void MouseZoom(float delta);

		DirectX::XMVECTOR CalculatePosition() const;

		std::pair<float, float> PanSpeed() const;
		float RotationSpeed() const;
		float ZoomSpeed() const;
	private:
		DirectX::XMVECTOR mFocalPoint = { 0.0f, 0.0f, 0.0f };
		DirectX::XMVECTOR mPosition = { 0.0f, 0.0f, 0.0f };
		
		DirectX::XMVECTOR mInitialCursorPosition = { 0.0f, 0.0f };

		float mDistance = 10.0f;
		float mPitch = 0.28f, mYaw = 0.0f;

		float mFOV = DirectX::XMConvertToRadians(45.0f), mAspectRatio = 1.778f, mNearClip = 0.1f, mFarClip = 1000.0f;

		float mViewportWidth = 1280, mViewportHeight = 720;

		float mRotationSpeed = 0.003f;
		float mTranslationSpeed = 1.0f;
	};
}