#include "tpch.h"
#include "Toast/Renderer/OrthographicCamera.h"

namespace Toast {

	OrthographicCamera::OrthographicCamera(float left, float right, float bottom, float top)
		: mProjectionMatrix(DirectX::XMMatrixOrthographicLH((right - left), (bottom - top), -1.0f, 1.0f)), mViewMatrix(DirectX::XMMatrixIdentity())
	{
		TOAST_PROFILE_FUNCTION();

		mViewProjectionMatrix = DirectX::XMMatrixMultiply(mViewMatrix, mProjectionMatrix);
	}

	void OrthographicCamera::SetProjection(float left, float right, float bottom, float top)
	{
		TOAST_PROFILE_FUNCTION();

		mProjectionMatrix = DirectX::XMMatrixOrthographicLH((right - left), (bottom - top), -1.0f, 1.0f);
		mViewProjectionMatrix = DirectX::XMMatrixMultiply(mViewMatrix, mProjectionMatrix);
	}

	void OrthographicCamera::RecalculateViewMatrix()
	{
		TOAST_PROFILE_FUNCTION();

		DirectX::XMMATRIX transform = DirectX::XMMatrixRotationZ(DirectX::XMConvertToRadians(mRotation)) * DirectX::XMMatrixTranslation(mPosition.x, mPosition.y, mPosition.z);

		mViewMatrix = DirectX::XMMatrixInverse(nullptr, transform);
		mViewProjectionMatrix = DirectX::XMMatrixMultiply(mViewMatrix, mProjectionMatrix);
	}
}