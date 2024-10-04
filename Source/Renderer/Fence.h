#pragma once

#include <Windows.h>

struct ID3D12CommandQueue;
struct ID3D12Fence;
struct ID3D12Device;

class Fence
{
public:
    void Create(ID3D12Device* pDevice, const char* pDebugName);
    void Destroy();

    void Signal(ID3D12CommandQueue* pCommandQueue);
    inline UINT64 GetValue() const { return mFenceValue; }

    void WaitOnCPU(UINT64 olderFence);
    void WaitOnGPU(ID3D12CommandQueue* pCommandQueue);

private:
    HANDLE       mHEvent = 0;
    ID3D12Fence* mpFence = nullptr;
    UINT64       mFenceValue = 0;
};

#include <cassert>
inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        assert(false);// throw HrException(hr);
    }
}
