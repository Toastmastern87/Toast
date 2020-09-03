#include "tpch.h"
#include "Toast/Renderer/OrthographicCameraController.h"

#include "Toast/Core/Input.h"
#include "Toast/Core/KeyCodes.h"

namespace Toast {

	OrthographicCameraController::OrthographicCameraController(float aspectRatio, bool rotation) 
		: mAspectRatio(aspectRatio), mCamera(-mAspectRatio * mZoomLevel, mAspectRatio * mZoomLevel, mZoomLevel, -mZoomLevel), mRotation(rotation)
	{

	}

	void OrthographicCameraController::OnUpdate(Timestep ts) 
	{
		TOAST_PROFILE_FUNCTION();

		if (Input::IsKeyPressed(TOAST_KEY_A))
		{
			mCameraPosition.x -= cos(DirectX::XMConvertToRadians(mCameraRotation)) * mCameraTranslationSpeed * ts;
			mCameraPosition.y -= sin(DirectX::XMConvertToRadians(mCameraRotation)) * mCameraTranslationSpeed * ts;
		}		
		else if (Input::IsKeyPressed(TOAST_KEY_D))
		{
			mCameraPosition.x += cos(DirectX::XMConvertToRadians(mCameraRotation)) * mCameraTranslationSpeed * ts;
			mCameraPosition.y += sin(DirectX::XMConvertToRadians(mCameraRotation)) * mCameraTranslationSpeed * ts;
		}

		if (Input::IsKeyPressed(TOAST_KEY_W))
		{
			mCameraPosition.x += -sin(DirectX::XMConvertToRadians(mCameraRotation)) * mCameraTranslationSpeed * ts;
			mCameraPosition.y += cos(DirectX::XMConvertToRadians(mCameraRotation)) * mCameraTranslationSpeed * ts;
		}
		else if (Input::IsKeyPressed(TOAST_KEY_S))
		{
			mCameraPosition.x -= -sin(DirectX::XMConvertToRadians(mCameraRotation)) * mCameraTranslationSpeed * ts;
			mCameraPosition.y -= cos(DirectX::XMConvertToRadians(mCameraRotation)) * mCameraTranslationSpeed * ts;
		}

		if (mRotation) 
		{
			if (Input::IsKeyPressed(TOAST_KEY_Q))
				mCameraRotation += mCameraRotationSpeed * ts;
			else if (Input::IsKeyPressed(TOAST_KEY_E))
				mCameraRotation -= mCameraRotationSpeed * ts;

			if (mCameraRotation > 180.0f)
				mCameraRotation -= 360.0f;
			else if (mCameraRotation <= -180.0f)
				mCameraRotation += 360.0f;

			mCamera.SetRotation(mCameraRotation);
		}

		mCamera.SetPosition(mCameraPosition);

		mCameraTranslationSpeed = mZoomLevel;
	}

	void OrthographicCameraController::OnEvent(Event& e) 
	{
		TOAST_PROFILE_FUNCTION();

		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<MouseScrolledEvent>(TOAST_BIND_EVENT_FN(OrthographicCameraController::OnMouseScrolled));
		dispatcher.Dispatch<WindowResizeEvent>(TOAST_BIND_EVENT_FN(OrthographicCameraController::OnWindowResize));
	}

	void OrthographicCameraController::OnResize(float width, float height)
	{
		mAspectRatio = width / height;
		mCamera.SetProjection(-mAspectRatio * mZoomLevel, mAspectRatio * mZoomLevel, mZoomLevel, -mZoomLevel);
	}

	bool OrthographicCameraController::OnMouseScrolled(MouseScrolledEvent& e)
	{
		TOAST_PROFILE_FUNCTION();

		mZoomLevel -= e.GetDelta() * 0.5f;
		mZoomLevel = std::max(mZoomLevel, 0.25f);
		mCamera.SetProjection(-mAspectRatio * mZoomLevel, mAspectRatio * mZoomLevel, mZoomLevel, -mZoomLevel);
		return false;
	}

	bool OrthographicCameraController::OnWindowResize(WindowResizeEvent& e) 
	{
		TOAST_PROFILE_FUNCTION();

		OnResize((float)e.GetWidth(), (float)e.GetHeight());
		return false;
	}
}