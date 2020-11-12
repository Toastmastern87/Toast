#pragma once

#include "Toast/Renderer/Material.h"

namespace Toast {

	class MaterialPanel
	{
	public:
		MaterialPanel() = default;
		~MaterialPanel() = default;

		void SetContext(const Ref<Material>& context);

		void OnImGuiRender();
	private:
		void DrawMaterialProperties();
	private:
		Ref<Material> mSelectionContext;
	};

}