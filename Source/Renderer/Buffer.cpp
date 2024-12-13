#include "Buffer.h"
#include "Common.h"

#include "Libs/D3DX12/d3dx12.h"

#include <cassert>

std::list<std::shared_ptr<DynamicHeap>> Buffer::mGlobalHeaps = {};

void DynamicHeap::Create(ID3D12Device* device, const D3D12_HEAP_DESC& desc)
{
    mDesc = desc;
    mRemSize = mDesc.SizeInBytes;

    device->CreateHeap(&mDesc, IID_PPV_ARGS(&mHeap));

    mOccupied = { std::list<BufferRegion>{} };
}

void DynamicHeap::Destroy()
{
    if (!mResRegisterMap.empty())
    {
        Log::Error("Attemp on deleting a occupied D3D12 Heap");
        assert(false);
    }

    mHeap->Release();
}

uint64_t DynamicHeap::SetResource(const size_t& offset, const size_t& size, std::list<BufferRegion>::iterator regionIt)
{
    BufferRegion bufferCoordinate = BufferRegion{ offset , size };

    if (regionIt == mOccupied.end())
        mOccupied.emplace_front(bufferCoordinate);
    else
        mOccupied.emplace(regionIt, bufferCoordinate);

    mRemSize -= size;

    return bufferCoordinate.offset;
}

void DynamicHeap::RegisterResource(ID3D12Resource* resource,uint64_t index)
{
    mResRegisterMap[resource] = BufferPlacement{ index };
}

uint64_t DynamicHeap::Allocate(ID3D12Resource* resource, size_t size, size_t alignment, uint64_t& indexInHeap)
{
    if(size < mRemSize)
        throw D3D12HeapNotEnoughMemory{"Not enough memory"};

    if (size == 0)
    {
        Log::Error("D3D12 Resource cannot have 0 size");
        assert(false);
    }

    if (!mOccupied.empty())
    {
        size_t newOffset = 0;
        size_t index = 0;

        auto region = mOccupied.begin();

        if (size <= region->offset)
        {
            return SetResource(0, size, mOccupied.end());
            indexInHeap = index;
        }

        newOffset = AlignOffset(region->offset + region->size, alignment);
        ++region;
        ++index;

        for (region; region != mOccupied.end(); ++region)
        {
            if (region->offset > newOffset && region->offset - newOffset >= size)
            {
                return SetResource(newOffset, size, --region);
                indexInHeap = index;
            }

            newOffset = AlignOffset(region->offset + region->size, alignment);
            ++index;
        }

        if (newOffset + size <= mDesc.SizeInBytes)
        {
            return SetResource(newOffset, size, --region);
            indexInHeap = index;
        }

    }
    else
    {
        if (size <= mDesc.SizeInBytes)
        {
            return SetResource(0, size, mOccupied.end());
            indexInHeap = 0;
        }
    }

    throw D3D12HeapNotEnoughMemory{"Not enough memory"};
}

uint64_t DynamicHeap::Allocate(ID3D12Resource* resource, size_t offset, size_t size, size_t alignment, uint64_t& indexInHeap)
{
    if (size < mRemSize)
        throw D3D12HeapNotEnoughMemory{"Not enough memory"};

    if (offset != ((offset + (alignment - 1)) & ~(alignment - 1)))
    {
        Log::Error("Non-aligned offset passed to D3D12 heap");
        assert(false);
    }

    if (!mOccupied.empty())
    {
        auto region = mOccupied.begin();
        size_t index = 0;

        for (region; region != mOccupied.end(); ++region)
        {
            if (offset >= region->offset + region->size)
            {
                if (++region == mOccupied.end())
                    break;

                ++index;

                if (offset + size <= region->offset)
                {
                    return SetResource(offset, size, --region);
                    indexInHeap = index;
                }
                else
                    break;
            }
            ++index;
        }

        if (region == mOccupied.end())
            if (offset + size <= mDesc.SizeInBytes)
            {
                return SetResource(offset, size, --region);
                indexInHeap = index;
            }
    }
    else
        if (offset + size <= mDesc.SizeInBytes)
        { 
            return SetResource(offset, size, mOccupied.end());
            indexInHeap = 0;
        }

    Log::Error("Invalid offset used for allocation inside D3D12 heap");
    assert(false);
}

