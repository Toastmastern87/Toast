#include "tpch.h"
#include "Mesh.h"

namespace Toast {

	Mesh::Mesh()
	{
		mMaterial = MaterialLibrary::Get("Standard");
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

	void Mesh::CreateFromPrimitive(float width, float height, float depth)
	{
		PrimitiveMesh mesh;

		switch (mPrimitiveType)
		{
		case PrimitiveType::PLANET:
			// TODO
			break;
		case PrimitiveType::CUBE:
			mesh = Primitives::CreateCube(width, height, depth);

			mVertexBuffer = CreateRef<VertexBuffer>(&mesh.Vertices[0], (sizeof(Vertex) * (uint32_t)mesh.Vertices.size()), (uint32_t)mesh.Vertices.size());
			mIndexBuffer = CreateRef<IndexBuffer>(&mesh.Indices[0], (uint32_t)mesh.Indices.size());

			mMeshActive = true;

			break;
		case PrimitiveType::ICOSPHERE:
			mesh = Primitives::CreateIcosphere(mIcosphereDivision);

			mVertexBuffer = CreateRef<VertexBuffer>(&mesh.Vertices[0], (sizeof(Vertex) * (uint32_t)mesh.Vertices.size()), (uint32_t)mesh.Vertices.size());
			mIndexBuffer = CreateRef<IndexBuffer>(&mesh.Indices[0], (uint32_t)mesh.Indices.size());

			mMeshActive = true;

			break;
		case PrimitiveType::GRID:
			mVertexBuffer = nullptr;
			mIndexBuffer = nullptr;

			mesh = Primitives::CreateGrid(mGridSize);

			mVertexBuffer = CreateRef<VertexBuffer>(&mesh.Vertices[0], (sizeof(Vertex) * (uint32_t)mesh.Vertices.size()), (uint32_t)mesh.Vertices.size());
			mIndexBuffer = CreateRef<IndexBuffer>(&mesh.Indices[0], (uint32_t)mesh.Indices.size());

			mMeshActive = true;

			break;
		}
	}
}