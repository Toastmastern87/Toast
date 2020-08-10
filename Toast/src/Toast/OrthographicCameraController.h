#pragma once

#include "Toast/Renderer/OrthographicCamera.h"
#include "Toast/Core/Timestep.h"

#include "Toast/Events/ApplicationEvent.h"
#include "Toast/Events/MouseEvent.h"

namespace Toast {

	class OrthographicCameraController 
	{
	public:
		OrthographicCameraController(float aspectRatio, bool rotation = false);

		void OnUpdate(Timestep ts);
		void OnEvent(Event& e);

		OrthographicCamera& GetCamera() { return mCamera; }
		const OrthographicCamera& GetCamera() const { return mCamera; }

		float GetZoomLevel() const { return mZoomLevel; }
		void SetZoomLevel(float level) { mZoomLevel = level; }
	private:
		bool OnMouseScrolled(MouseScrolledEvent& e);
		bool OnWindowResize(WindowResizeEvent& e);
	private:
		float mAspectRatio;
		float mZoomLevel = 1.0f;
		OrthographicCamera mCamera;

		bool mRotation;

		DirectX::XMFLOAT3 mCameraPosition = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
		float mCameraRotation = 0.0f;
		float mCameraTranslationSpeed = 5.0f;
		float mCameraRotationSpeed = 180.0f;
	};
}