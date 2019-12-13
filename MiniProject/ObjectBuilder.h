
#pragma once

#include <cstdint>
#include <DirectXMath.h>
#include <vector>

using namespace DirectX;
using namespace std;


class ObjectBuilder
{
public:

	struct Vertex
	{
		Vertex() {}
		Vertex(float px, float py, float pz, float nx, float ny, float nz) : Position(px, py, pz), Normal(nx, ny, nz) {}

		XMFLOAT3 Position;
		XMFLOAT3 Normal;
	};

	struct MeshData
	{
		vector<Vertex> Vertices;
		vector<uint32_t> Indices32;
	};

	// Creates a box centered at the origin with the given dimensions, where eachface has m rows and n columns of vertices.
	MeshData CreateBox(float width, float height, float depth);

	MeshData CreatePyramid(float width, float depth, float height);

	// Creates an MxN grid in the xz-plane with m rows and n columns, centered at the origin with the specified width and depth.
	MeshData CreateGrid(float width, float depth, uint32_t m, uint32_t n);

};

