#include "tpch.h"
#include "PlanetMeshBaker.h"

#include "Toast/Renderer/RendererBuffer.h"

namespace Toast {

	void PlanetMeshBaker::Init()
	{
		mBakeShaderHandle = AssetManager::GetEngineShaderHandle("PlanetMeshBake");
		TOAST_CORE_ASSERT(mBakeShaderHandle, "PlanetMeshBake shader not found in the asset registry!");

		mBakeCBuffer = ConstantBufferLibrary::Load("PlanetMeshBake", 32, std::vector<CBufferBindInfo>{ CBufferBindInfo(D3D11_COMPUTE_SHADER, (CBufferBindSlot)13) });
		mBakeCBuffer->Bind();
		mBakeBuffer.Allocate(mBakeCBuffer->GetSize());
		mBakeBuffer.ZeroInitialize();
	}

	void PlanetMeshBaker::EnsureCapacity(uint32_t patchCount, uint32_t verticesPerPatch)
	{
		if (patchCount == 0 || verticesPerPatch == 0)
			return;

		if (patchCount <= mCapacityPatches && verticesPerPatch == mVerticesPerPatch)
			return;

		mCapacityPatches = std::max(patchCount * 2, mCapacityPatches);
		mVerticesPerPatch = verticesPerPatch;

		const uint32_t capacityVertices = mCapacityPatches * mVerticesPerPatch;

		mPatchData = CreateRef<StructuredBuffer>(sizeof(PlanetMeshIcosphere::PlanetPatchGPU), mCapacityPatches, D3D11_USAGE_DYNAMIC, false, false);

		mBakedVertices = CreateRef<StructuredBuffer>(sizeof(PlanetBakedVertexGPU), capacityVertices, D3D11_USAGE_DEFAULT, true, false);

		TOAST_CORE_INFO("PlanetMeshBaker: buffer grown to %u patches / %u vertices (%.1f MB)", mCapacityPatches, capacityVertices, (capacityVertices * sizeof(PlanetBakedVertexGPU) + mCapacityPatches * sizeof(PlanetMeshIcosphere::PlanetPatchGPU)) / (1024.0 * 1024.0));
	}

	void PlanetMeshBaker::Upload(const std::vector<PlanetMeshIcosphere::PlanetPatchGPU>& patches, int patchLevels, uint32_t materialCount, float planetRadius, const DirectX::XMFLOAT3& camHiPS)
	{
		mPatchCount = (uint32_t)patches.size();
		if (mPatchCount == 0 || !mPatchData)
			return;

		mPatchData->Update(patches.data(), patches.size() * sizeof(PlanetMeshIcosphere::PlanetPatchGPU));
		mBakeBuffer.Write((uint8_t*)&mPatchCount, 4, 0);
		mBakeBuffer.Write((uint8_t*)&mVerticesPerPatch, 4, 4);
		mBakeBuffer.Write((uint8_t*)&patchLevels, 4, 8);
		mBakeBuffer.Write((uint8_t*)&materialCount, 4, 12);
		mBakeBuffer.Write((uint8_t*)&planetRadius, 4, 16);
		mBakeBuffer.Write((uint8_t*)&camHiPS, 12, 20);
		mBakeCBuffer->Map(mBakeBuffer);
	}

	void PlanetMeshBaker::Bake() 
	{
		if (mPatchCount == 0 || !mBakedVertices)
			return;

		Ref<Shader> bakeShader = AssetManager::GetAsset<Shader>(mBakeShaderHandle);
		if (!bakeShader)
			return;

		const uint32_t threadCount = mPatchCount * mVerticesPerPatch;
		uint32_t groups = (threadCount + PLANET_BAKE_THREADGROUP_SIZE - 1) / PLANET_BAKE_THREADGROUP_SIZE;
		if (groups > D3D11_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION)
		{
			TOAST_CORE_WARN("PlanetMeshBaker: %u patches need %u thread groups, over the D3D11 limit of %u - the rest are not baked", mPatchCount, groups, (uint32_t)D3D11_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION);
			groups = D3D11_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
		}

		mBakeCBuffer->Bind();
		RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 5, mPatchData->GetSRV());
		mBakedVertices->BindUAV(0);

		bakeShader->Bind();
		RenderCommand::DispatchCompute(groups, 1, 1);

		mBakedVertices->UnbindUAV(0);
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> nullSRV;
		RenderCommand::SetShaderResource(D3D11_COMPUTE_SHADER, 5, nullSRV);
	}

}