void DynamicHeap::Deallocate(ID3D12Resource* resource)
{
    auto registeredResource = mResRegisterMap.find(resource);

    if (registeredResource != mResRegisterMap.end())
    {
        auto toPop = mOccupied.begin();

        std::advance(toPop, registeredResource->second.index);

        mOccupied.erase(toPop);
        mResRegisterMap.erase(registeredResource);
    }
}


static D3D12_RESOURCE_STATES GetResourceTransitionState(EBufferType eType)
{
    D3D12_RESOURCE_STATES s = D3D12_RESOURCE_STATE_COMMON; //0
    switch (eType)
    {
    case CONSTANT_BUFFER: s = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER; break;
    case VERTEX_BUFFER: s = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER; break;
    case INDEX_BUFFER: s = D3D12_RESOURCE_STATE_INDEX_BUFFER; break;
    default:
        Log::Warning("StaticBufferPool::Create(): unkown resource type, couldn't determine resource transition state for upload.");
        break;
    }
    return s;
}

//
// CREATE / DESTROY
//
void StaticBufferHeap::Create(ID3D12Device* pDevice, EBufferType type, uint32 totalMemSize, bool bUseVidMem, const char* name)
{
    mpDevice = pDevice;
    mTotalMemSize = totalMemSize;
    mMemOffset = 0;
    mMemInit = 0;
    mpData = nullptr;
    mbUseVidMem = bUseVidMem;
    mType = type;

    HRESULT hr = {};
    if (bUseVidMem)
    {
        auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(totalMemSize);
        hr = mpDevice->CreateCommittedResource(
            &heapProp,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            GetResourceTransitionState(mType),
            nullptr,
            IID_PPV_ARGS(&mpVidMemBuffer));
        mpVidMemBuffer->SetName(L"StaticBufferPool::m_pVidMemBuffer");

        if (FAILED(hr))
        {
            assert(false);
            // TODO
        }
    }

    auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(totalMemSize);
    hr = mpDevice->CreateCommittedResource(
        &heapProp,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&mpSysMemBuffer));

    if (FAILED(hr))
    {
        assert(false);
        // TODO
    }

    mpSysMemBuffer->SetName(L"StaticBufferPool::m_pSysMemBuffer");

    mpSysMemBuffer->Map(0, NULL, reinterpret_cast<void**>(&mpData));
}

void StaticBufferHeap::Destroy()
{
    if (mbUseVidMem)
    {
        if (mpVidMemBuffer)
        {
            mpVidMemBuffer->Release();
            mpVidMemBuffer = nullptr;
        }
    }

    if (mpSysMemBuffer)
    {
        mpSysMemBuffer->Release();
        mpSysMemBuffer = nullptr;
    }
}



//
// ALLOC
//
bool StaticBufferHeap::AllocBuffer(uint32 numElements, uint32 strideInBytes, const void* pInitData, D3D12_GPU_VIRTUAL_ADDRESS* pBufferLocation, uint32* pSizeOut)
{
    void* pData;
    if (AllocBuffer(numElements, strideInBytes, &pData, pBufferLocation, pSizeOut))
    {
        memcpy(pData, pInitData, static_cast<size_t>(numElements) * strideInBytes);
        return true;
    }
    return false;
}

bool StaticBufferHeap::AllocVertexBuffer(uint32 numVertices, uint32 strideInBytes, const void* pInitData, D3D12_VERTEX_BUFFER_VIEW* pViewOut)
{
    assert(mType == EBufferType::VERTEX_BUFFER);
    void* pData = nullptr;
    if (AllocVertexBuffer(numVertices, strideInBytes, &pData, pViewOut))
    {
        memcpy(pData, pInitData, static_cast<size_t>(numVertices) * strideInBytes);
        return true;
    }

    return false;
}

