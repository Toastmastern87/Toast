#pragma once

#include "Toast/Core/Base.h"
#include "Toast/Scene/Scene.h"

namespace Toast {

	class EnvironmentalPanel
	{
	public:
		EnvironmentalPanel() = default;
		EnvironmentalPanel(const Ref<Scene>& context);
		~EnvironmentalPanel() = default;

		void SetContext(const Ref<Scene>& context);

		void OnImGuiRender();
	private:
		Ref<Scene> mContext;
	};
}
