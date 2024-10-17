#include "SwapChain.h"
#include "CommandQueue.h"
#include "Common.h"

#include "../Application/Platform.h" // CHECK_HR
#include "../Application/Window.h"
#include "../Utils/Source/Log.h"
#include "../Utils/Source/utils.h"


#include <dxgi1_6.h>
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

#ifdef _DEBUG
#pragma comment(lib, "dxguid.lib")
#include <DXGIDebug.h>
#endif 

#include <cassert>
#include <vector>

#define NUM_MAX_BACK_BUFFERS  3
#define LOG_SWAPCHAIN_VERBOSE 0

#if LOG_SWAPCHAIN_VERBOSE
    #define LOG_SWAPCHAIN_SYNCHRONIZATION_EVENTS  0
#endif

FWindowRepresentation::FWindowRepresentation(const std::unique_ptr<Window>& pWnd, bool bVSyncIn, bool bFullscreenIn)
    : hwnd(pWnd->GetHWND())
    , width(pWnd->GetWidth())
    , height(pWnd->GetHeight())
    , bVSync(bVSyncIn)
    , bFullscreen(bFullscreenIn)
{}

bool SwapChain::Create(const FSwapChainCreateDesc& desc)
{
    assert(desc.pDevice);
    assert(desc.pWindow && desc.pWindow->hwnd);
    assert(desc.pCmdQueue && desc.pCmdQueue->pQueue);
    assert(desc.numBackBuffers > 0 && desc.numBackBuffers <= NUM_MAX_BACK_BUFFERS);

    mpDevice = desc.pDevice;
    mHwnd = desc.pWindow->hwnd;
    mNumBackBuffers = desc.numBackBuffers;
    mpPresentQueue = desc.pCmdQueue->pQueue;

    HRESULT hr = {};
    IDXGIFactory4* pDxgiFactory = nullptr;
    hr = CreateDXGIFactory1(IID_PPV_ARGS(&pDxgiFactory));
    if (FAILED(hr))
    {
        Log::Error("SwapChain::Create(): Couldn't create DXGI Factory.");
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = desc.numBackBuffers;
    swapChainDesc.Height = desc.pWindow->height;
    swapChainDesc.Width  = desc.pWindow->width;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.Flags = desc.pWindow->bVSync
        ? 0
        : DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    if (desc.pWindow->bFullscreen)
    {
        swapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    }

    IDXGISwapChain1* pSwapChain = nullptr;
    hr = pDxgiFactory->CreateSwapChainForHwnd(
        desc.pCmdQueue->pQueue
        , desc.pWindow->hwnd
        , &swapChainDesc
        , NULL
        , NULL
        , &pSwapChain
    );

    UINT WndAssocFlags = DXGI_MWA_NO_ALT_ENTER; // We're gonna handle the Alt+Enter ourselves instead of DXGI
    pDxgiFactory->MakeWindowAssociation(desc.pWindow->hwnd, WndAssocFlags);

    const bool bSuccess = SUCCEEDED(hr);
    if (bSuccess)
    {
        pSwapChain->QueryInterface(IID_PPV_ARGS(&this->mpSwapChain));
        pSwapChain->Release();
    }
    else
    {
        std::string reason;
        switch (hr)
        {
        case E_OUTOFMEMORY: reason = "Out of memory"; break;
        case DXGI_ERROR_INVALID_CALL: reason = "DXGI Invalid call"; break;
            // lookup: https://docs.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-error
        default: reason = "UNKNOWN"; break;
        }
        Log::Error("Couldn't create Swapchain: %s", reason.c_str());
    }

    pDxgiFactory->Release();

    // Create Fence & Fence Event
    this->mFenceValues.resize(this->mNumBackBuffers, 0);
    D3D12_FENCE_FLAGS FenceFlags = D3D12_FENCE_FLAG_NONE;
    mpDevice->CreateFence(this->mFenceValues[this->mICurrentBackBuffer], FenceFlags, IID_PPV_ARGS(&this->mpFence));
    ++mFenceValues[mICurrentBackBuffer];
    mHEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (mHEvent == nullptr)
    {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }

    // -- Create the Back Buffers (render target views) Descriptor Heap -- //
    // describe an rtv descriptor heap and create
    D3D12_DESCRIPTOR_HEAP_DESC RTVHeapDesc = {};
    RTVHeapDesc.NumDescriptors = this->mNumBackBuffers; // number of descriptors for this heap.
    RTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // this heap is a render target view heap

    // This heap will not be directly referenced by the shaders (not shader visible), as this will store the output from the pipeline
    // otherwise we would set the heap's flag to D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
    RTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    ID3D12DescriptorHeap* rtvDescriptorHeap = nullptr;
    hr = this->mpDevice->CreateDescriptorHeap(&RTVHeapDesc, IID_PPV_ARGS(&this->mpDescHeapRTV));
    if (FAILED(hr))
    {
        Log::Error("SwapChain::Create() : Couldn't create DescriptorHeap: %0x%x", hr);
        return false;
    }
#if _DEBUG
    mpDescHeapRTV->SetName(L"SwapChainRTVDescHeap");
#endif

    // Create a RTV for each SwapChain buffer
    this->mRenderTargets.resize(this->mNumBackBuffers, nullptr);

    // Resize will handle RTV creation logic if its a Fullscreen SwapChain
    if (desc.pWindow->bFullscreen)
    {
        // TODO: the SetFullscreen here doesn't trigger WM_SIZE event, hence
        //       we have to call here. For now, we use the specified w and h,
        //       however, that results in non native screen resolution (=low res fullscreen).
        //       Need to figure out how to properly start a swapchain in fullscreen mode.
        this->SetFullscreen(true, desc.pWindow->width, desc.pWindow->height);
        this->Resize(desc.pWindow->width, desc.pWindow->height);
    }
    else
    {
        CreateRenderTargetViews();
    }


    Log::Info("SwapChain: Created swapchain<hwnd=0x%x> w/ %d back buffers of %dx%d.", mHwnd, desc.numBackBuffers, desc.pWindow->width, desc.pWindow->height);
    return bSuccess;
}

void SwapChain::Destroy()
{
    // https://docs.microsoft.com/en-us/windows/win32/direct3d12/swap-chains
    // Full-screen swap chains continue to have the restriction that 
    // SetFullscreenState(FALSE, NULL) must be called before the final 
    // release of the swap chain. 
    mpSwapChain->SetFullscreenState(FALSE, NULL);

    WaitForGPU();

    this->mpFence->Release();
    CloseHandle(this->mHEvent);

    DestroyRenderTargetViews();
    if (this->mpDescHeapRTV) this->mpDescHeapRTV->Release();
    if (this->mpSwapChain)   this->mpSwapChain->Release();
}

void SwapChain::Resize(int w, int h)
{
#if LOG_SWAPCHAIN_VERBOSE
    Log::Info("SwapChain<hwnd=0x%x> Resize: %dx%d", mHwnd, w, h);
#endif

    DestroyRenderTargetViews();
    for (int i = 0; i < this->mNumBackBuffers; i++)
    {
        mFenceValues[i] = mFenceValues[mpSwapChain->GetCurrentBackBufferIndex()];
    }

    const bool bVsync = false; // TODO
    mpSwapChain->ResizeBuffers(
        (UINT)this->mFenceValues.size(),
        w, h,
        this->mSwapChainFormat,
        bVsync ? 0 : (DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING /*| DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH*/)
    );

    CreateRenderTargetViews();
    this->mICurrentBackBuffer = mpSwapChain->GetCurrentBackBufferIndex();
}

void SwapChain::SetFullscreen(bool bState, int FSRecoveryWindowWidth, int FSRecoveryWindowHeight)
{
    HRESULT hr = {};

    // Set Fullscreen
    // https://docs.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-setfullscreenstate
    hr = mpSwapChain->SetFullscreenState(bState ? TRUE : FALSE, NULL);
    if (hr != S_OK)
    {
        switch (hr)
        {
        case DXGI_ERROR_NOT_CURRENTLY_AVAILABLE : Log::Error("SwapChain::SetFullScreen() : DXGI_ERROR_NOT_CURRENTLY_AVAILABLE"); break;
        case DXGI_STATUS_MODE_CHANGE_IN_PROGRESS: Log::Error("SwapChain::SetFullScreen() : DXGI_STATUS_MODE_CHANGE_IN_PROGRESS "); break;
            
        default: Log::Error("SwapChain::SetFullScreen() : unhandled error code"); break;
        }
    }

    // Figure out which monitor swapchain is in
    // and set the mode we want to use for ResizeTarget().
    IDXGIOutput6* pOut = nullptr;
    {
        IDXGIOutput* pOutput = nullptr;
        mpSwapChain->GetContainingOutput(&pOutput);
        hr = pOutput->QueryInterface(IID_PPV_ARGS(&pOut));
        assert(hr == S_OK);
        pOutput->Release();
    }
    
    DXGI_OUTPUT_DESC1 desc;
    pOut->GetDesc1(&desc);
    
    // Get supported mode count and then all the supported modes
    UINT NumModes = 0;
    hr = pOut->GetDisplayModeList1(DXGI_FORMAT_R8G8B8A8_UNORM, 0, &NumModes, NULL);
    std::vector<DXGI_MODE_DESC1> currMode(NumModes);
    hr = pOut->GetDisplayModeList1(DXGI_FORMAT_R8G8B8A8_UNORM, 0, &NumModes, &currMode[0]);

    DXGI_MODE_DESC1 matchDesc = {};
    matchDesc = currMode.back(); // back() usually has the highest resolution + refresh rate (not always the refresh rate)
    matchDesc.RefreshRate.Numerator = matchDesc.RefreshRate.Denominator = 0;  // let DXGI figure out the refresh rate
    if (!bState)
    {
        matchDesc.Width = FSRecoveryWindowWidth;
        matchDesc.Height = FSRecoveryWindowHeight;
    }
    DXGI_MODE_DESC1 matchedDesc = {};
    pOut->FindClosestMatchingMode1(&matchDesc, &matchedDesc, NULL);
    pOut->Release();

    DXGI_MODE_DESC mode = {};
    mode.Width            = matchedDesc.Width;
    mode.Height           = matchedDesc.Height;
    mode.RefreshRate      = matchedDesc.RefreshRate;
    mode.Format           = matchedDesc.Format;
    mode.Scaling          = matchedDesc.Scaling;
    mode.ScanlineOrdering = matchedDesc.ScanlineOrdering;
    // https://docs.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-resizetarget
    // calling ResizeTarget() will produce WM_RESIZE event right away.
    // RenderThread handles events twice before present to be able to catch
    // the Resize() event before calling Present() as the events
    // are produced and consumed on different threads (p:main, c:render).
    hr = mpSwapChain->ResizeTarget(&mode);
    if (hr != S_OK)
    {
        Log::Error("SwapChain::ResizeTarget() : unhandled error code");
    }


    const bool bRefreshRateIsInteger = mode.RefreshRate.Denominator == 1;
    if(bRefreshRateIsInteger)
        Log::Info("SwapChain::SetFullscreen() Mode: %dx%d@%dHz" , mode.Width, mode.Height, mode.RefreshRate.Numerator);
    else
        Log::Info("SwapChain::SetFullscreen() Mode: %dx%d@%.2fHz", mode.Width, mode.Height, (float)mode.RefreshRate.Numerator / mode.RefreshRate.Denominator);
}

bool SwapChain::IsFullscreen(/*IDXGIOUtput* ?*/) const
{
    BOOL fullscreenState;
    ThrowIfFailed(mpSwapChain->GetFullscreenState(&fullscreenState, nullptr));
    return fullscreenState;
}

HRESULT SwapChain::Present(bool bVSync)
{
    constexpr UINT VSYNC_INTERVAL = 1;

    // TODO: glitch detection and avoidance
    // https://docs.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-flip-model#avoiding-detecting-and-recovering-from-glitches

    HRESULT hr = {};
    UINT FlagPresent = (!bVSync && !IsFullscreen())
        ? DXGI_PRESENT_ALLOW_TEARING // works only in Windowed mode
        : 0;

    if (bVSync) hr = mpSwapChain->Present(VSYNC_INTERVAL, FlagPresent);
    else        hr = mpSwapChain->Present(0, FlagPresent);

    if (hr != S_OK)
    {
        switch (hr)
        {
        case DXGI_ERROR_DEVICE_RESET:
            Log::Error("SwapChain::Present(): DXGI_ERROR_DEVICE_RESET");
            // TODO: call HandleDeviceReset() from whoever will be responsible
            break;
        case DXGI_ERROR_DEVICE_REMOVED:
            Log::Error("SwapChain::Present(): DXGI_ERROR_DEVICE_REMOVED");
            // TODO: call HandleDeviceReset()
            break;
        case DXGI_ERROR_INVALID_CALL:
            Log::Error("SwapChain::Present(): DXGI_ERROR_INVALID_CALL");
            // TODO:
            break;
        case DXGI_STATUS_OCCLUDED:
            Log::Warning("SwapChain::Present(): DXGI_STATUS_OCCLUDED");
            break;
        default:
            assert(false); // unhandled Present() return code
            break;
        }
    }

    return hr;
}

void SwapChain::MoveToNextFrame()
{
#if LOG_SWAPCHAIN_SYNCHRONIZATION_EVENTS
    Log::Info("MoveToNextFrame() Begin : hwnd=0x%x iBackBuff=%d / Frame=%d / FenceVal=%d"
        , mHwnd, mICurrentBackBuffer, mNumTotalFrames, mFenceValues[mICurrentBackBuffer]
    );
#endif


    // Schedule a Signal command in the queue.
    const UINT64 currentFenceValue = mFenceValues[mICurrentBackBuffer];
    ThrowIfFailed(mpPresentQueue->Signal(mpFence, currentFenceValue));

    // Update the frame index.
    mICurrentBackBuffer = mpSwapChain->GetCurrentBackBufferIndex();
    ++mNumTotalFrames;

    // If the next frame is not ready to be rendered yet, wait until it is ready.
    UINT64 fenceComplVal = mpFence->GetCompletedValue();
    HRESULT hr = {};
    if (fenceComplVal < mFenceValues[mICurrentBackBuffer])
    {
#if LOG_SWAPCHAIN_SYNCHRONIZATION_EVENTS
        Log::Warning("SwapChain : next frame not ready. FenceComplVal=%d < FenceVal[curr]=%d", fenceComplVal, mFenceValues[mICurrentBackBuffer]);
#endif
        ThrowIfFailed(mpFence->SetEventOnCompletion(mFenceValues[mICurrentBackBuffer], mHEvent));
        hr = WaitForSingleObjectEx(mHEvent, 200, FALSE);
    }
    switch (hr)
    {
    case WAIT_TIMEOUT:
        Log::Warning("SwapChain<hwnd=0x%x> timed out on WaitForGPU(): Signal=%d, ICurrBackBuffer=%d, NumFramesPresented=%d"
            , mHwnd, mFenceValues[mICurrentBackBuffer], mICurrentBackBuffer, this->GetNumPresentedFrames());
        break;
    default: break;
    }

    assert(hr == S_OK);

    // Set the fence value for the next frame.
    mFenceValues[mICurrentBackBuffer] = currentFenceValue + 1;


#if LOG_SWAPCHAIN_SYNCHRONIZATION_EVENTS
    Log::Info("MoveToNextFrame() End   : hwnd=0x%x iBackBuff=%d / Frame=%d / FenceVal=%d"
        , mHwnd, mICurrentBackBuffer, mNumTotalFrames, mFenceValues[mICurrentBackBuffer]
    );
#endif
}

void SwapChain::WaitForGPU()
{
    ID3D12Fence* pFence;
    ThrowIfFailed(mpDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pFence)));

    ID3D12CommandQueue* queue = mpPresentQueue;

    ThrowIfFailed(queue->Signal(pFence, 1));

    HANDLE mHandleFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    pFence->SetEventOnCompletion(1, mHandleFenceEvent);
    WaitForSingleObject(mHandleFenceEvent, INFINITE);
    CloseHandle(mHandleFenceEvent);

    pFence->Release();
}

