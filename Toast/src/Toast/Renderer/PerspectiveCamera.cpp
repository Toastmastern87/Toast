#include "tpch.h"
#include "PerspectiveCamera.h"

#include "Toast/Core/Input.h"

namespace Toast {

	PerspectiveCamera::PerspectiveCamera(float verticalFOV, float nearClip, float farClip)
	{
		mCursorPos = Input::GetMousePosition();

		mFOV = verticalFOV;
		mNear = nearClip;
		mFar = farClip;

		mForward = { 0.0f, 0.0f, -1.0f };
		mUp = { 0.0f, 1.0f, 0.0f };
		mRight = { 1.0f, 0.0f, 0.0f };

		mPosition = { 0.0f, 1.0f, -5.0f };

		RecalculateProjectionMatrix();
		RecalculateViewMatrix();
	}

	void PerspectiveCamera::SetViewportSize(uint32_t width, uint32_t height)
	{
		mAspectRatio = (float)width / (float)height;
		RecalculateProjectionMatrix();
	}

	void PerspectiveCamera::OnUpdate(Timestep ts)
	{
		// Rotation
		{
			mForward = { 0.0f, 0.0f, 1.0f };
			mUp = { 0.0f, 1.0f, 0.0f };
			mRight = { 1.0f, 0.0f, 0.0f };

			DirectX::XMFLOAT2 newCursorPos = Input::GetMousePosition();

			if (Input::IsMouseButtonPressed(Mouse::ButtonRight))
			{
				if (mCursorPos.x != newCursorPos.x)
					mYaw += (newCursorPos.x - mCursorPos.x) * mRotationSpeed;
				if (mCursorPos.y != newCursorPos.y)
					mPitch += (newCursorPos.y - mCursorPos.y) * mRotationSpeed;

				mPitch = std::min(mPitch, DirectX::XM_PIDIV2);
				mPitch = std::max(mPitch, -DirectX::XM_PIDIV2);

				if (mYaw >= DirectX::XM_2PI)
					mYaw -= DirectX::XM_2PI;
				if (mYaw <= -DirectX::XM_2PI)
					mYaw += DirectX::XM_2PI;
			}

			DirectX::XMVECTOR quad = DirectX::XMQuaternionRotationRollPitchYaw(mPitch, mYaw, 0.0f);

			mForward = DirectX::XMVector3Rotate(mForward, quad);
			mUp = DirectX::XMVector3Rotate(mUp, quad);
			mRight = DirectX::XMVector3Rotate(mRight, quad);

			mCursorPos = newCursorPos;
		}

		// Translation
		{
			DirectX::XMVECTOR distanceVector = DirectX::XMVector3Length(mPosition);
			float distance = DirectX::XMVectorGetX(distanceVector);

			if (Input::IsKeyPressed(Key::W))
				mPosition = DirectX::XMVectorAdd(mPosition, DirectX::XMVectorScale(mForward, (mTranslationSpeed * ts * distance)));
			if (Input::IsKeyPressed(Key::S))
				mPosition = DirectX::XMVectorAdd(mPosition, DirectX::XMVectorScale(mForward, -(mTranslationSpeed * ts * distance)));
			if (Input::IsKeyPressed(Key::A))
				mPosition = DirectX::XMVectorAdd(mPosition, DirectX::XMVectorScale(mRight, -(mTranslationSpeed * ts * distance)));
			if (Input::IsKeyPressed(Key::D))
				mPosition = DirectX::XMVectorAdd(mPosition, DirectX::XMVectorScale(mRight, (mTranslationSpeed * ts * distance)));
			if (Input::IsKeyPressed(Key::E))
				mPosition = DirectX::XMVectorAdd(mPosition, DirectX::XMVectorScale(DirectX::XMVECTOR({0.0f, 1.0f, 0.0f}), (mTranslationSpeed * ts * distance)));
			if (Input::IsKeyPressed(Key::Q))
				mPosition = DirectX::XMVectorAdd(mPosition, DirectX::XMVectorScale(DirectX::XMVECTOR({ 0.0f, 1.0f, 0.0f }), -(mTranslationSpeed * ts * distance)));
		}

		RecalculateViewMatrix();
	}

	void PerspectiveCamera::RecalculateViewMatrix()
	{
		mViewMatrix = DirectX::XMMatrixLookToLH(mPosition, mForward, mUp);
	}

	void PerspectiveCamera::RecalculateProjectionMatrix()
	{
		mProjection = DirectX::XMMatrixPerspectiveFovLH(mFOV, mAspectRatio, mNear, mFar);
	}

}