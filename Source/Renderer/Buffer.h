#pragma once

#include "ResourceHeaps.h"
#include "Common.h"
#include <unordered_map>

#include <mutex>

#include "Fence.h"

struct ID3D12Device;
struct ID3D12Resource;
struct D3D12_CONSTANT_BUFFER_VIEW_DESC;
struct ID3D12GraphicsCommandList;

#define BUFFER_MEMORY_ALIGNMENT 256llu

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

inline bool operator==(const FVertexDefault& lhs, const FVertexDefault& rhs)
{
    for (size_t i = 0; i < 3; ++i)
    {
        if (i < 2)
        {
            if (lhs.position[i] != rhs.position[i] ||
                lhs.uv[i] != rhs.uv[i])
                return false;
        }
        else
        {
            if (lhs.position[i] != rhs.position[i])
                return false;
        }
    }

    return true;
}

struct FVertexWithColor
{
    float position[3];
    float color[3];
    float uv[2];
};

inline bool operator==(const FVertexWithColor& lhs, const FVertexWithColor& rhs)
{
    for (size_t i = 0; i < 3; ++i)
    {
        if (i < 2)
        {
            if (lhs.position[i] != rhs.position[i] ||
                lhs.color[i] != rhs.color[i] ||
                lhs.uv[i] != rhs.uv[i])
                return false;
        }
        else
        {
            if (lhs.position[i] != rhs.position[i] ||
                lhs.color[i] != rhs.color[i])
                return false;
        }
    }

    return true;
}

struct FVertexWithColorAndAlpha
{
    float position[3];
    float color[4];
    float uv[2];
};

inline bool operator==(const FVertexWithColorAndAlpha& lhs, const FVertexWithColorAndAlpha& rhs)
{
    for (size_t i = 0; i < 4; ++i)
    {
        if (i < 2)
        {
            if (lhs.position[i] != rhs.position[i] ||
                lhs.color[i] != rhs.color[i] ||
                lhs.uv[i] != rhs.uv[i])
                return false;
        }
        else if(i < 3)
        {
            if (lhs.position[i] != rhs.position[i] ||
                lhs.color[i] != rhs.color[i])
                return false;
        }
        else
        {
            if (lhs.color[i] != rhs.color[i])
                return false;
        }
    }

    return true;
}

struct FVertexWithNormal
{
    float position[3];
    float normal[3];
    float uv[2];
};

inline bool operator==(const FVertexWithNormal& lhs, const FVertexWithNormal& rhs)
{
    for (size_t i = 0; i < 3; ++i)
    {
        if (i < 2)
        {
            if (lhs.position[i] != rhs.position[i] ||
                lhs.normal[i] != rhs.normal[i] ||
                lhs.uv[i] != rhs.uv[i])
                return false;
        }
        else
        {
            if (lhs.position[i] != rhs.position[i] ||
                lhs.normal[i] != rhs.normal[i])
                return false;
        }
    }

    return true;
}


struct FVertexWithNormalAndTangent
{
    float position[3];
    float normal[3];
    float tangent[3];
    float uv[2];
};

inline bool operator==(const FVertexWithNormalAndTangent& lhs, const FVertexWithNormalAndTangent& rhs)
{
    for (size_t i = 0; i < 3; ++i)
    {
        if (i < 2)
        {
            if (lhs.position[i] != rhs.position[i] ||
                lhs.tangent[i]  != rhs.tangent[i]  ||
                lhs.normal[i]   != rhs.normal[i]   ||
                lhs.uv[i] != rhs.uv[i])
                return false;
        }
        else
        {
            if (lhs.position[i] != rhs.position[i] ||
                lhs.tangent[i] != rhs.tangent[i]   ||
                lhs.normal[i] != rhs.normal[i])
                return false;
        }
    }

    return true;
}

class D3D12HeapNotEnoughMemory : public std::exception
{
public:
    D3D12HeapNotEnoughMemory(const char* info) : std::exception(info)
    {}
};

class DynamicHeap
{
    struct BufferRegion
    {
        size_t offset;
        size_t size;
    };

    struct BufferPlacement
    {
        uint64_t index;
    };

