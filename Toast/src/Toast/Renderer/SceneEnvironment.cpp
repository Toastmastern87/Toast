#include "tpch.h"
#include "SceneEnvironment.h"

#include "Renderer.h"

namespace Toast {

	static Ref<Shader> spBRDFShader;

	Environment Environment::Load(const std::string& filepath)
	{
		auto radiance = Renderer::CreateEnvironmentMap(filepath);
		// Compute Cook-Torrance BRDF 2D LUT for split-sum approximation.
		Ref<Texture2D> spBRDFLUT = CreateRef<Texture2D>(DXGI_FORMAT_R16G16_FLOAT, DXGI_FORMAT_R16G16_FLOAT, 256, 256);

		if (!spBRDFShader)
			spBRDFShader = CreateRef<Shader>("assets/shaders/Environment/SPBRDF.hlsl");

		spBRDFLUT->CreateUAV(0);

		spBRDFLUT->BindForReadWrite(0, D3D11_COMPUTE_SHADER);
		spBRDFShader->Bind();
		RenderCommand::DispatchCompute(spBRDFLUT->GetWidth() / 32, spBRDFLUT->GetHeight() / 32, 1);
		spBRDFLUT->UnbindUAV();

		return { filepath, radiance, nullptr, spBRDFLUT };
	}
}