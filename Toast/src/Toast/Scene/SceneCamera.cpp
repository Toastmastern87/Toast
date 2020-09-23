#include "tpch.h"
#include "SceneCamera.h"

namespace Toast {


	SceneCamera::SceneCamera()
	{
		RecalculateProjection();
	}

	void SceneCamera::SetPerpective(float verticalFOV, float nearClip, float farClip)
	{
		mProjectionType = ProjectionType::Perspective;
		mPerspectiveFOV = verticalFOV;
		mPerspectiveNear = nearClip;
		mPerspectiveFar = farClip;
		RecalculateProjection();
	}

	void SceneCamera::SetOrthographic(float size, float nearClip, float farClip)
	{
		mProjectionType = ProjectionType::Orthographic;
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
		switch (mProjectionType) 
		{
		case ProjectionType::Perspective:
			mProjection = DirectX::XMMatrixPerspectiveFovLH(mPerspectiveFOV, mAspectRatio, mPerspectiveNear, mPerspectiveFar);

			break;
		case ProjectionType::Orthographic:
			float orthoLeft = -mOrthographicSize * mAspectRatio * 0.5f;
			float orthoRight = mOrthographicSize * mAspectRatio * 0.5f;
			float orthoBottom = mOrthographicSize * 0.5f;
			float orthoTop = -mOrthographicSize * 0.5f;

			mProjection = DirectX::XMMatrixOrthographicLH((orthoRight - orthoLeft), (orthoBottom - orthoTop), mOrthographicNear, mOrthographicFar);

			break;
		}
	}

}