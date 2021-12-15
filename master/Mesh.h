#pragma once

#include <d3d12.h>
#include <wrl/client.h>

#include "Vertex.h"

class Mesh
{
public:
	Mesh(Vertex* vertexArray, int numVertices, unsigned int* indexArray, int numIndices);
	Mesh(const char* objFile);

	D3D12_VERTEX_BUFFER_VIEW GetVB() { return vbView; }
	D3D12_INDEX_BUFFER_VIEW GetIB() { return ibView; }
	int GetIndexCount() { return numIndices; }

private:
	int numIndices;
	D3D12_VERTEX_BUFFER_VIEW vbView;
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
	
	D3D12_INDEX_BUFFER_VIEW ibView;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;

	void CalculateTangents(Vertex* vertexArray, int numVertices, unsigned int* indexArray, int numIndices);
	void CreateBuffers(Vertex* vertexArray, int numVertices, unsigned int* indexArray, int numIndices);
};

