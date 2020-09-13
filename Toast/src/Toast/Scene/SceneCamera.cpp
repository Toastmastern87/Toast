#include "tpch.h"
#include "SceneCamera.h"

namespace Toast {


	SceneCamera::SceneCamera()
	{
		RecalculateProjection();
	}

	void SceneCamera::SetOrthographic(float size, float nearClip, float farClip)
	{
		mOrthographicSize = size;
		mOrthographicNear = nearClip;
		mOrthographicFar = farClip;	
		RecalculateProjection();
	}

	void SceneCamera::SetViewportSize(uint32_t width, uint32_t height)
	{
		mAspectRatio = (float)width / (float)height;
		RecalculateProjection();
	}

	void SceneCamera::RecalculateProjection()
	{
		float orthoLeft = -mOrthographicSize * mAspectRatio * 0.5f;
		float orthoRight = mOrthographicSize * mAspectRatio * 0.5f;
		float orthoBottom = mOrthographicSize * 0.5f;
		float orthoTop = -mOrthographicSize * 0.5f;

		mProjection = DirectX::XMMatrixOrthographicLH((orthoRight - orthoLeft), (orthoBottom - orthoTop), mOrthographicNear, mOrthographicFar);
	}

}