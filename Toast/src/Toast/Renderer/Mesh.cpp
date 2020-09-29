#include "tpch.h"
#include "Mesh.h"

namespace Toast {

	Mesh::Mesh()
	{
		mMeshShader = ShaderLibrary::Load("assets/shaders/Mesh.hlsl");

		const std::initializer_list<BufferLayout::BufferElement>& layout = {
														   { ShaderDataType::Float3, "POSITION" },
														   { ShaderDataType::Float3, "NORMAL" },
														   { ShaderDataType::Float3, "TANGENT" },
														   { ShaderDataType::Float3, "BINORMAL" },
														   { ShaderDataType::Float2, "TEXCOORD" }
		};

		mLayout = BufferLayout::Create(layout, mMeshShader);
	}

	Mesh::~Mesh()
	{

	}

	void Mesh::OnUpdate(Timestep ts)
	{

	}

	void Mesh::CreateFromFile()
	{

	}

	void Mesh::CreateFromPrimitive()
	{
		PrimitiveMesh mesh;

		switch (mPrimitiveType)
		{
		case PrimitiveType::PLANE:
			// TODO
			break;
		case PrimitiveType::CUBE:
			mesh = Primitives::CreateCube();

			mVertexBuffer = VertexBuffer::Create(&mesh.Vertices[0], (sizeof(Vertex) * (uint32_t)mesh.Vertices.size()), (uint32_t)mesh.Vertices.size());
			mIndexBuffer = IndexBuffer::Create(&mesh.Indices[0], (uint32_t)mesh.Indices.size());

			mMeshActive = true;

			break;
		case PrimitiveType::ICOSPHERE:
			// TODO
			break;
		case PrimitiveType::GRID:
			mesh = Primitives::CreateGrid();

			mVertexBuffer = VertexBuffer::Create(&mesh.Vertices[0], (sizeof(Vertex) * (uint32_t)mesh.Vertices.size()), (uint32_t)mesh.Vertices.size());
			mIndexBuffer = IndexBuffer::Create(&mesh.Indices[0], (uint32_t)mesh.Indices.size());

			mMeshActive = true;

			break;
		}
	}
}