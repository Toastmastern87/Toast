#pragma once

#include "Toast/Core/Timestep.h"

#include "Toast/Renderer/RendererBuffer.h"
#include "Toast/Renderer/Shader.h"
#include "Toast/Renderer/Material.h"
#include "Toast/Renderer/Formats.h"

#include <DirectXMath.h>

#include <../cgltf/include/cgltf.h>

namespace Toast {

	struct PlanetPatch
	{
		int level = 0;
		DirectX::XMFLOAT3 a = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 r = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 s = { 0.0f, 0.0f, 0.0f };

		PlanetPatch(int Level, DirectX::XMFLOAT3 A, DirectX::XMFLOAT3 R, DirectX::XMFLOAT3 S)
		{
			level = Level;
			a = A;
			r = R;
			s = S;
		}
	};

	struct PlanetFace
	{
		DirectX::XMVECTOR A = { 0.0f, 0.0f, 0.0f }, B = { 0.0f, 0.0f, 0.0f }, C = { 0.0f, 0.0f, 0.0f };
		PlanetFace* Parent = nullptr;
		short Level = 0;

		PlanetFace()
		{
		}

		PlanetFace(DirectX::XMVECTOR a, DirectX::XMVECTOR b, DirectX::XMVECTOR c, PlanetFace* parent, short level)
		{
			A = a;
			B = b;
			C = c;

			Parent = parent;

			Level = level;
		}
	};

	struct PlanetVertex
	{
		DirectX::XMFLOAT2 Position = { 0.0f, 0.0f };

		PlanetVertex(DirectX::XMFLOAT2 pos)
		{
			Position = pos;
		}
	};

	struct Vertex
	{
		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT3 Normal;
		DirectX::XMFLOAT4 Tangent;
		DirectX::XMFLOAT2 Texcoord;

		Vertex(DirectX::XMFLOAT3 pos, DirectX::XMFLOAT3 nor, DirectX::XMFLOAT4 tan, DirectX::XMFLOAT2 uv)
		{
			Position = pos;
			Normal = nor;
			Tangent = tan;
			Texcoord = uv;
		}

		Vertex(DirectX::XMFLOAT3 pos)
		{
			Position = pos;
			Normal = { 0.0f, 0.0f, 0.0f };
			Tangent = { 0.0f, 0.0f, 0.0f, 0.0f };
			Texcoord = { 0.0f, 0.0f };
		}

		Vertex()
		{
			Position = { 0.0f, 0.0f, 0.0f };
			Normal = { 0.0f, 0.0f, 0.0f };
			Tangent = { 0.0f, 0.0f, 0.0f, 0.0f };
			Texcoord = { 0.0f, 0.0f };
		}

		Vertex operator+(const Vertex& a)
		{
			return Vertex({ this->Position.x + a.Position.x, this->Position.y + a.Position.y, this->Position.z + a.Position.z });
		}

		Vertex operator*(const float factor)
		{
			return Vertex({ this->Position.x * factor, this->Position.y * factor, this->Position.z * factor });
		}

		Vertex operator/(const float factor)
		{
			return Vertex({ this->Position.x / factor, this->Position.y / factor, this->Position.z / factor });
		}

		Vertex& operator*=(const float factor)
		{
			this->Position.x *= factor;
			this->Position.y *= factor;
			this->Position.z *= factor;

			return *this;
		}

		Vertex operator-(const Vertex& a)
		{
			return Vertex({ this->Position.x - a.Position.x, this->Position.y - a.Position.y, this->Position.z - a.Position.z });
		}
	};

	struct Animation 
	{
		bool IsActive = true;
		float TimeElapsed = 0.0f;
		cgltf_animation_channel AnimationChannel;
		cgltf_animation_sampler AnimationSampler;
		cgltf_accessor SamplerInput;
		cgltf_accessor SamplerOutput;
		cgltf_buffer_view Buffer_View;
		cgltf_buffer BufferData;
		Buffer DataBuffer;

		Animation() = default;
		Animation(cgltf_animation_channel animationChannel, cgltf_animation_sampler animationSampler)
			: AnimationChannel(animationChannel), AnimationSampler(animationSampler) {}
	};

	class Submesh
	{
	public:
		void OnUpdate(Timestep ts);

		uint32_t FindPosition(float animationTime, const std::string& animationName);
		DirectX::XMVECTOR InterpolateTranslation(float animationTime, const std::string& animationName);
	public:
		uint32_t BaseVertex;
		uint32_t BaseIndex;
		std::string MaterialName;
		uint32_t IndexCount;
		uint32_t VertexCount;

		DirectX::XMMATRIX Transform = DirectX::XMMatrixIdentity();
		DirectX::XMFLOAT3 Translation = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
		DirectX::XMFLOAT4 Rotation = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
		DirectX::XMFLOAT3 Scale = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);

