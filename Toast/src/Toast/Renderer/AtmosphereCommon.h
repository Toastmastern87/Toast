#pragma once

#include "Toast/Core/Base.h"

#include <DirectXMath.h>
#include <cstddef>
#include <cstdint>

namespace Toast {

	// CPU Mirror of `cbuffer Atmosphere : register(b5)`
	struct AtmosphereCBufferGPU
	{
		float AtmosphereHeight;
		float RayleighScaleHeight;
		float MieScaleHeight;
		float OzoneStrength;

		DirectX::XMFLOAT3 RayleighScattering;
		float MieAnisotropyBroad;

		DirectX::XMFLOAT3 MieScattering;
		float BakeIBL;

		DirectX::XMFLOAT3 MieAbsorption;
		uint32_t FaceIndex;

		DirectX::XMFLOAT3 GroundAlbedo;
		uint32_t StepsTransmittance;

		DirectX::XMFLOAT3 MieAnisotropy;
		uint32_t StepsMultiScattering;

		DirectX::XMFLOAT3 MieLobeWeight;
		float Pad0;
	};

	TOAST_STATIC_ASSERT(sizeof(AtmosphereCBufferGPU) == 112, "AtmosphereCBufferGPU must match the HLSL Atmosphere cbuffer (112 bytes)")
	TOAST_STATIC_ASSERT(offsetof(AtmosphereCBufferGPU, OzoneStrength) == 12, "AtmosphereCBufferGPU::OzoneStrength must be at offset 12")
	TOAST_STATIC_ASSERT(offsetof(AtmosphereCBufferGPU, RayleighScattering) == 16, "AtmosphereCBufferGPU::RayleighScattering must be at offset 16")
	TOAST_STATIC_ASSERT(offsetof(AtmosphereCBufferGPU, MieAnisotropyBroad) == 28, "AtmosphereCBufferGPU::MieAnisotropyBroad must be at offset 28")
	TOAST_STATIC_ASSERT(offsetof(AtmosphereCBufferGPU, MieScattering) == 32, "AtmosphereCBufferGPU::MieScattering must be at offset 32")
	TOAST_STATIC_ASSERT(offsetof(AtmosphereCBufferGPU, BakeIBL) == 44, "AtmosphereCBufferGPU::BakeIBL must be at offset 44")
	TOAST_STATIC_ASSERT(offsetof(AtmosphereCBufferGPU, MieAbsorption) == 48, "AtmosphereCBufferGPU::MieAbsorption must be at offset 48")
	TOAST_STATIC_ASSERT(offsetof(AtmosphereCBufferGPU, FaceIndex) == 60, "AtmosphereCBufferGPU::FaceIndex must be at offset 60")
	TOAST_STATIC_ASSERT(offsetof(AtmosphereCBufferGPU, GroundAlbedo) == 64, "AtmosphereCBufferGPU::GroundAlbedo must be at offset 64")
	TOAST_STATIC_ASSERT(offsetof(AtmosphereCBufferGPU, StepsTransmittance) == 76, "AtmosphereCBufferGPU::StepsTransmittance must be at offset 76")
	TOAST_STATIC_ASSERT(offsetof(AtmosphereCBufferGPU, MieAnisotropy) == 80, "AtmosphereCBufferGPU::MieAnisotropy must be at offset 80")
	TOAST_STATIC_ASSERT(offsetof(AtmosphereCBufferGPU, StepsMultiScattering) == 92, "AtmosphereCBufferGPU::StepsMultiScattering must be at offset 92")
	TOAST_STATIC_ASSERT(offsetof(AtmosphereCBufferGPU, MieLobeWeight) == 96, "AtmosphereCBufferGPU::MieLobeWeight must be at offset 96")

	struct SunDiscCBufferGPU
	{
		float SunDiscRadius;    // rad
		int32_t SunDiscToggle;  // 1 = draw the sun disc (0 while baking the IBL cube)
		float Pad0;
		float Pad1;
	};

	TOAST_STATIC_ASSERT(sizeof(SunDiscCBufferGPU) == 16, "SunDiscCBufferGPU must match the HLSL SunDiscSettings cbuffer (16 bytes)")
	TOAST_STATIC_ASSERT(offsetof(SunDiscCBufferGPU, SunDiscToggle) == 4, "SunDiscCBufferGPU::SunDiscToggle must be at offset 4")

}