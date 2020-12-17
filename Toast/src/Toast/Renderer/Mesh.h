#pragma once

#include "Toast/Core/Timestep.h"

#include "Toast/Renderer/Buffer.h"
#include "Toast/Renderer/Shader.h"
#include "Toast/Renderer/Material.h"

#include "Toast/Renderer/Formats.h"

#include <DirectXMath.h>

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
		DirectX::XMFLOAT2 Morphing = { 0.0f, 0.0f };

		PlanetVertex(DirectX::XMFLOAT2 pos, DirectX::XMFLOAT2 morph)
		{
			Position = pos;
			Morphing = morph;
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

		std::string MeshName;
	};

	class Mesh 
	{
	public:
		enum class MeshType { NONE = 0, MODEL, CUBE, SPHERE, PLANET };
	public:
		Mesh();
		~Mesh();

		void OnUpdate(Timestep ts);
		void Init();
		void InitPlanet();

		std::vector<Vertex>& GetVertices() { return mVertices; }
		std::vector<PlanetVertex>& GetPlanetVertices() { return mPlanetVertices; }
		std::vector<PlanetPatch>& GetPlanetPatches() { return mPlanetPatches; }
		std::vector<uint32_t>& GetIndices() { return mIndices; }

		void CreateFromFile();
		void AddSubmesh(uint32_t indexCount);

		const Ref<Material> GetMaterial() const { return mMaterial; }
		void SetMaterial(Ref<Material>& material) { mMaterial = material; }

	private:
		std::vector<Submesh> mSubmeshes;

		Ref<VertexBuffer> mVertexBuffer;
		Ref<VertexBuffer> mInstanceVertexBuffer;
		Ref<IndexBuffer> mIndexBuffer;
		Ref<Material> mMaterial;

		uint32_t mVertexCount = 0;
		uint32_t mIndexCount = 0;

		std::vector<Vertex> mVertices;

		std::vector<PlanetVertex> mPlanetVertices = {};
		std::vector<PlanetFace> mPlanetFaces = {};
		std::vector<PlanetPatch> mPlanetPatches = {};

		std::vector<uint32_t> mIndices;

		PrimitiveTopology mTopology = PrimitiveTopology::TRIANGLELIST;

		friend class Scene;
		friend class Renderer;
		friend class Primitives;
		friend class SceneHierarchyPanel;
	};
}