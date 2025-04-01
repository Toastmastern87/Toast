#pragma once

#include "Toast/Core/Base.h"
#include "Toast/Scene/Scene.h"

namespace Toast {

	class EnvironmentalPanel
	{
	public:
		EnvironmentalPanel() = default;
		EnvironmentalPanel(Scene* context);
		~EnvironmentalPanel() = default;

		void SetContext(Scene* context);

		void OnImGuiRender();
	private:
		Scene* mContext;
	};
}
