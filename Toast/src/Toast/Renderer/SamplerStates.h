#pragma once

#include <array>

#include <d3d11.h>
#include <wrl.h>

namespace Toast {

	enum class SamplerType : uint8_t
	{
		Default,		// Anisotropic, Wrap
		LinearClamp,	// Linear, Clamp
		LinearWrap,		// Linear, Wrap
		PointClamp,		// Point, Clamp
		BRDF,			// Linear, Clamp (Semantically distinct)
		UWrapVClamp,	// Linear, U=Wrap V=Clamp
		SkyTest,		// Anisotropic, Clamp, bias 0.5
		ShadowCmp,		// Comparison Sampler
		Count
	};

	class SamplerStates 
	{
	public:
		static void Init();
		static void Shutdown();

		static ID3D11SamplerState* Get(SamplerType type)
		{
			return sSamplers[static_cast<size_t>(type)].Get();
		}
	private:
		static std::array<Microsoft::WRL::ComPtr<ID3D11SamplerState>, static_cast<size_t>(SamplerType::Count)> sSamplers;
	};

}