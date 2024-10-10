#include "Fence.h"
#include "Common.h"

#include <d3d12.h>

void Fence::Create(ID3D12Device* pDevice, const char* pDebugName)
{
    mFenceValue = 0;
    //ThrowIfFailed(
        pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mpFence))
    //)
    ;
    //SetName(mpFence, pDebugName);
    //mHEvent = CreateEventEx(nullptr, "FENCE_EVENT0", FALSE, EVENT_ALL_ACCESS);
    mHEvent = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
}

void Fence::Destroy() 
{
    if(mpFence) mpFence->Release();
    CloseHandle(mHEvent);
}

void Fence::Signal(ID3D12CommandQueue* pCommandQueue)
{
    ++mFenceValue;
    //ThrowIfFailed(
        pCommandQueue->Signal(mpFence, mFenceValue)

    //)
    ;
}


void Fence::WaitOnCPU(UINT64 FenceWaitValue)
{
    if (mpFence->GetCompletedValue() < FenceWaitValue)
    {
        ThrowIfFailed(mpFence->SetEventOnCompletion(FenceWaitValue, mHEvent));
        WaitForSingleObject(mHEvent, INFINITE);
    }
}

void Fence::WaitOnGPU(ID3D12CommandQueue* pCommandQueue)
{
    //ThrowIfFailed(
        pCommandQueue->Wait(mpFence, mFenceValue)
    //);
    ;
}