bool StaticBufferHeap::AllocIndexBuffer(uint32 numIndices, uint32 strideInBytes, const void* pInitData, D3D12_INDEX_BUFFER_VIEW* pOut)
{
    assert(mType == EBufferType::INDEX_BUFFER);
    void* pData = nullptr;
    if (AllocIndexBuffer(numIndices, strideInBytes, &pData, pOut))
    {
        memcpy(pData, pInitData, static_cast<size_t>(strideInBytes) * numIndices);
        return true;
    }
    return false;
}



bool StaticBufferHeap::AllocBuffer(uint32 numElements, uint32 strideInBytes, void** ppDataOut, D3D12_GPU_VIRTUAL_ADDRESS* pBufferLocationOut, uint32* pSizeOut)
{
    std::lock_guard<std::mutex> lock(mMtx);

    uint32 size = AlignOffset(numElements * strideInBytes, (uint32)BUFFER_MEMORY_ALIGNMENT);
    assert(mMemOffset + size < mTotalMemSize); // if this is hit, initialize heap with a larger size.

    *ppDataOut = (void*)(mpData + mMemOffset);

    ID3D12Resource*& pRsc = mbUseVidMem ? mpVidMemBuffer : mpSysMemBuffer;

    *pBufferLocationOut = mMemOffset + pRsc->GetGPUVirtualAddress();
    *pSizeOut = size;

    mMemOffset += size;

    return true;
}

bool StaticBufferHeap::AllocVertexBuffer(uint32 numVertices, uint32 strideInBytes, void** ppDataOut, D3D12_VERTEX_BUFFER_VIEW* pViewOut)
{
    bool bSuccess = AllocBuffer(numVertices, strideInBytes, ppDataOut, &pViewOut->BufferLocation, &pViewOut->SizeInBytes);
    pViewOut->StrideInBytes = bSuccess ? strideInBytes : 0;
    return bSuccess;
}

bool StaticBufferHeap::AllocIndexBuffer(uint32 numIndices, uint32 strideInBytes, void** ppDataOut, D3D12_INDEX_BUFFER_VIEW* pViewOut)
{
    bool bSuccess = AllocBuffer(numIndices, strideInBytes, ppDataOut, &pViewOut->BufferLocation, &pViewOut->SizeInBytes);
    pViewOut->Format = bSuccess
        ? ((strideInBytes == 4) ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT)
        : DXGI_FORMAT_UNKNOWN;
    return bSuccess;
}

//
// UPLOAD
//
void StaticBufferHeap::UploadData(ID3D12GraphicsCommandList* pCmd)
{
    if (mbUseVidMem)
    {
        D3D12_RESOURCE_STATES state = GetResourceTransitionState(mType);

        pCmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mpVidMemBuffer
            , state
            , D3D12_RESOURCE_STATE_COPY_DEST)
        );

        pCmd->CopyBufferRegion(mpVidMemBuffer, mMemInit, mpSysMemBuffer, mMemInit, mMemOffset - mMemInit);

        pCmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mpVidMemBuffer
            , D3D12_RESOURCE_STATE_COPY_DEST
            , state)
        );

        mMemInit = mMemOffset;
    }
}


