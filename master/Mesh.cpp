#include "Mesh.h"
#include "DX12Helper.h"

Mesh::Mesh(Vertex* vertexArray, int numVertices, unsigned int* indexArray, int numIndices)
{
}

Mesh::Mesh(const char* objFile)
{
}

void Mesh::CalculateTangents(Vertex* vertices, int numVertices, unsigned int* indices, int numIndices)
{
}

void Mesh::CreateBuffers(Vertex* vertexArray, int numVertices, unsigned int* indexArray, int numIndices)
{
	//Save the index count
	this->numIndices = numIndices;
	
	//Calculate the tangents before copying to buffer
	CalculateTangents(vertexArray, numVertices, indexArray, numIndices);

	vertexBuffer = DX12Helper::GetInstance().CreateStaticBuffer(sizeof(Vertex), numVertices, vertexArray);
	indexBuffer = DX12Helper::GetInstance().CreateStaticBuffer(sizeof(unsigned int), numIndices, indexArray);

	// Set up the views
	vbView.StrideInBytes = sizeof(Vertex);
	vbView.SizeInBytes = sizeof(Vertex) * numVertices;
	vbView.BufferLocation = vertexBuffer->GetGPUVirtualAddress(); //GPU address only, meaningless in C++

	ibView.Format = DXGI_FORMAT_R32_UINT;
	ibView.SizeInBytes = sizeof(unsigned int) * numIndices;
	ibView.BufferLocation = indexBuffer->GetGPUVirtualAddress();

}
