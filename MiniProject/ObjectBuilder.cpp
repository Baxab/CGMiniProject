
#include "ObjectBuilder.h"
#include <algorithm>

using namespace DirectX;
using namespace std;


ObjectBuilder::MeshData ObjectBuilder::CreateBox(float width, float height, float depth)
{
	MeshData meshData;

	Vertex v[24];

	float w2 = 0.5f*width;
	float h2 = 0.5f*height;
	float d2 = 0.5f*depth;

	// Fill in the front face vertex data.
	v[0] = Vertex(-w2, -h2, -d2, 0.0f, 0.0f, -1.0f);
	v[1] = Vertex(-w2, +h2, -d2, 0.0f, 0.0f, -1.0f);
	v[2] = Vertex(+w2, +h2, -d2, 0.0f, 0.0f, -1.0f);
	v[3] = Vertex(+w2, -h2, -d2, 0.0f, 0.0f, -1.0f);

	// Fill in the back face vertex data.
	v[4] = Vertex(-w2, -h2, +d2, 0.0f, 0.0f, 1.0f);
	v[5] = Vertex(+w2, -h2, +d2, 0.0f, 0.0f, 1.0f);
	v[6] = Vertex(+w2, +h2, +d2, 0.0f, 0.0f, 1.0f);
	v[7] = Vertex(-w2, +h2, +d2, 0.0f, 0.0f, 1.0f);

	// Fill in the top face vertex data.
	v[8] = Vertex(-w2, +h2, -d2, 0.0f, 1.0f, 0.0f);
	v[9] = Vertex(-w2, +h2, +d2, 0.0f, 1.0f, 0.0f);
	v[10] = Vertex(+w2, +h2, +d2, 0.0f, 1.0f, 0.0f);
	v[11] = Vertex(+w2, +h2, -d2, 0.0f, 1.0f, 0.0f);

	// Fill in the bottom face vertex data.
	v[12] = Vertex(-w2, -h2, -d2, 0.0f, -1.0f, 0.0f);
	v[13] = Vertex(+w2, -h2, -d2, 0.0f, -1.0f, 0.0f);
	v[14] = Vertex(+w2, -h2, +d2, 0.0f, -1.0f, 0.0f);
	v[15] = Vertex(-w2, -h2, +d2, 0.0f, -1.0f, 0.0f);

	// Fill in the left face vertex data.
	v[16] = Vertex(-w2, -h2, +d2, -1.0f, 0.0f, 0.0f);
	v[17] = Vertex(-w2, +h2, +d2, -1.0f, 0.0f, 0.0f);
	v[18] = Vertex(-w2, +h2, -d2, -1.0f, 0.0f, 0.0f);
	v[19] = Vertex(-w2, -h2, -d2, -1.0f, 0.0f, 0.0f);

	// Fill in the right face vertex data.
	v[20] = Vertex(+w2, -h2, -d2, 1.0f, 0.0f, 0.0f);
	v[21] = Vertex(+w2, +h2, -d2, 1.0f, 0.0f, 0.0f);
	v[22] = Vertex(+w2, +h2, +d2, 1.0f, 0.0f, 0.0f);
	v[23] = Vertex(+w2, -h2, +d2, 1.0f, 0.0f, 0.0f);


	meshData.Vertices.assign(&v[0], &v[24]);

	uint32_t i[36];

	// Fill in the front face index data
	i[0] = 0; i[1] = 1; i[2] = 2;
	i[3] = 0; i[4] = 2; i[5] = 3;

	// Fill in the back face index data
	i[6] = 4; i[7] = 5; i[8] = 6;
	i[9] = 4; i[10] = 6; i[11] = 7;

	// Fill in the top face index data
	i[12] = 8; i[13] = 9; i[14] = 10;
	i[15] = 8; i[16] = 10; i[17] = 11;

	// Fill in the bottom face index data
	i[18] = 12; i[19] = 13; i[20] = 14;
	i[21] = 12; i[22] = 14; i[23] = 15;

	// Fill in the left face index data
	i[24] = 16; i[25] = 17; i[26] = 18;
	i[27] = 16; i[28] = 18; i[29] = 19;

	// Fill in the right face index data
	i[30] = 20; i[31] = 21; i[32] = 22;
	i[33] = 20; i[34] = 22; i[35] = 23;

	meshData.Indices32.assign(&i[0], &i[36]);

	return meshData;
}

