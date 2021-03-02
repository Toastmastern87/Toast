#include "tpch.h"
#include "EditorCamera.h"

#include "Toast/Core/Input.h"

namespace Toast {

	EditorCamera::EditorCamera(float fov, float aspectRatio, float nearClip, float farClip)
		: mFOV(DirectX::XMConvertToRadians(fov)), mAspectRatio(aspectRatio), mNearClip(nearClip), mFarClip(farClip), Camera(DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(fov), aspectRatio, nearClip, farClip))
	{
		UpdateView();
	}

	void EditorCamera::UpdateProjection()
	{
		mAspectRatio = (float)mViewportWidth / (float)mViewportHeight;
		mProjection = DirectX::XMMatrixPerspectiveFovLH(mFOV, mAspectRatio, mNearClip, mFarClip);
	}

	void EditorCamera::UpdateView()
	{
		mPosition = CalculatePosition();
		mViewMatrix = DirectX::XMMatrixLookToLH(mPosition, GetForwardDirection(), GetUpDirection());
	}

	void EditorCamera::OnUpdate(Timestep ts)
	{
		if (Input::IsKeyPressed(Key::LeftAlt))
		{
			const DirectX::XMVECTOR& mouse{ Input::GetMouseX(), Input::GetMouseY() };
			DirectX::XMVECTOR delta = DirectX::XMVectorScale(DirectX::XMVectorSubtract(mouse, mInitialCursorPosition), 0.003f);
			mInitialCursorPosition = mouse;

			if (Input::IsMouseButtonPressed(Mouse::ButtonMiddle))
				MousePan(delta);
			else if (Input::IsMouseButtonPressed(Mouse::ButtonLeft))
				MouseRotate(delta);
			else if (Input::IsMouseButtonPressed(Mouse::ButtonRight))
				MouseZoom(DirectX::XMVectorGetY(delta));
		}

		UpdateView();
	}

	void EditorCamera::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<MouseScrolledEvent>(TOAST_BIND_EVENT_FN(EditorCamera::OnMouseScroll));
	}

	bool EditorCamera::OnMouseScroll(MouseScrolledEvent& e)
	{
		float delta = e.GetDelta() * 0.1f;
		MouseZoom(delta);
		UpdateView();

		return false;
	}

	DirectX::XMVECTOR EditorCamera::GetForwardDirection() const
	{
		return DirectX::XMVector3Rotate({ 0.0f, 0.0f, 1.0f }, GetOrientation());
	}

	DirectX::XMVECTOR EditorCamera::GetUpDirection() const
	{
		return DirectX::XMVector3Rotate({ 0.0f, 1.0f, 0.0f }, GetOrientation());
	}

	DirectX::XMVECTOR EditorCamera::GetRightDirection() const
	{
		return DirectX::XMVector3Rotate({ 1.0f, 0.0f, 0.0f }, GetOrientation());
	}

	DirectX::XMVECTOR EditorCamera::GetOrientation() const
	{
		return DirectX::XMQuaternionRotationRollPitchYaw(mPitch, mYaw, 0.0f);
	}

	void EditorCamera::MousePan(const DirectX::XMVECTOR& delta)
	{
		auto [xSpeed, ySpeed] = PanSpeed();
		mFocalPoint = DirectX::XMVectorAdd(mFocalPoint, DirectX::XMVectorScale(GetRightDirection(), (DirectX::XMVectorGetX(delta) * xSpeed * mDistance)));
		mFocalPoint = DirectX::XMVectorAdd(mFocalPoint, DirectX::XMVectorScale(GetUpDirection(), (DirectX::XMVectorGetY(delta) * ySpeed * mDistance)));
	}

	void EditorCamera::MouseRotate(const DirectX::XMVECTOR& delta)
	{
		float yawSign = DirectX::XMVectorGetY(GetUpDirection()) < 0 ? -1.0f : 1.0f;
		mYaw += yawSign * DirectX::XMVectorGetX(delta) * RotationSpeed();
		mPitch += DirectX::XMVectorGetY(delta) * RotationSpeed();
	}

	void EditorCamera::MouseZoom(float delta)
	{
		mDistance -= delta * ZoomSpeed();
		if (mDistance < 1.0f)
		{
			mFocalPoint = DirectX::XMVectorAdd(mFocalPoint, GetForwardDirection());
			mDistance = 1.0f;
		}
	}

	DirectX::XMVECTOR EditorCamera::CalculatePosition() const
	{
		return DirectX::XMVectorSubtract(mFocalPoint, DirectX::XMVectorScale(GetForwardDirection(), mDistance));
	}

	std::pair<float, float> EditorCamera::PanSpeed() const
	{
		float x = std::min(mViewportWidth / 1000.0f, 2.4f); // Max is 2.4f
		float xFactor = 0.0366f * (x * x) - 0.1778f * x + 0.3021;

		float y = std::min(mViewportHeight / 1000.0f, 2.4f); // Max is 2.4f
		float yFactor = 0.0366f * (y * y) - 0.1778f * y + 0.3021;

		return { xFactor, yFactor };
	}

	float EditorCamera::RotationSpeed() const
	{
		return 0.8f;
	}

	float EditorCamera::ZoomSpeed() const
	{
		float distance = mDistance * 0.2f;
		distance = std::max(distance, 0.0f);
		float speed = distance * distance;
		speed = std::min(speed, 1000.0f); // max speed = 100
		return speed;
	}

}