//
// DYNAMIC BUFFER HEAP
//
void DynamicBufferHeap::Create(ID3D12Device* pDevice, uint32_t numberOfBackBuffers, uint32_t memTotalSize)
{
    m_memTotalSize = AlignOffset(memTotalSize, 256u);
    m_mem.Create(numberOfBackBuffers, memTotalSize);
    ThrowIfFailed(pDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(memTotalSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_pBuffer)));
    SetName(m_pBuffer, "DynamicBufferHeap::m_pBuffer");
    m_pBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_pData));
}
void DynamicBufferHeap::Destroy()
{
    m_pBuffer->Release();
    m_mem.Destroy();
}
bool DynamicBufferHeap::AllocConstantBuffer(uint32_t size, void** pData, D3D12_GPU_VIRTUAL_ADDRESS* pBufferViewDesc)
{
    size = AlignOffset(size, 256u);
    uint32_t memOffset;
    if (m_mem.Alloc(size, &memOffset) == false)
    {
        Log::Error("Ran out of mem for 'dynamic' buffers, please increase the allocated size\n");
        return false;
    }
    *pData = (void*)(m_pData + memOffset);
    *pBufferViewDesc = m_pBuffer->GetGPUVirtualAddress() + memOffset;
    return true;
}
bool DynamicBufferHeap::AllocVertexBuffer(uint32_t NumVertices, uint32_t strideInBytes, void** ppData, D3D12_VERTEX_BUFFER_VIEW* pView)
{
    uint32_t size = AlignOffset(NumVertices * strideInBytes, 256u);
    uint32_t memOffset;
    if (m_mem.Alloc(size, &memOffset) == false)
        return false;
    *ppData = (void*)(m_pData + memOffset);
    pView->BufferLocation = m_pBuffer->GetGPUVirtualAddress() + memOffset;
    pView->StrideInBytes = strideInBytes;
    pView->SizeInBytes = size;
    return true;
}
bool DynamicBufferHeap::AllocIndexBuffer(uint32_t NumIndices, uint32_t strideInBytes, void** ppData, D3D12_INDEX_BUFFER_VIEW* pView)
{
    uint32_t size = AlignOffset(NumIndices * strideInBytes, 256u);
    uint32_t memOffset;
    if (m_mem.Alloc(size, &memOffset) == false)
        return false;
    *ppData = (void*)(m_pData + memOffset);
    pView->BufferLocation = m_pBuffer->GetGPUVirtualAddress() + memOffset;
    pView->Format = (strideInBytes == 4) ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
    pView->SizeInBytes = size;
    return true;
}
//--------------------------------------------------------------------------------------
//
// OnBeginFrame
//
//--------------------------------------------------------------------------------------
void DynamicBufferHeap::OnBeginFrame()
{
    m_mem.OnBeginFrame();
}
//
// RING BUFFER
// 
void RingBuffer::Create(uint32_t TotalSize)
{
    m_Head = 0;
    m_AllocatedSize = 0;
    m_TotalSize = TotalSize;
}
uint32_t RingBuffer::PaddingToAvoidCrossOver(uint32_t size)
{
    int tail = GetTail();
    if ((tail + size) > m_TotalSize)
        return (m_TotalSize - tail);
    else
        return 0;
}
bool RingBuffer::Alloc(uint32_t size, uint32_t* pOut)
{
    if (m_AllocatedSize + size <= m_TotalSize)
    {
        if (pOut)
            *pOut = GetTail();
        m_AllocatedSize += size;
        return true;
    }
    assert(false);
    return false;
}
bool RingBuffer::Free(uint32_t size)
{
    if (m_AllocatedSize >= size)
    {
        m_Head = (m_Head + size) % m_TotalSize;
        m_AllocatedSize -= size;
        return true;
    }
    return false;
}
//
// RING BUFFER WITH TABS
//
void RingBufferWithTabs::Create(uint32_t numberOfBackBuffers, uint32_t memTotalSize)
{
    m_backBufferIndex = 0;
    m_numberOfBackBuffers = numberOfBackBuffers;
    //init mem per frame tracker
    m_memAllocatedInFrame = 0;
    for (int i = 0; i < 3; i++)
        m_allocatedMemPerBackBuffer[i] = 0;
    m_mem.Create(memTotalSize);
}
void RingBufferWithTabs::Destroy()
{
    m_mem.Free(m_mem.GetSize());
}
bool RingBufferWithTabs::Alloc(uint32_t size, uint32_t* pOut)
{
    uint32_t padding = m_mem.PaddingToAvoidCrossOver(size);
    if (padding > 0)
    {
        m_memAllocatedInFrame += padding;
        if (m_mem.Alloc(padding, NULL) == false) //alloc chunk to avoid crossover, ignore offset        
        {
            return false;  //no mem, cannot allocate apdding
        }
    }
    if (m_mem.Alloc(size, pOut) == true)
    {
        m_memAllocatedInFrame += size;
        return true;
    }
    return false;
}
void RingBufferWithTabs::OnBeginFrame()
{
    m_allocatedMemPerBackBuffer[m_backBufferIndex] = m_memAllocatedInFrame;
    m_memAllocatedInFrame = 0;
    m_backBufferIndex = (m_backBufferIndex + 1) % m_numberOfBackBuffers;
    // free all the entries for the oldest buffer in one go
    uint32_t memToFree = m_allocatedMemPerBackBuffer[m_backBufferIndex];
    m_mem.Free(memToFree);
}



