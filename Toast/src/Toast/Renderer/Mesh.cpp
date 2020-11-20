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

	void Mesh::Init()
	{
		mVertexBuffer = CreateRef<VertexBuffer>(&mVertices[0], (sizeof(Vertex) * (uint32_t)mVertices.size()), (uint32_t)mVertices.size(), 0);
		mIndexBuffer = CreateRef<IndexBuffer>(&mIndices[0], (uint32_t)mIndices.size());
	}

	void Mesh::InitPlanet()
	{
		mSubmeshes.clear();
		mVertexBuffer = nullptr;
		mInstanceVertexBuffer = nullptr;
		mIndexBuffer = nullptr;

		mVertexBuffer = CreateRef<VertexBuffer>(&mPlanetVertices[0], (sizeof(PlanetVertex) * (uint32_t)mPlanetVertices.size()), (uint32_t)mPlanetVertices.size(), 0);
		mInstanceVertexBuffer = CreateRef<VertexBuffer>(sizeof(PlanetPatch) * 100000, 100000, 1);
		mIndexBuffer = CreateRef<IndexBuffer>(&mIndices[0], (uint32_t)mIndices.size());

		mInstanceVertexBuffer->SetData(&mPlanetPatches[0], (sizeof(PlanetPatch) * mPlanetPatches.size()));
	}

	void Mesh::OnUpdate(Timestep ts)
	{

	}

	void Mesh::CreateFromFile()
	{

	}

	void Mesh::AddSubmesh(uint32_t indexCount)
	{
		Submesh& submesh = mSubmeshes.emplace_back();
		submesh.BaseVertex = mVertexCount;
		submesh.BaseIndex = mIndexCount;
		submesh.MaterialIndex = 0;
		submesh.IndexCount = indexCount;
	}

}