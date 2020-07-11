#pragma once

#include <d3d11.h>

namespace Toast {

	class Shader {
	public:
		Shader(const std::string& vertexSrc, const std::string& pixelSrc);
		~Shader();

		void Bind() const;
		void Unbind() const;

	private:
		ID3D11VertexShader* mVertexShader = nullptr;
		ID3D11PixelShader* mPixelShader = nullptr;
	};
}