//uint64_t DynamicHeap::Allocate(ID3D12Resource* resource, size_t size, size_t alignment, const std::list<ID3D12Resource*>& toAlias)
//{
//    if (toAlias.empty())
//    {
//        Log::Error("empty aliasing resource list passed , try using other overload fucntion of allocate");
//        assert(false);
//    }
//
//    std::pair<uint64_t, uint64_t > aliasedRange{ 0,ULLONG_MAX };
//
//    FindAliasedRange(aliasedRange, toAlias);
//
//    aliasedRange.first = (aliasedRange.first) & ~(alignment - 1);
//
//    if(aliasedRange.second - aliasedRange.first <= size)
//        
//
//    for (auto level = mOccupied.begin(); level != mOccupied.end(); ++level)
//    {
//        auto region = level->begin();
//
//        for (region; region != level->end(); ++region)
//        {
//            if (aliasedRange.first >= region->offset + region->size)
//            {
//                ++region;
//
//                uint64_t right = 0;
//                if (region == level->end())
//                    right = mDesc.SizeInBytes;
//                else
//                    right = region->offset;
//
//                if (aliasedRange.second < right)
//                {
//                   
//                }
//
//                if (aliasedRange.second - aliasedRange.first >= size)
//                {
//                    return SetResource(aliasedRange.first, size, 0, mOccupied.begin(), mOccupied.begin()->end(), resource);
//                }
//
//                --region;
//            }
//        }
//    }
//}
//
//uint64_t DynamicHeap::Allocate(ID3D12Resource* resource, size_t offset, size_t size, size_t alignment, const std::list<ID3D12Resource*>& toAlias)
//{
//    return false;
//}
//void DynamicHeap::FindAliasedRange(std::pair<uint64_t,uint64_t>& aliasedRange, const std::list<ID3D12Resource*>& toAlias)
//{
//    for (auto resource = toAlias.begin(); resource != toAlias.end(); ++resource)
//    {
//        auto mappedResource = mResRegisterMap.find(*resource);
//
//        if (mappedResource != mResRegisterMap.end())
//        {
//            const auto& region = mappedResource->second.region;
//
//            if (aliasedRange.first < region.offset)
//                aliasedRange.first = region.offset;
//
//            auto last = region.offset + region.size;
//
//            if (aliasedRange.second > last)
//                aliasedRange.second = last;
//        }
//    }
//
//    for (auto level = mOccupied.begin(); level != mOccupied.end(); ++level)
//    {
//        auto region = level->begin();
//
//        for (region; region != level->end(); ++region)
//        {
//            auto left = region->offset + region->size;
//
//            if (left <= aliasedRange.first)
//            {
//                aliasedRange.first = left;
//
//                ++region;
//
//                if (region == level->end())
//                    aliasedRange.second = mDesc.SizeInBytes;
//                else
//                {
//                    if (aliasedRange.second < region->offset)
//                        aliasedRange.second = region->offset;
//                }
//
//                break;
//            }
//            else
//                continue;
//
//        }
//    }
//}

