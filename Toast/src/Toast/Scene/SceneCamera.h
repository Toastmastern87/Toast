#pragma once

#include "Toast/Renderer/Camera.h"

namespace Toast {

	class SceneCamera : public Camera 
	{
	public:
		SceneCamera();
		virtual ~SceneCamera() = default;

		void SetOrthographic(float size, float nearClip, float farClip); 

		void SetViewportSize(uint32_t width, uint32_t height);

		float GetOrthographicSize() const { return mOrthographicSize; }
		void SetOrthographicSize(float size) { mOrthographicSize = std::max(size, 0.1f); RecalculateProjection(); }
	private:
		void RecalculateProjection();
	private:
		float mOrthographicSize = 10.0f;
		float mOrthographicNear = -1.0f, mOrthographicFar = 1.0f;

		float mAspectRatio = 1.0f;
	};
}