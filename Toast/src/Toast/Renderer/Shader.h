#pragma once

#include <DirectXMath.h>

namespace Toast {

	class Shader 
	{
	public:
		virtual ~Shader() = default;

		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		virtual void SetData(const std::string& cbName, void* data) = 0;

		virtual const std::string GetName() const = 0;

		static Ref<Shader> Create(const std::string& filepath);
	};

	class ShaderLibrary 
	{
	public:
		static void Add(const std::string name, const Ref<Shader>& shader);
		static void Add(const Ref<Shader>& shader);
		static Ref<Shader> Load(const std::string& filepath);
		static Ref<Shader> Load(const std::string& name, const std::string& filepath);

		static Ref<Shader> Get(const std::string& name);

		static bool Exists(const std::string& name);
	private:
		static std::unordered_map<std::string, Ref<Shader>> mShaders;
	};
}