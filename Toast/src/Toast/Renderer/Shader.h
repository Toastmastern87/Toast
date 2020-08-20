#pragma once

#include <DirectXMath.h>

namespace Toast {

	class Shader 
	{
	public:
		virtual ~Shader() = default;

		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		virtual void SetSceneData(const DirectX::XMMATRIX& matrix) = 0;
		virtual void SetObjectData(const DirectX::XMMATRIX& matrix) = 0;
		virtual void SetColorData(const DirectX::XMFLOAT4& values, float tilingFactor) = 0;

		virtual const std::string GetName() const = 0;

		static Ref<Shader> Create(const std::string& filepath);
	};

	class ShaderLibrary 
	{
	public:
		void Add(const std::string name, const Ref<Shader>& shader);
		void Add(const Ref<Shader>& shader);
		Ref<Shader> Load(const std::string& filepath);
		Ref<Shader> Load(const std::string& name, const std::string& filepath);

		Ref<Shader> Get(const std::string& name);

		bool Exists(const std::string& name);
	private:
		std::unordered_map<std::string, Ref<Shader>> mShaders;
	};
}