    //struct BufferCoordinate
    //{
    //    BufferRegion region;
    //    uint8_t level;
    //};

    using Occupied = std::list<BufferRegion>;
    using RegistereMap = std::unordered_map<ID3D12Resource*, BufferPlacement>;

public:
    void Create(ID3D12Device* device , const D3D12_HEAP_DESC& desc);
    void Destroy();

    uint64_t Allocate(ID3D12Resource* resource, size_t size, size_t alignment);
    uint64_t Allocate(ID3D12Resource* resource, size_t offset, size_t size, size_t alignment);

    bool IsAvailable(const uint64_t size) const { return size >= mRemSize; }

    //uint64_t Allocate(ID3D12Resource* resource, size_t size,   size_t alignment, const std::list<ID3D12Resource*>& toAlias);
    //uint64_t Allocate(ID3D12Resource* resource, size_t offset, size_t size, size_t alignment, const std::list<ID3D12Resource*>& toAlias);


    void Deallocate(ID3D12Resource* resource);

    //void SetAliasmentState(bool aliasment) 
    //{
    //    if(mOccupied.size() <= 1)
    //        mbAliasAllowed = aliasment;
    //    else
    //    {
    //        Log::Warning("Couldn't change Dynamic Heap aliasment policy because it's in \"alisament\" state");
    //    }
    //
    //}

    const D3D12_HEAP_DESC& GetDesc() const { return mDesc; }

private:

    uint64_t SetResource(const size_t& offset, const size_t& size, std::list<BufferRegion>::iterator regionIt, ID3D12Resource* resource, uint64_t index);

    //void FindAliasedRange(std::pair<uint64_t, uint64_t>& aliasedRange, const std::list<ID3D12Resource*>& toAlias);

    ID3D12Heap1* mHeap;
    D3D12_HEAP_DESC mDesc;

    uint64_t mRemSize;

    Occupied mOccupied;

    RegistereMap mResRegisterMap;

    //bool mbAliasAllowed;
};

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

class Buffer
{
public:
    void Release();

    void UploadOnGPU(ID3D12CommandQueue* mCommndQueue , ID3D12GraphicsCommandList* pCmdList);

    virtual ~Buffer();

protected:
    Buffer(bool bUseVidMem) :mbUseVidMem(bUseVidMem) 
    {
        mFence.Create(mpDevice, "Buffer Fence");
    }

    void AllocOnHeap(ID3D12Device* pDevice, D3D12_HEAP_DESC& desc);
    void DeallocateFromHeap();

    std::mutex mMtx;

    ID3D12Device* mpDevice = nullptr;

    static std::list<std::shared_ptr<DynamicHeap>> mGlobalHeaps;

    std::shared_ptr<DynamicHeap> mSysResourceHeap;
    std::shared_ptr<DynamicHeap> mVidResourceHeap;

    ID3D12Resource* mSysResource = nullptr;
    ID3D12Resource* mVidResource = nullptr;

    EBufferType mType = EBufferType::NUM_BUFFER_TYPES;

    uint64_t mSysOffsetInHeap = 0;
    uint64_t mVidOffsetInHeap = 0;
    uint64_t mSize = 0 ;

    bool mbIsResident = false;
    bool mbUseVidMem = false;

    Fence mFence;
    uint64_t mFenceValue = 0;
};

class VertexBuffer : public Buffer
{
public:
    VertexBuffer(bool bUseVidMem) :Buffer(bUseVidMem) {}

    void Create(ID3D12Device* pDevice,uint32 numVertices, uint32 strideInBytes, const void* pInitData, D3D12_VERTEX_BUFFER_VIEW* pViewOut, D3D12_HEAP_DESC& desc);
    
private:

};

class IndexBuffer : public Buffer
{
public:
    IndexBuffer(bool bUseVidMem) :Buffer(bUseVidMem) {}

    void Create(ID3D12Device* pDevice,uint32 numIndices, uint32 strideInBytes, const void* pInitData, D3D12_INDEX_BUFFER_VIEW* pViewOut, D3D12_HEAP_DESC& desc);

private:

};


class StaticBufferHeap
{
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
    uint32_t m_allocatedMemPerBackBuffer[3];

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