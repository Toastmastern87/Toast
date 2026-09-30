#pragma once

#include "TerrainObjectCommon.h"

#include "Toast/Assets/AssetManager.h"

#include "Toast/Renderer/RendererBuffer.h"

#include "PlanetSystem.h"

#include <wrl.h>
#include <d3d11.h>

namespace Toast {

	class TerrainObjectSystem 
	{
	public:
		TerrainObjectSystem() = default;
		~TerrainObjectSystem() = default;

		bool Init();

		void LoadShaders();

		void Reset();

		void Scatter(Planet* planet, const Vector3& worldTranslation);

		Ref<StructuredBuffer> GetInstanceBuffer(uint32_t typeIndex) { return mInstanceBuffers[typeIndex]; }
		ID3D11Buffer* GetIndirectArgs() { return mIndirectArgs.Get(); }
		ID3D11UnorderedAccessView* GetIndirectArgsUAV() { return mIndirectArgsUAV.Get(); }
		uint32_t GetArgsOffset(uint32_t typeIndex) { return typeIndex * TERRAIN_ARGS_STRIDE; }
	private:
		Ref<StructuredBuffer> mInstanceBuffers[MAX_TERRAIN_OBJECT_TYPES];

		Microsoft::WRL::ComPtr<ID3D11Buffer>				mIndirectArgs;
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>	mIndirectArgsUAV;

		Ref<ConstantBuffer>	mScatterCBuffer;
		Buffer				mScatterBuffer;

		AssetHandle mKickoffShaderHandle = 0;
		AssetHandle mScatterShaderHandle = 0;

		bool mInitialized = false;
	};

}