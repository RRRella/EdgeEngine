#pragma once

#include "../Renderer/Renderer.h"

#include <string>
#include <vector>


enum EBuiltInMeshes
{
	TRIANGLE = 0,
	QUAD,
	CUBE,
	SPHERE,
	CYLINDER,
	CONE,
	
	NUM_BUILTIN_MESHES
};

struct VertexIndexBufferIDPair
{
	BufferID  mVertexBufferID = -1;
	BufferID  mIndexBufferID  = -1;

	inline std::pair<BufferID, BufferID> GetIABufferPair() const { return std::make_pair(mVertexBufferID, mIndexBufferID); }
};

template<class TVertex>
struct MeshLODData
{
	MeshLODData< TVertex >() = delete;
	MeshLODData< TVertex >(int numLODs, const char* pMeshName)
		: LODVertices(numLODs)
		, LODIndices(numLODs)
		, meshName(pMeshName)
	{}
	
	std::vector<std::vector<TVertex>> LODVertices;
	std::vector<std::vector<unsigned>> LODIndices ;
	std::string meshName;
};

struct Mesh
{
public:
	//
	// Constructors / Operators
	//
	template<class TVertex>
	Mesh(
		Renderer* pRenderer,
		const std::vector<TVertex>&  vertices,
		const std::vector<unsigned>& indices,
		const std::string&           name
	);

	template<class TVertex>
	Mesh(const MeshLODData<TVertex>& meshLODData);

	Mesh() = default;
	// Mesh() = delete;
	// Mesh(const Mesh&) = delete; // Model.cpp uses copy
	//Mesh(const Mesh&&) = delete;
	//void operator=(const Mesh&) = delete;


	//
	// Interface
	//
	std::pair<BufferID, BufferID> GetIABuffers(int lod = 0) const;

	
private:
	std::vector<VertexIndexBufferIDPair> mLODBufferPairs;
};

//
// Template Definitions
//
template<class TVertex>
Mesh::Mesh(
	Renderer* pRenderer,
	const std::vector<TVertex>& vertices,
	const std::vector<unsigned>& indices,
	const std::string& name
)
{
	FBufferDesc bufferDesc = {};

	const std::string VBName = name + "_LOD[0]_VB";
	const std::string IBName = name + "_LOD[0]_IB";

	bufferDesc.Type         = VERTEX_BUFFER;
	//bufferDesc.Usage        = GPU_READ_WRITE;
	bufferDesc.NumElements  = static_cast<unsigned>(vertices.size());
	bufferDesc.Stride       = sizeof(vertices[0]);
	bufferDesc.pData        = static_cast<const void*>(vertices.data());
	bufferDesc.Name         = VBName;
	BufferID vertexBufferID = pRenderer->CreateBuffer(bufferDesc);

	bufferDesc.Type        = INDEX_BUFFER;
	//bufferDesc.Usage       = GPU_READ_WRITE;
	bufferDesc.NumElements = static_cast<unsigned>(indices.size());
	bufferDesc.Stride      = sizeof(unsigned);
	bufferDesc.pData       = static_cast<const void*>(indices.data());
	BufferID indexBufferID = pRenderer->CreateBuffer(bufferDesc);

	mLODBufferPairs.push_back({ vertexBufferID, indexBufferID }); // LOD[0]
}

template<class TVertex>
Mesh::Mesh(const MeshLODData<TVertex>& meshLODData)
{
	for (size_t LOD = 0; LOD < meshLODData.LODVertices.size(); ++LOD)
	{
		FBufferDesc bufferDesc = {};

		const std::string VBName = meshLODData.meshName + "_LOD[" + std::to_string(LOD) + "]_VB";
		const std::string IBName = meshLODData.meshName + "_LOD[" + std::to_string(LOD) + "]_IB";

		bufferDesc.mType = VERTEX_BUFFER;
		//bufferDesc.mUsage = GPU_READ_WRITE;
		bufferDesc.mElementCount = static_cast<unsigned>(meshLODData.LODVertices[LOD].size());
		bufferDesc.mStride = sizeof(meshLODData.LODVertices[LOD][0]);
		BufferID vertexBufferID = spRenderer->CreateBuffer(bufferDesc, meshLODData.LODVertices[LOD].data(), VBName.c_str());

		bufferDesc.mType = INDEX_BUFFER;
		//bufferDesc.mUsage = GPU_READ_WRITE;
		bufferDesc.mElementCount = static_cast<unsigned>(meshLODData.LODIndices[LOD].size());
		bufferDesc.mStride = sizeof(unsigned);
		BufferID indexBufferID = spRenderer->CreateBuffer(bufferDesc, meshLODData.LODIndices[LOD].data(), IBName.c_str());

		mLODBufferPairs.push_back({ vertexBufferID, indexBufferID });
	}
}