ObjectBuilder::MeshData ObjectBuilder::CreatePyramid(float width, float depth, float height)
{
	MeshData meshData;

	Vertex v[16];

	float w2 = 0.5f*width;
	float d2 = 0.5f*depth;

	XMFLOAT3 v1 = {0, height, 0};
	XMFLOAT3 v2 = {w2, 0, -d2};
	XMFLOAT3 v3 = {0,0,d2};
	XMVECTOR v33 = XMLoadFloat3(&v3);
	XMVECTOR v11 = XMLoadFloat3(&v1);
	XMVECTOR v22 = XMLoadFloat3(&v2);
	XMVECTOR vect1 = XMVectorSubtract(v22, v11);
	XMVECTOR n1 = XMVector3Cross(v33, vect1);
	XMVECTOR norm_1 = XMVector3Normalize(n1);
	XMFLOAT3 normale;
	XMStoreFloat3(&normale, norm_1);


	v[0] = Vertex(0, height, 0, normale.x, normale.y, normale.z);
	v[1] = Vertex(w2, 0, d2, normale.x, normale.y, normale.z);
	v[2] = Vertex(w2, 0, -d2, normale.x, normale.y, normale.z);
	
	v[3] = Vertex(0, height, 0, normale.z, normale.y, -normale.x);
	v[4] = Vertex(w2, 0, -d2, normale.z, normale.y, -normale.x);
	v[5] = Vertex(-w2, 0, -d2, normale.z, normale.y, -normale.x);
	
	v[6] = Vertex(0, height, 0, -normale.x, normale.y, normale.z);
	v[7] = Vertex(-w2, 0, -d2, -normale.x, normale.y, normale.z);
	v[8] = Vertex(-w2, 0, d2, -normale.x, normale.y, normale.z);
	
	v[9] = Vertex(0, height, 0, normale.z, normale.y, normale.x);
	v[10] = Vertex(-w2, 0, d2, normale.z, normale.y, normale.x);
	v[11] = Vertex(w2, 0, d2, normale.z, normale.y, normale.x);

	v[12] = Vertex(-w2, 0, -d2, 0.0f, -1.0f, 0.0f);
	v[13] = Vertex(-w2, 0, d2, 0.0f, -1.0f, 0.0f);
	v[14] = Vertex(w2, 0, d2, 0.0f, -1.0f, 0.0f);
	v[15] = Vertex(w2, 0, -d2, 0.0f, -1.0f, 0.0f);



	meshData.Vertices.assign(&v[0], &v[16]);

	uint32_t i[18];

	// Fill in the front face index data
	i[0] = 0; i[1] = 1; i[2] = 2;
	i[3] = 3; i[4] = 4; i[5] = 5;

	// Fill in the back face index data
	i[6] = 6; i[7] = 7; i[8] = 8;
	i[9] = 9; i[10] = 10; i[11] = 11;

	// Fill in the top face index data
	i[12] = 12; i[13] = 15; i[14] = 14;
	i[15] = 12; i[16] = 14; i[17] = 13;

	meshData.Indices32.assign(&i[0], &i[18]);

	return meshData;
}



ObjectBuilder::MeshData ObjectBuilder::CreateGrid(float width, float depth, uint32_t m, uint32_t n)
{
	MeshData meshData;

	uint32_t vertexCount = m*n;
	uint32_t faceCount = (m - 1)*(n - 1) * 2;

	//
	// Create the vertices.
	//

	float halfWidth = 0.5f*width;
	float halfDepth = 0.5f*depth;

	float dx = width / (n - 1);
	float dz = depth / (m - 1);

	float du = 1.0f / (n - 1);
	float dv = 1.0f / (m - 1);

	meshData.Vertices.resize(vertexCount);
	for (uint32_t i = 0; i < m; ++i)
	{
		float z = halfDepth - i*dz;
		for (uint32_t j = 0; j < n; ++j)
		{
			float x = -halfWidth + j*dx;
			meshData.Vertices[i*n + j].Position = XMFLOAT3(x, 0.0f, z);
			meshData.Vertices[i*n + j].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
		}
	}

	// Create the indices.
	meshData.Indices32.resize(faceCount * 3); // 3 indices per face

	// Iterate over each quad and compute indices.
	uint32_t k = 0;
	for (uint32_t i = 0; i < m - 1; ++i)
	{
		for (uint32_t j = 0; j < n - 1; ++j)
		{
			meshData.Indices32[k] = i*n + j;
			meshData.Indices32[k + 1] = i*n + j + 1;
			meshData.Indices32[k + 2] = (i + 1)*n + j;

			meshData.Indices32[k + 3] = (i + 1)*n + j;
			meshData.Indices32[k + 4] = i*n + j + 1;
			meshData.Indices32[k + 5] = (i + 1)*n + j + 1;

			k += 6; // next quad
		}
	}

	return meshData;
}
