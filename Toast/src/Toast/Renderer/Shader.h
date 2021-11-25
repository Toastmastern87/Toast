#pragma once

#include "Toast/Renderer/RendererBuffer.h"

#include <d3d11.h>
#include <d3d11shader.h>
#include <d3d11shadertracing.h>
#include <wrl.h>
#include <DirectXMath.h>
#include <string>

namespace Toast {

	enum class ShaderCBufferElementType
	{
		None = 0, Bool, Int, Float, Float2, Float3, Float4, Mat4
	};

	class ShaderCBufferElement
	{
	public:
		ShaderCBufferElement() = default;
		ShaderCBufferElement(std::string name, uint32_t size, uint32_t offset);

		const std::string& GetName() const { return mName; }
		uint32_t GetSize() const { return mSize; }
		uint32_t GetOffset() const { return mOffset; }

		static const std::string& CBufferTypeToString(const ShaderCBufferElementType type);
	private:
		std::string mName;
		uint32_t mSize;
		uint32_t mOffset;
	};

	struct ShaderCBufferBindingDesc
	{
		std::string Name;
		D3D11_SHADER_TYPE ShaderType = D3D11_SHADER_TYPE::D3D11_VERTEX_SHADER;
		uint32_t BindPoint =  0;
		uint32_t Size = 0;
		std::unordered_map<std::string, ShaderCBufferElement> CBufferElements;
	};

	class ShaderLayout
	{
	public:
		struct ShaderInputElement
		{
			std::string mName;
			DXGI_FORMAT mType;
			uint32_t mSize;
			size_t mOffset;
			uint32_t mSemanticIndex;
			D3D11_INPUT_CLASSIFICATION mInputClassification;

			ShaderInputElement() = default;

			ShaderInputElement(DXGI_FORMAT type, const std::string& name, const uint32_t semanticIndex = 0)
				: mName(name), mType(type), mSize(ShaderDataTypeSize(type)), mOffset(0), mSemanticIndex(semanticIndex)
			{
			}

			uint32_t GetComponentCount() const
			{
				switch (mType)
				{
				case DXGI_FORMAT_R32_FLOAT:					return 1;
				case DXGI_FORMAT_R32G32_FLOAT:				return 2;
				case DXGI_FORMAT_R32G32B32_FLOAT:			return 3;
				case DXGI_FORMAT_R32G32B32A32_FLOAT:		return 4;
				case DXGI_FORMAT_R32_UINT:					return 1;
				case DXGI_FORMAT_R32G32_UINT:				return 2;
				case DXGI_FORMAT_R32G32B32_UINT:			return 3;
				case DXGI_FORMAT_R32G32B32A32_UINT:			return 4;
				}

				return 0;
			}
		};

		ShaderLayout(const std::vector<ShaderInputElement>& elements, void* VSRaw);
		virtual ~ShaderLayout();

		void Bind() const;
		void Unbind() const;

		inline uint32_t GetStride() const { return mStride; }
		inline const std::vector<ShaderInputElement>& GetElements() const { return mElements; }

		std::vector<ShaderInputElement>::iterator begin() { return mElements.begin(); }
		std::vector<ShaderInputElement>::iterator end() { return mElements.end(); }
		std::vector<ShaderInputElement>::const_iterator begin() const { return mElements.begin(); }
		std::vector<ShaderInputElement>::const_iterator end() const { return mElements.end(); }

	private:
		virtual void CalculateOffsetAndStride();
		void CalculateSemanticIndex();

	private:
		uint32_t mStride;
		std::vector<ShaderInputElement> mElements;
		Microsoft::WRL::ComPtr<ID3D11InputLayout> mInputLayout;
	};

	class Shader
	{
	public:
		enum class BindingType
		{
			Buffer,
			Texture,
			Sampler
		};

		struct ResourceBindingDesc
		{
			std::string Name			{""};
			D3D11_SHADER_TYPE Shader	{ D3D11_VERTEX_SHADER };
			uint32_t BindPoint			{ 0 };
			BindingType Type			{ BindingType::Buffer };
			uint32_t Size				{ 0 };
			uint32_t Count				{ 0 };
		};

		struct CBufferElementBindingDesc
		{
			std::string Name			{""};
			std::string CBufferName		{""};
			uint32_t Size				{ 0 };
			uint32_t Offset				{ 0 };
		};
	public:
		Shader(const std::string& filepath);
		~Shader();

		void Invalidate(const std::string& filepath);

		void Bind() const;
		void Unbind() const;

		const std::string GetName() const { return mName; }

		const ID3D10Blob* GetVSRaw() const { return mRawBlobs.at(D3D11_VERTEX_SHADER); }

		const std::vector<ResourceBindingDesc> GetResourceBindings() const { return mResourceBindings; }
		const std::vector<CBufferElementBindingDesc> GetCBufferElementBindings(const std::string& cbufferName) const;
		const std::unordered_map<std::string, ShaderCBufferBindingDesc>& GetCBuffersBindings() const { return mCBufferBindings; }
		std::string GetResourceName(BindingType type, uint32_t bindSlot, D3D11_SHADER_TYPE shaderType) const;
	private:
		std::string ReadFile(const std::string& filepath);
		std::unordered_map<D3D11_SHADER_TYPE, std::string> PreProcess(const std::string& source);
		void Compile(const std::unordered_map<D3D11_SHADER_TYPE, std::string> shaderSources);

		void ProcessInputLayout(const std::string& source);
		void ProcessResources();
	private:
		Ref<ShaderLayout> mLayout;

		std::string mName;

		Microsoft::WRL::ComPtr<ID3D11VertexShader> mVertexShader; 
		Microsoft::WRL::ComPtr<ID3D11PixelShader> mPixelShader;
		Microsoft::WRL::ComPtr<ID3D11ComputeShader> mComputeShader;
		std::unordered_map<D3D11_SHADER_TYPE, ID3D10Blob*> mRawBlobs;

		std::vector<ResourceBindingDesc> mResourceBindings;
		std::vector<CBufferElementBindingDesc> mCBufferElementBindings;

		std::unordered_map<std::string, ShaderCBufferBindingDesc> mCBufferBindings;
	};

	class ShaderLibrary 
	{
	public:
		static void Delete(const std::string name);
		static Shader* Load(const std::string& filepath);
		static Shader* Load(const std::string& name, const std::string& filepath);
		static void Reload(const std::string& filepath);

		static Shader* Get(const std::string& name);
		static std::vector<std::string> GetShaderList();

		static bool Exists(const std::string& name);
	private:
		static std::unordered_map<std::string, Scope<Shader>> mShaders;
	};
}