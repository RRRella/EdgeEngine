#pragma once

#include "ResourceHeaps.h"
#include "Common.h"

#include <mutex>

struct ID3D12Device;
struct ID3D12Resource;
struct D3D12_CONSTANT_BUFFER_VIEW_DESC;
struct ID3D12GraphicsCommandList;

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



//
// BUFFER POOL DEFINITIONS
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

class StaticBufferPool
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

#if 0
class StaticConstantBufferPool
{
public:
    void Create(ID3D12Device* pDevice, uint32 totalMemSize, ResourceViewHeaps* pHeaps, uint32 cbvEntriesSize, bool bUseVidMem);
    void Destroy();
    bool AllocConstantBuffer(uint32 size, void** pData, uint32* pIndex);
    bool CreateCBV(uint32 index, int srvOffset, CBV_SRV_UAV* pCBV);
    void UploadData(ID3D12GraphicsCommandList* pCmdList);
    void FreeUploadHeap();

private:
    ID3D12Device* mpDevice;
    ID3D12Resource* m_pMemBuffer;
    ID3D12Resource* mpSysMemBuffer;
    ID3D12Resource* mpVidMemBuffer;

    char*          mpData;
    uint32         mMemOffset;
    uint32         mTotalMemSize;

    uint32         m_cbvOffset;
    uint32         m_cbvEntriesSize;

    D3D12_CONSTANT_BUFFER_VIEW_DESC* m_pCBVDesc;

    bool            mbUseVidMem;
};

class DynamicBufferRing
{
public:
    void Create(ID3D12Device* pDevice, uint32 numberOfBackBuffers, uint32 memTotalSize, ResourceViewHeaps* pHeaps);
    void Destroy();

    bool AllocIndexBuffer(uint32 numIndices, uint32 strideInBytes, void** pData, D3D12_INDEX_BUFFER_VIEW* pView);
    bool AllocVertexBuffer(uint32 numVertices, uint32 strideInBytes, void** pData, D3D12_VERTEX_BUFFER_VIEW* pView);
    bool AllocConstantBuffer(uint32 size, void** pData, D3D12_GPU_VIRTUAL_ADDRESS* pBufferViewDesc);
    void OnBeginFrame();

private:
    uint32          m_memTotalSize;
    RingWithTabs    m_mem;
    char*           mpData = nullptr;
    ID3D12Resource* m_pBuffer = nullptr;
};
#endif