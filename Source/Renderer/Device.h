#pragma once

#include <d3d12.h>
#include <dxgi.h>

struct FDeviceCreateDesc
{
	bool bEnableDebugLayer = false;
	bool bEnableValidationLayer = false;
};


class Device 
{
public:
	bool Create(const FDeviceCreateDesc& desc);
	void Destroy();

	inline ID3D12Device* GetDevicePtr()  const { return mpDevice; }
	inline IDXGIAdapter* GetAdapterPtr() const { return mpAdapter; }

	UINT GetDeviceMemoryMax() const;
	UINT GetDeviceMemoryAvailable() const;

private:
	ID3D12Device* mpDevice  = nullptr;
	IDXGIAdapter* mpAdapter = nullptr;
	// TODO: Multi-adapter systems: https://docs.microsoft.com/en-us/windows/win32/direct3d12/multi-engine

	//UINT mRTVDescIncremenetSize = 0;
};