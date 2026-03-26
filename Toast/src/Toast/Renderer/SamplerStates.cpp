#include "tpch.h"
#include "SamplerStates.h"

#include "Toast/Renderer/RenderCommand.h"

namespace Toast {

	std::array<Microsoft::WRL::ComPtr<ID3D11SamplerState>, static_cast<size_t>(SamplerType::Count)> SamplerStates::sSamplers;

	void SamplerStates::Init() 
	{
		RendererAPI* API = RenderCommand::sRendererAPI.get();
		ID3D11Device* device = API->GetDevice();

		auto CreateSampler = [&](SamplerType type, const D3D11_SAMPLER_DESC& desc)
			{
				HRESULT hr = device->CreateSamplerState(&desc, sSamplers[static_cast<size_t>(type)].GetAddressOf());
				TOAST_CORE_ASSERT(SUCCEEDED(hr), "Failed to create sampler state");
			};

		// Default: Anisotropic, Wrap
		{
			D3D11_SAMPLER_DESC desc = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
			desc.Filter = D3D11_FILTER_ANISOTROPIC;
			desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
			desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
			desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
			desc.MaxAnisotropy = 16;
			CreateSampler(SamplerType::Default, desc);
		}

		// LinearClamp
		{
			D3D11_SAMPLER_DESC desc = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
			desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
			desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
			desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			CreateSampler(SamplerType::LinearClamp, desc);
		}

		// LinearWrap
		{
			D3D11_SAMPLER_DESC desc = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
			desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
			desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
			desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
			CreateSampler(SamplerType::LinearWrap, desc);
		}

		// PointClamp
		{
			D3D11_SAMPLER_DESC desc = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
			desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
			desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
			desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
			desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			CreateSampler(SamplerType::PointClamp, desc);
		}

		// BRDF (same desc as LinearClamp, but semantically separate)
		{
			D3D11_SAMPLER_DESC desc = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
			desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
			desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
			desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			CreateSampler(SamplerType::BRDF, desc);
		}

		// UWrapVClamp
		{
			D3D11_SAMPLER_DESC desc = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
			desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
			desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
			desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			CreateSampler(SamplerType::UWrapVClamp, desc);
		}

		// SkyTest: Anisotropic, Clamp, MipLODBias 0.5
		{
			D3D11_SAMPLER_DESC desc = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
			desc.Filter = D3D11_FILTER_ANISOTROPIC;
			desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
			desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
			desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			desc.MaxAnisotropy = 16;
			desc.MipLODBias = 0.5f;
			CreateSampler(SamplerType::SkyTest, desc);
		}

		// ShadowCmp: Comparison sampler for shadow mapping
		{
			D3D11_SAMPLER_DESC desc = {};
			desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
			desc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
			desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
			desc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
			desc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
			desc.BorderColor[0] = 0.0f;
			desc.BorderColor[1] = 0.0f;
			desc.BorderColor[2] = 0.0f;
			desc.BorderColor[3] = 0.0f;
			desc.MipLODBias = 0.0f;
			desc.MaxAnisotropy = 1;
			desc.MinLOD = 0.0f;
			desc.MaxLOD = D3D11_FLOAT32_MAX;
			CreateSampler(SamplerType::ShadowCmp, desc);
		}
	}

	void SamplerStates::Shutdown()
	{
		for (auto& sampler : sSamplers)
			sampler.Reset();
	}

}