Buffer::~Buffer()
{
    //Dont need to put a lock here 
    //because it's illegal to enter this section when it's deleted from the first place

    if(mbCreated)
        Release();

    mFence.Destroy();
}

void VertexBuffer::Create(uint32 numVertices, uint32 strideInBytes, const void* pInitData, D3D12_VERTEX_BUFFER_VIEW* pViewOut , const D3D12_HEAP_DESC& desc)
{
    std::lock_guard<std::mutex> lock(mMtx);

    if (!mbCreated)
    {
        AllocBuffer(numVertices, strideInBytes, pInitData, desc);

        if (mbUseVidMem)
            pViewOut->BufferLocation = mVidResource->GetGPUVirtualAddress();
        else
            pViewOut->BufferLocation = mSysResource->GetGPUVirtualAddress();

        pViewOut->SizeInBytes = mSize;

        pViewOut->StrideInBytes = strideInBytes;

        UploadOnGPU();

        mbCreated = true;
    }
}

void IndexBuffer::Create(uint32 numIndices, uint32 strideInBytes, const void* pInitData, D3D12_INDEX_BUFFER_VIEW* pViewOut, const D3D12_HEAP_DESC& desc)
{
    std::lock_guard<std::mutex> lock(mMtx);

    if (!mbCreated)
    {
        AllocBuffer(numIndices, strideInBytes, pInitData, desc);

        if (mbUseVidMem)
            pViewOut->BufferLocation = mVidResource->GetGPUVirtualAddress();
        else
            pViewOut->BufferLocation = mSysResource->GetGPUVirtualAddress();

        pViewOut->SizeInBytes = mSize;

        pViewOut->Format = ((strideInBytes == 4) ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT);

        UploadOnGPU();

        mbCreated = true;
    }
}

void Buffer::Release()
{
    std::lock_guard<std::mutex> lock(mMtx);
    mFence.WaitOnCPU(mFenceValue);

    DeallocateFromHeap();

    mbIsResident = false;
    mbCreated = false;
}

void Buffer::UploadOnGPU()
{
    if (mbUseVidMem)
    {
        if (!mbIsResident)
        {
            mpDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mCommandAllocator));
            mpDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCommandAllocator, nullptr, IID_PPV_ARGS(&mCommandList));

            D3D12_RESOURCE_STATES state = GetResourceTransitionState(mType);

            mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mVidResource
                , state
                , D3D12_RESOURCE_STATE_COPY_DEST)
            );

            mCommandList->CopyBufferRegion(mVidResource, 0, mSysResource, 0, mSize);

            mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mVidResource
                , D3D12_RESOURCE_STATE_COPY_DEST
                , state)
            );

            mCommandList->Close();
            mCommndQueue->ExecuteCommandLists(1, CommandListCast(&mCommandList));

            mFenceValue = mFence.Signal(mCommndQueue);

            mbIsResident = true;
        }
    }
}


