#pragma once

#include "Toast/Core/Timestep.h"
#include "Toast/Renderer/Mesh.h"

namespace Toast {
	
	class Planet
	{
	public:
		Planet(Ref<Mesh> planetMesh = nullptr);
		~Planet();

		void OnUpdate(Timestep ts);
		
		void SetMesh(Ref<Mesh> planetMesh) { mPlanetMesh = planetMesh; }
	private:
		void GenerateSphereCellGeometry(uint8_t maxPlanetCellLevel);
		void GenerateDistanceLUT();
	private:
		int mSubdivisions = 0, mPatchLevels = 0;
		Ref<Mesh> mPlanetMesh;
	};
}