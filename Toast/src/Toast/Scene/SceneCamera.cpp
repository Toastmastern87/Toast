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

	void SceneCamera::SetOrthographic(float size, float nearClip, float farClip)
	{
		mProjectionType = ProjectionType::Orthographic;
		mOrthographicSize = size;
		mNearClip = nearClip;
		mFarClip = farClip;
		RecalculateProjection();
	}

	void SceneCamera::SetViewportSize(uint32_t width, uint32_t height)
	{
		mAspectRatio = (float)width / (float)height;
		RecalculateProjection();
	}

	void SceneCamera::RecalculateProjection()
	{
		DirectX::XMMATRIX projection;

		switch (mProjectionType) 
		{
		case ProjectionType::Perspective:
			projection = DirectX::XMMatrixPerspectiveFovLH(mPerspectiveFOV, mAspectRatio, mNearClip, mFarClip);
			DirectX::XMStoreFloat4x4(&mProjection, projection);
			DirectX::XMStoreFloat4x4(&mInvProjection, DirectX::XMMatrixInverse(nullptr, projection));

			break;
		case ProjectionType::Orthographic:
			float orthoLeft = -mOrthographicSize * mAspectRatio * 0.5f;
			float orthoRight = mOrthographicSize * mAspectRatio * 0.5f;
			float orthoBottom = mOrthographicSize * 0.5f;
			float orthoTop = -mOrthographicSize * 0.5f;

			projection = DirectX::XMMatrixOrthographicLH((orthoRight - orthoLeft), (orthoBottom - orthoTop), mNearClip, mFarClip);
			DirectX::XMStoreFloat4x4(&mProjection, projection);
			DirectX::XMStoreFloat4x4(&mInvProjection, DirectX::XMMatrixInverse(nullptr, projection));

			break;
		}
	}

}