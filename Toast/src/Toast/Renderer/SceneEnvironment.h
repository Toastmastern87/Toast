#pragma once

#include "Texture.h"

namespace Toast {

	struct Environment
	{
		std::string FilePath;
		Ref<TextureCube> RadianceMap;
		Ref<TextureCube> IrradianceMap;
		Ref<Texture2D> SpecularBRDFLUT;

		static Environment Load(const std::string& filepath);
	};

}