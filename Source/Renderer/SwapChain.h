#pragma once

#include "Fence.h"

#include <d3d12.h>
#include <dxgi1_6.h>

#include <vector>
#include <memory>

#include "Libs/D3DX12/d3dx12.h"

class CommandQueue;
class Window;

struct FWindowRepresentation
{
	HWND hwnd; int width, height;
	bool bVSync;
	bool bFullscreen;
	FWindowRepresentation(const std::unique_ptr<Window>& pWnd, bool bVSync, bool bFullscreen);
};
struct FSwapChainCreateDesc
{
	ID3D12Device*                pDevice   = nullptr;
	const FWindowRepresentation* pWindow   = nullptr;
	CommandQueue*                pCmdQueue = nullptr;

	int numBackBuffers;
	bool bVSync;
	bool bFullscreen;
};


// https://docs.microsoft.com/en-us/windows/win32/direct3d12/swap-chains
class SwapChain
{
public:
	bool Create(const FSwapChainCreateDesc& desc);
	void Destroy();
	void Resize(int w, int h);

	void SetFullscreen(bool bState, int FSRecoveryWindowWidth, int FSRecoveryWindowHeight);
	bool IsFullscreen() const;

	HRESULT Present(bool bVSync = false);
	void MoveToNextFrame();
	void WaitForGPU();

	/* Getters */ 
	inline int                           GetNumBackBuffers()                const { return mNumBackBuffers; }
	inline int                           GetCurrentBackBufferIndex()        const { return mICurrentBackBuffer; }
	inline CD3DX12_CPU_DESCRIPTOR_HANDLE GetCurrentBackBufferRTVHandle()    const { return CD3DX12_CPU_DESCRIPTOR_HANDLE(mpDescHeapRTV->GetCPUDescriptorHandleForHeapStart(), GetCurrentBackBufferIndex(), mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)); }
	inline ID3D12Resource*               GetCurrentBackBufferRenderTarget() const { return mRenderTargets[GetCurrentBackBufferIndex()]; }
	inline unsigned long long            GetNumPresentedFrames()            const { return mNumTotalFrames; }

private:
	void CreateRenderTargetViews();
	void DestroyRenderTargetViews();

private:
	friend class Window; // Window can access pSwapChain.

	HWND                         mHwnd               = NULL;
	unsigned short               mNumBackBuffers     = 0;
	unsigned short               mICurrentBackBuffer = 0;
	unsigned long long           mNumTotalFrames     = 0;

	HANDLE                       mHEvent = 0;
	ID3D12Fence*                 mpFence = nullptr;
	std::vector<UINT64>          mFenceValues;

	std::vector<ID3D12Resource*> mRenderTargets;
	ID3D12DescriptorHeap*        mpDescHeapRTV = nullptr;

	ID3D12Device*                mpDevice         = nullptr;
	IDXGIAdapter*                mpAdapter        = nullptr;

	IDXGISwapChain4*             mpSwapChain      = nullptr;
	ID3D12CommandQueue*          mpPresentQueue   = nullptr;
	DXGI_FORMAT                  mSwapChainFormat = DXGI_FORMAT_UNKNOWN;
	// TODO: HDR: https://docs.microsoft.com/en-us/windows/win32/direct3darticles/high-dynamic-range
};
