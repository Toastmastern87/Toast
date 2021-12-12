#pragma once

#include "Toast/Core/Timestep.h"

#include "Toast/Renderer/RendererBuffer.h"
#include "Toast/Renderer/Shader.h"
#include "Toast/Renderer/Material.h"
#include "Toast/Renderer/Formats.h"

#include <DirectXMath.h>

#pragma warning(push, 0)
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>
#pragma warning(pop)

struct aiScene;

namespace Assimp {
	class Importer;
}

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
		DirectX::XMFLOAT3 Tangent;
		DirectX::XMFLOAT3 Binormal;
		DirectX::XMFLOAT2 Texcoord;

		Vertex(DirectX::XMFLOAT3 pos, DirectX::XMFLOAT3 nor, DirectX::XMFLOAT3 tan, DirectX::XMFLOAT3 bin, DirectX::XMFLOAT2 uv)
		{
			Position = pos;
			Normal = nor;
			Tangent = tan;
			Binormal = bin;
			Texcoord = uv;
		}

		Vertex(DirectX::XMFLOAT3 pos)
		{
			Position = pos;
			Normal = { 0.0f, 0.0f, 0.0f };
			Tangent = { 0.0f, 0.0f, 0.0f };
			Binormal = { 0.0f, 0.0f, 0.0f };
			Texcoord = { 0.0f, 0.0f };
		}

		Vertex()
		{
			Position = { 0.0f, 0.0f, 0.0f };
			Normal = { 0.0f, 0.0f, 0.0f };
			Tangent = { 0.0f, 0.0f, 0.0f };
			Binormal = { 0.0f, 0.0f, 0.0f };
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

	class Submesh
	{
	public:
		uint32_t BaseVertex;
		uint32_t BaseIndex;
		uint32_t MaterialIndex;
		uint32_t IndexCount;
		uint32_t VertexCount;

		DirectX::XMMATRIX Transform = DirectX::XMMatrixIdentity();

		std::string MeshName;
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
		void Set(const std::string& cbufferName, const std::string& name, const T& value)
		{
			auto decl = FindCBufferElementDeclaration(cbufferName, name);

			TOAST_CORE_ASSERT(decl, "Couldn't find constant buffer element!");
			if (!decl)
				return;

			if (cbufferName == "Model")
				mModelBuffer.Write((byte*)&value, decl->GetSize(), decl->GetOffset());
			else if (cbufferName == "Planet")
				mPlanetBuffer.Write((byte*)&value, decl->GetSize(), decl->GetOffset());
			else if (cbufferName == "PlanetPS")
				mPlanetPSBuffer.Write((byte*)&value, decl->GetSize(), decl->GetOffset());
		}

		template <typename T>
		T& Get(const std::string& cbufferName, const std::string& name)
		{
			auto decl = FindCBufferElementDeclaration(bufferName, name);
			TOAST_CORE_ASSERT(decl, "Couldn't find constant buffer element!");

			if(cbufferName == "Model")
				return mModelBuffer.Read<T>(decl->GetOffset());
			else if (cbufferName == "Planet")
				return mPlanetBuffer.Read<T>(decl->GetOffset());
			else if (cbufferName == "PlanetPS")
				return mPlanetPSBuffer.Read<T>(decl->GetOffset());
		}

		void OnUpdate(Timestep ts);
		void InitPlanet();

		const std::string& GetFilePath() const { return mFilePath; }

		std::vector<Vertex>& GetVertices() { return mVertices; }
		std::vector<uint32_t>& GetIndices() { return mIndices; }
		std::vector<PlanetVertex>& GetPlanetVertices() { return mPlanetVertices; }
		std::vector<PlanetPatch>& GetPlanetPatches() { return mPlanetPatches; }

		void CreateFromFile();
		void AddSubmesh(uint32_t indexCount);

		void GeneratePlanetMesh(DirectX::XMMATRIX planetTransform, DirectX::XMVECTOR& cameraPos, int16_t subdivisions);

		const Ref<Material> GetMaterial() const { return mMaterial; }
		void SetMaterial(Ref<Material> material) { mMaterial = material; }

		std::vector<PlanetFace>& GetPlanetFaces() { return mPlanetFaces; }

		void TraverseNodes(aiNode* node, const DirectX::XMMATRIX& parentTransform = DirectX::XMMatrixIdentity(), uint32_t level = 0);

		DirectX::XMMATRIX& GetLocalTransform() { return mSubmeshes[0].Transform; }
		void SetLocalTransform(DirectX::XMMATRIX& transform) { mSubmeshes[0].Transform = transform; }

		void Map();
		void Bind();

		bool GetIsPlanet() const { return mIsPlanet; }
		void SetIsPlanet(bool isPlanet) { mIsPlanet = isPlanet;	}
	private:
		const ShaderCBufferElement* FindCBufferElementDeclaration(const std::string& cbufferName, const std::string& name);
	private:
		std::string mFilePath = "";

		std::unique_ptr<Assimp::Importer> mImporter = nullptr;
		const aiScene* mScene = nullptr;
		std::unordered_map<aiNode*, std::vector<uint32_t>> mNodeMap;

		DirectX::XMMATRIX mTransform = DirectX::XMMatrixIdentity();

		std::vector<Submesh> mSubmeshes;

		Ref<VertexBuffer> mVertexBuffer;
		Ref<VertexBuffer> mInstanceVertexBuffer;
		Ref<IndexBuffer> mIndexBuffer;
		Ref<Material> mMaterial = nullptr;

		uint32_t mVertexCount = 0;
		uint32_t mIndexCount = 0;

		std::vector<Vertex> mVertices;

		std::vector<PlanetVertex> mPlanetVertices = {};
		std::vector<PlanetFace> mPlanetFaces = {};
		std::vector<PlanetPatch> mPlanetPatches = {};

		std::vector<uint32_t> mIndices;

		PrimitiveTopology mTopology = PrimitiveTopology::TRIANGLELIST;

		Ref<ConstantBuffer> mModelCBuffer, mPlanetCBuffer, mPlanetPSCBuffer;
		Buffer mModelBuffer, mPlanetBuffer, mPlanetPSBuffer;

		bool mIsPlanet = false;

		friend class Scene;
		friend class Renderer;
		friend class RendererDebug;
		friend class Primitives;
		friend class SceneHierarchyPanel;
		friend class PropertiesPanel;
		friend class ScriptWrappers;
	};
}