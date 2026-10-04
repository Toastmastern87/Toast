#pragma once

#include "PlanetMeshBakerCommon.h"
#include "PlanetSystem.h"

namespace Toast {

	class StructuredBuffer;
	class ConstantBuffer;

	class PlanetMeshBaker
	{
	public:
		PlanetMeshBaker() = default;

		void Init();

		void EnsureCapacity(uint32_t patchCount, uint32_t verticesPerPatch);

		void Upload(const std::vector<PlanetMeshIcosphere::PlanetPatchGPU>& patches, int patchLevels, uint32_t materialCount, float planetRadius, const DirectX::XMFLOAT3& camHiPS);

		void Bake();

		const Ref<StructuredBuffer>& GetBakedVertices() const { return mBakedVertices; }
	private:
		Ref<StructuredBuffer> mPatchData;
		Ref<StructuredBuffer> mBakedVertices;

		Ref<ConstantBuffer> mBakeCBuffer;
		Buffer mBakeBuffer;

		AssetHandle mBakeShaderHandle = 0;

		uint32_t mCapacityPatches;
		uint32_t mVerticesPerPatch;
		uint32_t mPatchCount;
	};

}