		std::string MeshName;

		bool IsAnimated = false;
		std::unordered_map<std::string, Ref<Animation>> Animations;
	};

	class Mesh 
	{
	public:
		enum class MeshType { NONE = 0, MODEL, CUBE, SPHERE, PLANET };
	public:
		Mesh();
		Mesh(const std::string& filePath, const bool skyboxMesh = false);
		Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const DirectX::XMMATRIX& transform);
		~Mesh() = default;

		template <typename T>
		void Set(const std::string& materialName, const std::string& cbufferName, const std::string& name, const T& value)
		{
			auto decl = FindCBufferElementDeclaration(materialName, cbufferName, name);

			TOAST_CORE_ASSERT(decl, "Couldn't find constant buffer element!");
			if (!decl)
				return;

			if (cbufferName == "Model")
				mModelBuffer.Write((byte*)&value, decl->GetSize(), decl->GetOffset());
			else if (cbufferName == "Planet") 
				mPlanetBuffer.Write((byte*)&value, decl->GetSize(), decl->GetOffset());
		}

		template <typename T>
		T& Get(const std::string& materialName, const std::string& cbufferName, const std::string& name)
		{
			auto decl = FindCBufferElementDeclaration(materialName, bufferName, name);
			TOAST_CORE_ASSERT(decl, "Couldn't find constant buffer element!");

			if(cbufferName == "Model")
				return mModelBuffer.Read<T>(decl->GetOffset());
			else if (cbufferName == "Planet")
				return mPlanetBuffer.Read<T>(decl->GetOffset());
		}

		void OnUpdate(Timestep ts);
		void InvalidatePlanet(bool patchGeometryRebuilt);

		const std::string& GetFilePath() const { return mFilePath; }

		std::vector<Vertex>& GetVertices() { return mVertices; }
		std::vector<uint32_t>& GetIndices() { return mIndices; }
		std::vector<PlanetVertex>& GetPlanetVertices() { return mPlanetVertices; }
		std::vector<PlanetPatch>& GetPlanetPatches() { return mPlanetPatches; }


		std::vector<Submesh>& GetSubmeshes() { return mSubmeshes; }
		void AddSubmesh(uint32_t indexCount);
		uint32_t GetNumberOfSubmeshes() { return mSubmeshes.size(); }

		const Ref<Material> GetMaterial(std::string materialName) const { if (mMaterials.find(materialName) != mMaterials.end()) return mMaterials.at(materialName); else return nullptr; }
		void SetMaterial(std::string materialName, Ref<Material> material) { mMaterials[materialName] = material; TOAST_CORE_INFO("ADDING MATERIAL, Number of materials: %d", mMaterials.size()); }

		std::vector<PlanetFace>& GetPlanetFaces() { return mPlanetFaces; }

		DirectX::XMMATRIX& GetLocalTransform() { return mSubmeshes[0].Transform; }
		void SetLocalTransform(DirectX::XMMATRIX& transform) { mSubmeshes[0].Transform = transform; }

		void Map(const std::string& materialName);
		void Bind(const std::string& materialName, bool environment = true);

		bool GetIsPlanet() const { return mIsPlanet; }
		void SetIsPlanet(bool isPlanet);

		bool GetIsAnimated() const { return mIsAnimated; }
	private:
		const ShaderCBufferElement* FindCBufferElementDeclaration(const std::string& materialName, const std::string& cbufferName, const std::string& name);
	private:
		std::string mFilePath = "";

		DirectX::XMMATRIX mTransform = DirectX::XMMatrixIdentity();

		std::vector<Submesh> mSubmeshes;

		Ref<VertexBuffer> mVertexBuffer;
		Ref<VertexBuffer> mInstanceVertexBuffer;
		Ref<IndexBuffer> mIndexBuffer;
		std::unordered_map<std::string, Ref<Material>> mMaterials;

		uint32_t mVertexCount = 0;
		uint32_t mIndexCount = 0;

		std::vector<Vertex> mVertices;

		std::vector<PlanetVertex> mPlanetVertices = {};
		std::vector<PlanetFace> mPlanetFaces = {};
		std::vector<PlanetPatch> mPlanetPatches = {};

		std::vector<uint32_t> mIndices;

		PrimitiveTopology mTopology = PrimitiveTopology::TRIANGLELIST;

		Ref<ConstantBuffer> mModelCBuffer, mPlanetCBuffer;
		Buffer mModelBuffer, mPlanetBuffer;

		bool mIsPlanet = false;
		bool mIsAnimated = false;

		friend class Scene;
		friend class Renderer;
		friend class RendererDebug;
		friend class Primitives;
		friend class SceneHierarchyPanel;
		friend class PropertiesPanel;
		friend class ScriptWrappers;
	};
}