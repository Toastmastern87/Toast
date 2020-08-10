#pragma once

#include <DirectXMath.h>

namespace Toast {

	class OrthographicCamera 
	{
	public:
		OrthographicCamera(float left, float right, float bottom, float top);

		void SetProjection(float left, float right, float bottom, float top);

		const DirectX::XMFLOAT3& GetPosition() const { return mPosition; }
		void SetPosition(const DirectX::XMFLOAT3& position) { mPosition = position; RecalculateViewMatrix(); }

		float GetRotation() const { return mRotation; }
		void SetRotation(float rotation) { mRotation = rotation; RecalculateViewMatrix(); }

		const DirectX::XMMATRIX& GetProjectionMatrix() const { return mProjectionMatrix; }
		const DirectX::XMMATRIX& GetViewMatrix() const { return mViewMatrix; }
		const DirectX::XMMATRIX& GetViewProjectionMatrix() const { return mViewProjectionMatrix; }
	private:
		void RecalculateViewMatrix();
	private:
		DirectX::XMMATRIX mProjectionMatrix;
		DirectX::XMMATRIX mViewMatrix;
		DirectX::XMMATRIX mViewProjectionMatrix;

		DirectX::XMFLOAT3 mPosition = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
		float mRotation = 0.0f;
	};
}