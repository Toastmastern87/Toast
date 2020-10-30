#pragma once

#include "Toast/Renderer/Camera.h"
#include "Toast/Core/Timestep.h"

namespace Toast {

	class PerspectiveCamera : public Camera
	{
	public:
		PerspectiveCamera() = default;
		PerspectiveCamera(float verticalFOV, float nearClip = 0.01f, float farClip = 10000.0f);
		virtual ~PerspectiveCamera() = default;

		void SetViewportSize(uint32_t width, uint32_t height);

		void OnUpdate(Timestep ts);

		const DirectX::XMMATRIX GetViewMatrix() const { return mViewMatrix; }

		void SetVerticalFOV(float verticalFOV) { mFOV = DirectX::XMConvertToRadians(verticalFOV); RecalculateProjectionMatrix(); }
		const float GetVerticalFOV() const { return DirectX::XMConvertToDegrees(mFOV); }
		void SetNearClip(float nearClip) { mNear = std::max(nearClip, 0.001f); RecalculateProjectionMatrix(); }
		const float GetNearClip() const { return mNear; }
		void SetFarClip(float farClip) { mFar = farClip; RecalculateProjectionMatrix(); }
		const float GetFarClip() const { return mFar; }

		const float GetAspecRatio() const { return mAspectRatio; }
		void SetAspectRatio(float aspectRatio) { mAspectRatio = aspectRatio; RecalculateProjectionMatrix(); }
	private:
		void RecalculateProjectionMatrix();
		void RecalculateViewMatrix();
	private:
		DirectX::XMMATRIX mViewMatrix;
		DirectX::XMVECTOR mForward, mUp, mRight, mPosition;
		
		float mPitch = 0.0f, mYaw = 0.0f;

		DirectX::XMFLOAT2 mCursorPos;

		float mFOV = DirectX::XMConvertToRadians(45.0f);
		float mNear = 0.01f, mFar = 10000.0f;

		float mAspectRatio = 1.0f;

		float mRotationSpeed = 3.0f;
		float mTranslationSpeed = 4.0f;
	};
}