#include "tpch.h"
#include "EditorCamera.h"

#include "Toast/Core/Input.h"

namespace Toast {

	EditorCamera::EditorCamera(float fov, float aspectRatio, float nearClip, float farClip)
		: mFOV(DirectX::XMConvertToRadians(fov)), mAspectRatio(aspectRatio)
	{
		DirectX::XMMATRIX projection = DirectX::XMMatrixPerspectiveFovLH(mFOV, mAspectRatio, nearClip, farClip);
		DirectX::XMMATRIX invProjection = DirectX::XMMatrixInverse(nullptr, projection);
		DirectX::XMMATRIX view = DirectX::XMMatrixLookToLH(mPosition, DirectX::XMLoadFloat4(&GetForwardDirection()), GetUpDirection());
		DirectX::XMMATRIX invView = DirectX::XMMatrixInverse(nullptr, view);

		DirectX::XMFLOAT4X4 fView, fInvView, fProjection, fInvProjection;

		DirectX::XMStoreFloat4x4(&fView, view);
		DirectX::XMStoreFloat4x4(&fInvView, invView);
		DirectX::XMStoreFloat4x4(&fProjection, projection);
		DirectX::XMStoreFloat4x4(&fInvProjection, invProjection);

		mViewMatrix = fView;
		mInvViewMatrix = fInvView;
		mProjection = fProjection;
		mInvProjection = fInvProjection;
		mNearClip = nearClip;
		mFarClip = farClip;

		//Setting up starting focal point
		mFocalPoint = DirectX::XMVector3Rotate(mFocalPoint, DirectX::XMQuaternionRotationRollPitchYaw(mPitch, mYaw, 0.0f));

		UpdateView();
	}

	void EditorCamera::UpdateProjection()
	{
		mAspectRatio = (float)mViewportWidth / (float)mViewportHeight;
		DirectX::XMMATRIX projection = DirectX::XMMatrixPerspectiveFovLH(mFOV, mAspectRatio, mNearClip, mFarClip);

		DirectX::XMStoreFloat4x4(&mInvProjection, DirectX::XMMatrixInverse(nullptr, projection));
		DirectX::XMStoreFloat4x4(&mProjection, projection);
	}

	void EditorCamera::UpdateView()
	{
		DirectX::XMMATRIX view = DirectX::XMMatrixLookToLH(mPosition, DirectX::XMLoadFloat4(&GetForwardDirection()), GetUpDirection());
		DirectX::XMStoreFloat4x4(&mViewMatrix, view);
		DirectX::XMStoreFloat4x4(&mInvViewMatrix, DirectX::XMMatrixInverse(nullptr, view));
	}

	void EditorCamera::OnUpdate(Timestep ts)
	{
		const DirectX::XMVECTOR& mouse{ Input::GetMouseX(), Input::GetMouseY() };
		DirectX::XMVECTOR delta = DirectX::XMVectorScale(DirectX::XMVectorSubtract(mouse, mInitialCursorPosition), 0.003f);
		mInitialCursorPosition = mouse;

		if (Input::IsMouseButtonPressed(Mouse::ButtonMiddle) && Input::IsKeyPressed(Key::LeftShift))
			MousePan(delta);
		else if (Input::IsMouseButtonPressed(Mouse::ButtonMiddle))
			MouseRotate(delta);

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

	DirectX::XMFLOAT4& EditorCamera::GetForwardDirection()
	{
		DirectX::XMFLOAT4 fForward;
		DirectX::XMStoreFloat4(&fForward, DirectX::XMVector3Rotate({ 0.0f, 0.0f, 1.0f }, GetOrientation()));

		return fForward;
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
		mFocalPoint = DirectX::XMVectorAdd(mFocalPoint, DirectX::XMVectorScale(GetRightDirection(), (DirectX::XMVectorGetX(delta) * -xSpeed * 5.0f)));
		mPosition = DirectX::XMVectorAdd(mPosition, DirectX::XMVectorScale(GetRightDirection(), (DirectX::XMVectorGetX(delta) * -xSpeed * 5.0f)));
		mFocalPoint = DirectX::XMVectorAdd(mFocalPoint, DirectX::XMVectorScale(GetUpDirection(), (DirectX::XMVectorGetY(delta) * ySpeed * 5.0f)));
		mPosition = DirectX::XMVectorAdd(mPosition, DirectX::XMVectorScale(GetUpDirection(), (DirectX::XMVectorGetY(delta) * ySpeed * 5.0f)));
	}

	void EditorCamera::MouseRotate(const DirectX::XMVECTOR& delta)
	{
		float yawSign = DirectX::XMVectorGetY(GetUpDirection()) < 0 ? -1.0f : 1.0f;
		mYaw += yawSign * DirectX::XMVectorGetX(delta) * RotationSpeed();
		mPitch += DirectX::XMVectorGetY(delta) * RotationSpeed();

		mFocalPoint = DirectX::XMVector3Rotate(mFocalPoint, DirectX::XMQuaternionRotationRollPitchYaw((DirectX::XMVectorGetY(delta) * RotationSpeed()), (yawSign * DirectX::XMVectorGetX(delta) * RotationSpeed()), 0.0f));
	}

	void EditorCamera::MouseZoom(float delta)
	{
		mPosition = DirectX::XMVectorAdd(mPosition, DirectX::XMVectorScale(DirectX::XMLoadFloat4(&GetForwardDirection()), delta * ZoomSpeed()));
		float distanceToFocalPoint = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(mPosition, mFocalPoint)));
		if (distanceToFocalPoint < 10.0f) 
		{
			mPosition = DirectX::XMVectorSubtract(mPosition, DirectX::XMVectorScale(DirectX::XMLoadFloat4(&GetForwardDirection()), delta * ZoomSpeed()));
			distanceToFocalPoint = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(mPosition, mFocalPoint)));
		}
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
		float distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(mPosition, mFocalPoint))) * 0.2f;
		distance = std::max(distance, 0.0f);
		float speed = distance * distance;
		speed = std::min(speed, 1000.0f); // max speed = 1000
		return speed;
	}

}