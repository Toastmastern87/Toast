#pragma once

#include "Toast/Assets/Asset.h"

namespace Toast {

	class Script : public Asset
	{
	public:
		Script() = default;

		static AssetType GetStaticType() { return AssetType::Script; }
		virtual AssetType GetAssetType() const override { return AssetType::Script; }
	};

}

