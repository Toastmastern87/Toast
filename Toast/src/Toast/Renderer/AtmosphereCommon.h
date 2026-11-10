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
		float MSGain;

		DirectX::XMFLOAT3 RayleighScattering;
		float SGain;

		DirectX::XMFLOAT3 MieScattering;
		float Pad0;

		DirectX::XMFLOAT3 MieAbsorption;
		float Pad1;

		DirectX::XMFLOAT3 GroundAlbedo;
		uint32_t FaceIndex;

		DirectX::XMFLOAT3 MieAnisotropy;
		float OzoneStrength;

		uint32_t StepsTransmittance;
		uint32_t StepsMultiScattering;
		float APFarDynamic;
		float Pad2;

		DirectX::XMFLOAT3 SunsetTint;
		float BakeIBL;

		DirectX::XMFLOAT3 MieLobeWeight;
		float MieAnisotropyBroad;
	};

	TOAST_STATIC_ASSERT(sizeof(AtmosphereCBufferGPU) == 144, "AtmosphereCBufferGPU must match the HLSL Atmosphere cbuffer (144 bytes)")
	TOAST_STATIC_ASSERT(offsetof(AtmosphereCBufferGPU, RayleighScattering) == 16, "AtmosphereCBufferGPU::RayleighScattering must be at offset 16")
	TOAST_STATIC_ASSERT(offsetof(AtmosphereCBufferGPU, MieScattering) == 32, "AtmosphereCBufferGPU::MieScattering must be at offset 32")
	TOAST_STATIC_ASSERT(offsetof(AtmosphereCBufferGPU, MieAbsorption) == 48, "AtmosphereCBufferGPU::MieAbsorption must be at offset 48")
	TOAST_STATIC_ASSERT(offsetof(AtmosphereCBufferGPU, GroundAlbedo) == 64, "AtmosphereCBufferGPU::GroundAlbedo must be at offset 64")
	TOAST_STATIC_ASSERT(offsetof(AtmosphereCBufferGPU, FaceIndex) == 76, "AtmosphereCBufferGPU::FaceIndex must be at offset 76")
	TOAST_STATIC_ASSERT(offsetof(AtmosphereCBufferGPU, MieAnisotropy) == 80, "AtmosphereCBufferGPU::MieAnisotropy must be at offset 80")
	TOAST_STATIC_ASSERT(offsetof(AtmosphereCBufferGPU, OzoneStrength) == 92, "AtmosphereCBufferGPU::OzoneStrength must be at offset 92")
	TOAST_STATIC_ASSERT(offsetof(AtmosphereCBufferGPU, StepsTransmittance) == 96, "AtmosphereCBufferGPU::StepsTransmittance must be at offset 96")
	TOAST_STATIC_ASSERT(offsetof(AtmosphereCBufferGPU, APFarDynamic) == 104, "AtmosphereCBufferGPU::APFarDynamic must be at offset 104")
	TOAST_STATIC_ASSERT(offsetof(AtmosphereCBufferGPU, SunsetTint) == 112, "AtmosphereCBufferGPU::SunsetTint must be at offset 112")
	TOAST_STATIC_ASSERT(offsetof(AtmosphereCBufferGPU, BakeIBL) == 124, "AtmosphereCBufferGPU::BakeIBL must be at offset 124")
	TOAST_STATIC_ASSERT(offsetof(AtmosphereCBufferGPU, MieLobeWeight) == 128, "AtmosphereCBufferGPU::MieLobeWeight must be at offset 128")
	TOAST_STATIC_ASSERT(offsetof(AtmosphereCBufferGPU, MieAnisotropyBroad) == 140, "AtmosphereCBufferGPU::MieAnisotropyBroad must be at offset 140")

}