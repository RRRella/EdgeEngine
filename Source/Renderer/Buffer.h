#pragma once

#include "ResourceHeaps.h"
#include "Common.h"

#include <mutex>

struct ID3D12Device;
struct ID3D12Resource;
struct D3D12_CONSTANT_BUFFER_VIEW_DESC;
struct ID3D12GraphicsCommandList;

//
// BUFFER DESCRIPTOR
//
enum EBufferType
{
    VERTEX_BUFFER = 0,
    INDEX_BUFFER,
    CONSTANT_BUFFER,
    NUM_BUFFER_TYPES
};
struct FBufferDesc
{
    EBufferType Type;
    uint        NumElements;
    uint        Stride;
    const void* pData;
    std::string Name;
};


//
// VERTEX BUFFER DEFINITIONS
//
enum EVertexBufferType
{
    DEFAULT = 0,
    COLOR,
    COLOR_AND_ALPHA,
    NORMAL,
    NORMAL_AND_TANGENT,

    NUM_VERTEX_BUFFER_TYPES
};
struct FVertexDefault
{
    float position[3];
    float uv[2];
};
struct FVertexWithColor
{
    float position[3];
    float color[3];
    float uv[2];
};
struct FVertexWithColorAndAlpha
{
    float position[3];
    float color[4];
    float uv[2];
};
struct FVertexWithNormal
{
    float position[3];
    float normal[3];
    float uv[2];
};
struct FVertexWithNormalAndTangent
{
    float position[3];
    float normal[3];
    float tangent[3];
    float uv[2];
};


class StaticBufferHeap
{
    static size_t MEMORY_ALIGNMENT; // TODO: potentially move to renderer settings ini or make a member
public:
    void Create(ID3D12Device* pDevice, EBufferType type, uint32 totalMemSize, bool bUseVidMem, const char* name);
    void Destroy();

    bool AllocBuffer      (uint32 numElements, uint32 strideInBytes, const void* pInitData, D3D12_GPU_VIRTUAL_ADDRESS* pBufferLocationOut, uint32* pSizeOut);
    bool AllocVertexBuffer(uint32 numVertices, uint32 strideInBytes, const void* pInitData, D3D12_VERTEX_BUFFER_VIEW* pViewOut);
    bool AllocIndexBuffer (uint32 numIndices , uint32 strideInBytes, const void* pInitData, D3D12_INDEX_BUFFER_VIEW* pOut);
    //bool AllocConstantBuffer(uint32 size, void* pData, D3D12_CONSTANT_BUFFER_VIEW_DESC* pViewDesc);

    void UploadData(ID3D12GraphicsCommandList* pCmdList);
    //void FreeUploadHeap();

private:
    bool AllocBuffer      (uint32 numElements, uint32 strideInBytes, void** ppDataOut, D3D12_GPU_VIRTUAL_ADDRESS* pBufferLocationOut, uint32* pSizeOut);
    bool AllocVertexBuffer(uint32 numVertices, uint32 strideInBytes, void** ppDataOut, D3D12_VERTEX_BUFFER_VIEW* pViewOut);
    bool AllocIndexBuffer (uint32 numIndices , uint32 strideInBytes, void** ppDataOut, D3D12_INDEX_BUFFER_VIEW* pIndexView);
    //bool AllocConstantBuffer(uint32 size, void** pData, D3D12_CONSTANT_BUFFER_VIEW_DESC* pViewDesc);

private:
    ID3D12Device*  mpDevice = nullptr;
    EBufferType    mType    = EBufferType::NUM_BUFFER_TYPES;
    std::mutex     mMtx;

    bool           mbUseVidMem     = true;
    char*          mpData          = nullptr;
    uint32         mMemInit        = 0;
    uint32         mMemOffset      = 0;
    uint32         mTotalMemSize   = 0;

    ID3D12Resource* mpSysMemBuffer = nullptr;
    ID3D12Resource* mpVidMemBuffer = nullptr;
};

//
// RING BUFFERS
//
class RingBuffer
{
public:
    void Create(uint32_t TotalSize);
    inline uint32_t GetSize() { return m_AllocatedSize; }
    inline uint32_t GetHead() { return m_Head; }
    inline uint32_t GetTail() { return (m_Head + m_AllocatedSize) % m_TotalSize; }
    //helper to avoid allocating chunks that wouldn't fit contiguously in the ring
    uint32_t PaddingToAvoidCrossOver(uint32_t size);
    bool Alloc(uint32_t size, uint32_t* pOut);
    bool Free(uint32_t size);
private:
    uint32_t m_Head;
    uint32_t m_AllocatedSize;
    uint32_t m_TotalSize;
};

class RingBufferWithTabs
{
public:
    void Create(uint32_t numberOfBackBuffers, uint32_t memTotalSize);
    void Destroy();
    bool Alloc(uint32_t size, uint32_t* pOut);
    void OnBeginFrame();

private:
    //internal ring buffer
    RingBuffer m_mem;

    uint32_t m_backBufferIndex;
    uint32_t m_numberOfBackBuffers;

    uint32_t m_memAllocatedInFrame;
    uint32_t m_allocatedMemPerBackBuffer[4];

};

class DynamicBufferHeap
{
public:
    void Create(ID3D12Device* pDevice, uint32_t numberOfBackBuffers, uint32_t memTotalSize);
    void Destroy();

    bool AllocIndexBuffer(uint32_t numbeOfIndices, uint32_t strideInBytes, void** pData, D3D12_INDEX_BUFFER_VIEW* pView);
    bool AllocVertexBuffer(uint32_t numbeOfVertices, uint32_t strideInBytes, void** pData, D3D12_VERTEX_BUFFER_VIEW* pView);
    bool AllocConstantBuffer(uint32_t size, void** pData, D3D12_GPU_VIRTUAL_ADDRESS* pBufferViewDesc);
    void OnBeginFrame();

private:
    uint32_t            m_memTotalSize;
    RingBufferWithTabs  m_mem;
    char* m_pData = nullptr;
    ID3D12Resource* m_pBuffer = nullptr;
};