void Buffer::AllocOnHeap(const D3D12_HEAP_DESC& desc, uint64_t& indexInVidHeap, uint64_t& indexInSysHeap)
{
    if (!mGlobalHeaps.empty())
    {
        bool SysMemAllocated = false;
        bool VidMemAllocated = false;
        for (auto& heap : mGlobalHeaps)
        {
            if (SysMemAllocated && VidMemAllocated)
                return;

            //check to see if heap has enough memory , but in allocation phase , it may fail anyway , allocations are not contigious

            if (heap->IsAvailable(mSize))
            {
                try
                {
                    if (!SysMemAllocated)
                    {
                        if (heap->GetDesc().Properties.Type == D3D12_HEAP_TYPE_UPLOAD)
                        {
                            mSysOffsetInHeap = heap->Allocate(mSysResource, mSize, BUFFER_MEMORY_ALIGNMENT, indexInSysHeap);
                            mSysResourceHeap = heap;
                            SysMemAllocated = true;
                        }
                    }
                    

                    if (mbUseVidMem)
                    {
                        if (!VidMemAllocated)
                        {
                            if (heap->GetDesc().Properties.Type == D3D12_HEAP_TYPE_DEFAULT)
                            {
                                mVidOffsetInHeap = heap->Allocate(mVidResource, mSize, BUFFER_MEMORY_ALIGNMENT, indexInVidHeap);
                                mVidResourceHeap = heap;
                                VidMemAllocated = true;
                            }
                        }
                    }
                }
                catch (const D3D12HeapNotEnoughMemory& e)
                {
                    continue;
                }
            }
        }
    }

    mSysResourceHeap = mGlobalHeaps.emplace_back(std::make_shared<DynamicHeap>());
    mVidResourceHeap = mGlobalHeaps.emplace_back(std::make_shared<DynamicHeap>());

    auto uploadDesc = desc;
    uploadDesc.Properties.Type = D3D12_HEAP_TYPE_UPLOAD;
    uploadDesc.SizeInBytes = mSize;

    auto defaultDesc = desc;
    defaultDesc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
    defaultDesc.SizeInBytes = mSize;

    mSysResourceHeap->Create(mpDevice, uploadDesc);
    mVidResourceHeap->Create(mpDevice, defaultDesc);


    mSysOffsetInHeap = mSysResourceHeap->Allocate(mSysResource, mSize, BUFFER_MEMORY_ALIGNMENT, indexInSysHeap);

    if (mbUseVidMem)
        mVidOffsetInHeap = mVidResourceHeap->Allocate(mVidResource, mSize, BUFFER_MEMORY_ALIGNMENT, indexInVidHeap);
}

void Buffer::DeallocateFromHeap()
{
    //Deallocate resources from their respective heaps
    mSysResourceHeap->Deallocate(mSysResource);
    mSysResource->Release();

    if (mSysResourceHeap->IsEmpty())
    {
        auto loc = std::find(mGlobalHeaps.begin(), mGlobalHeaps.end(), mSysResourceHeap);

        mGlobalHeaps.erase(loc);
        mSysResourceHeap.reset();
    }


    if (mbUseVidMem)
    {
        mVidResourceHeap->Deallocate(mVidResource);
        mVidResource->Release();

        if (mVidResourceHeap->IsEmpty())
        {
            auto loc = std::find(mGlobalHeaps.begin(), mGlobalHeaps.end(), mVidResourceHeap);

            mGlobalHeaps.erase(loc);
            mVidResourceHeap.reset();
        }
    }
}

void Buffer::AllocBuffer(uint32 numElements, uint32 strideInBytes, const void* pInitData, const D3D12_HEAP_DESC& desc)
{
    mSize = AlignOffset((uint64_t)numElements * strideInBytes, BUFFER_MEMORY_ALIGNMENT);
    uint64_t indexInSysHeap = 0;
    uint64_t indexInVidHeap = 0;
    AllocOnHeap(desc, indexInVidHeap, indexInSysHeap);

    HRESULT hr = {};
    if (mbUseVidMem)
    {
        auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(mSize);
        hr = mpDevice->CreatePlacedResource(
            mVidResourceHeap->GetHeap(),
            mVidOffsetInHeap,
            &bufferDesc,
            GetResourceTransitionState(mType),
            nullptr,
            IID_PPV_ARGS(&mVidResource));

        if (FAILED(hr))
            assert(false);

        mVidResource->SetName(L"VidMemBuffer");

        mVidResourceHeap->RegisterResource(mVidResource, indexInVidHeap);
    }

    auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(mSize);
    hr = mpDevice->CreatePlacedResource(
        mSysResourceHeap->GetHeap(),
        mSysOffsetInHeap,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&mSysResource));

    if (FAILED(hr))
        assert(false);

    mSysResource->SetName(L"SysMemBuffer");

    mSysResourceHeap->RegisterResource(mSysResource, indexInSysHeap);

    UINT8* data = nullptr;

    mSysResource->Map(0, NULL, reinterpret_cast<void**>(&data));

    memcpy(data, pInitData, static_cast<size_t>(strideInBytes) * numElements);
}
