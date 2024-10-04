#pragma once

#include "Common.h"

#include <d3d12.h>

class ResourceView
{
public:
    inline uint32 GetSize() const { return mSize; }
    inline D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescHandle(uint32 i = 0) const { return D3D12_CPU_DESCRIPTOR_HANDLE{ mCPUDescriptor.ptr + static_cast<uint64>(i)*mDescriptorSize }; }
    inline D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescHandle(uint32 i = 0) const { return D3D12_GPU_DESCRIPTOR_HANDLE{ mGPUDescriptor.ptr + static_cast<uint64>(i)*mDescriptorSize }; }

    inline void SetResourceView(uint32 size, uint32 dsvDescriptorSize, D3D12_CPU_DESCRIPTOR_HANDLE CPUDescriptor, D3D12_GPU_DESCRIPTOR_HANDLE GPUDescriptor)
    {
        mSize = size;
        mCPUDescriptor = CPUDescriptor;
        mGPUDescriptor = GPUDescriptor;
        mDescriptorSize = dsvDescriptorSize;
    }

private:
    uint32 mSize = 0;
    uint32 mDescriptorSize = 0;

    D3D12_CPU_DESCRIPTOR_HANDLE mCPUDescriptor;
    D3D12_GPU_DESCRIPTOR_HANDLE mGPUDescriptor;
};

using VBV = D3D12_VERTEX_BUFFER_VIEW;
using IBV = D3D12_INDEX_BUFFER_VIEW;
class RTV         : public ResourceView { };
class DSV         : public ResourceView { };
class CBV_SRV_UAV : public ResourceView { };
class SAMPLER     : public ResourceView { };

