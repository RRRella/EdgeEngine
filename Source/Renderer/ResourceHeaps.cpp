#include "ResourceHeaps.h"
#include "ResourceViews.h"

#include "Libs/D3DX12/d3dx12.h"
#include "../Utils/Source/Log.h"

#include <cassert>

//--------------------------------------------------------------------------------------
//
// StaticResourceViewHeap
//
//--------------------------------------------------------------------------------------
void StaticResourceViewHeap::Create(ID3D12Device* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32 descriptorCount, bool forceCPUVisible)
{
    mDescriptorCount = descriptorCount;
    mIndex = 0;
        
    mDescriptorElementSize = pDevice->GetDescriptorHandleIncrementSize(heapType);

    D3D12_DESCRIPTOR_HEAP_DESC descHeap;
    descHeap.NumDescriptors = descriptorCount;
    descHeap.Type = heapType;
    descHeap.Flags = ((heapType == D3D12_DESCRIPTOR_HEAP_TYPE_RTV) || (heapType == D3D12_DESCRIPTOR_HEAP_TYPE_DSV)) 
        ? D3D12_DESCRIPTOR_HEAP_FLAG_NONE 
        : D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    
    if (forceCPUVisible)
    {
        descHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    }
    descHeap.NodeMask = 0;

    pDevice->CreateDescriptorHeap(&descHeap, IID_PPV_ARGS(&mpHeap));
    mpHeap->SetName(L"StaticHeap");
}

void StaticResourceViewHeap::Destroy()
{
    mpHeap->Release();
}

bool StaticResourceViewHeap::AllocDescriptor(uint32 size, ResourceView* pRV)
{
    if ((mIndex + size) > mDescriptorCount)
    {
        assert(!"StaticResourceViewHeapDX12 heap ran of memory, increase its size");
        return false;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE CPUView = mpHeap->GetCPUDescriptorHandleForHeapStart();
    CPUView.ptr += mIndex * mDescriptorElementSize;

    D3D12_GPU_DESCRIPTOR_HANDLE GPUView = mpHeap->GetGPUDescriptorHandleForHeapStart();
    GPUView.ptr += mIndex * mDescriptorElementSize;

    mIndex += size;

    pRV->SetResourceView(size, mDescriptorElementSize, CPUView, GPUView);

    return true;
}


// ===========================================================================================================================================


//--------------------------------------------------------------------------------------
//
// UploadHeap
//
//--------------------------------------------------------------------------------------
void UploadHeap::Create(ID3D12Device* pDevice, SIZE_T uSize)
{
    mpDevice = pDevice;

    // Create command list and allocators 

    pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mpCommandAllocator));
    mpCommandAllocator->SetName(L"UploadHeap::mpCommandAllocator");
    pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mpCommandAllocator, nullptr, IID_PPV_ARGS(&mpCommandList));
    mpCommandList->SetName(L"UploadHeap::mpCommandList");

    // Create buffer to suballocate
    auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uSize);

    HRESULT hr = {};
    hr = pDevice->CreateCommittedResource(
        &heapProp,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&mpUploadHeap)
    );
    if (FAILED(hr))
    {
        Log::Error("Couldn't create upload heap.");
        return;
    }

    hr = mpUploadHeap->Map(0, NULL, (void**)&mpDataBegin);
    if (FAILED(hr))
    {
        Log::Error("Couldn't map upload heap.");
        return;
    }

    mpDataCur = mpDataBegin;
    mpDataEnd = mpDataBegin + mpUploadHeap->GetDesc().Width;

    hr = pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mpFence));
    mHEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    mFenceValue = 1;
}

void UploadHeap::Destroy()
{
    mpUploadHeap->Release();

    if (mpFence) mpFence->Release();

    mpCommandList->Release();
    mpCommandAllocator->Release();
}


UINT8* UploadHeap::Suballocate(SIZE_T uSize, UINT64 uAlign)
{
    mpDataCur = reinterpret_cast<UINT8*>(AlignOffset(reinterpret_cast<SIZE_T>(mpDataCur), SIZE_T(uAlign)));

    // return NULL if we ran out of space in the heap
    if (mpDataCur >= mpDataEnd || mpDataCur + uSize >= mpDataEnd)
    {
        return NULL;
    }

    UINT8* pRet = mpDataCur;
    mpDataCur += uSize;
    return pRet;
}

void UploadHeap::UploadToGPUAndWait(ID3D12CommandQueue* pCmdQueue)
{
    mpCommandList->Close();
    pCmdQueue->ExecuteCommandLists(1, CommandListCast(&mpCommandList));
    pCmdQueue->Signal(mpFence, mFenceValue);
    if (mpFence->GetCompletedValue() < mFenceValue)
    {
        mpFence->SetEventOnCompletion(mFenceValue, mHEvent);
        WaitForSingleObject(mHEvent, INFINITE);
    }

    ++mFenceValue;

    // Reset so it can be reused
    mpCommandAllocator->Reset();
    mpCommandList->Reset(mpCommandAllocator, nullptr);

    mpDataCur = mpDataBegin;
}