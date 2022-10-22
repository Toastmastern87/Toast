#include "tpch.h"
#include "SceneCamera.h"

namespace Toast {

	SceneCamera::SceneCamera()
	{
		RecalculateProjection();
	}

	void SceneCamera::SetPerspective(float verticalFOV, float nearClip, float farClip)
	{
		mProjectionType = ProjectionType::Perspective;
		mPerspectiveFOV = verticalFOV;
		mNearClip = nearClip;
		mFarClip = farClip;
		RecalculateProjection();
	}

	void SceneCamera::SetOrthographic(float width, float height, float nearClip, float farClip)
	{
		mProjectionType = ProjectionType::Orthographic;
		mOrthographicWidth = width;
		mOrthographicHeight = height;
		mNearClip = nearClip;
		mFarClip = farClip;
		RecalculateProjection();
	}

	void SceneCamera::SetViewportSize(uint32_t width, uint32_t height)
	{
		mAspectRatio = (float)width / (float)height;
		RecalculateProjection();
	}

	DirectX::XMFLOAT4& SceneCamera::GetForwardDirection()
	{
		DirectX::XMFLOAT4 fForward = { 0.0f, 0.0f, 1.0f, 0.0f };

		return fForward;
	}

	void SceneCamera::RecalculateProjection()
	{
		DirectX::XMMATRIX projection, orthoProjection;
		//Near and far switched due to Toast Engine running inverted-z depth
		projection = DirectX::XMMatrixPerspectiveFovLH(mPerspectiveFOV, mAspectRatio, mFarClip, mNearClip);
		//projection = DirectX::XMMatrixPerspectiveFovLH(mPerspectiveFOV, mAspectRatio, mNearClip, mFarClip);
		DirectX::XMStoreFloat4x4(&mProjection, projection);
		DirectX::XMStoreFloat4x4(&mInvProjection, DirectX::XMMatrixInverse(nullptr, projection));

		orthoProjection = DirectX::XMMatrixOrthographicLH(mOrthographicWidth, mOrthographicHeight, mNearClip, mFarClip);
		DirectX::XMStoreFloat4x4(&mOrthoProjection, orthoProjection);
		DirectX::XMStoreFloat4x4(&mInvOrthoProjection, DirectX::XMMatrixInverse(nullptr, orthoProjection));
	}

} 