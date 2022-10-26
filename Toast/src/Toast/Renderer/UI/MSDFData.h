#pragma once

#undef INFINITE
#include "../vendor/msdf-atlas-gen/msdf-atlas-gen/msdf-atlas-gen.h"

#include <vector>

namespace Toast {

	struct MSDFData
	{
		msdf_atlas::FontGeometry FontGeometry;
		std::vector<msdf_atlas::GlyphGeometry> Glyphs;
	};

}