void SwapChain::CreateRenderTargetViews()
{
    // From Adam Sawicki's D3D12MA sample code:
    // get the size of a descriptor in this heap (this is a rtv heap, so only rtv descriptors should be stored in it.
    // descriptor sizes may vary from g_Device to g_Device, which is why there is no set size and we must ask the 
    // g_Device to give us the size. we will use this size to increment a descriptor handle offset
    const UINT RTVDescSize = this->mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE hRTV{ this->mpDescHeapRTV->GetCPUDescriptorHandleForHeapStart() };

    HRESULT hr = {};
    for (int i = 0; i < this->mNumBackBuffers; i++)
    {
        hr = this->mpSwapChain->GetBuffer(i, IID_PPV_ARGS(&this->mRenderTargets[i]));
        if (FAILED(hr))
        {
            assert(false);
        }

        this->mpDevice->CreateRenderTargetView(this->mRenderTargets[i], nullptr, hRTV);
        hRTV.ptr += RTVDescSize;
    }
}

void SwapChain::DestroyRenderTargetViews()
{
    for (int i = 0; i < this->mNumBackBuffers; i++)
    {
        if (this->mRenderTargets[i])
        {
            this->mRenderTargets[i]->Release();
            this->mRenderTargets[i] = nullptr;
        }
    }
}
