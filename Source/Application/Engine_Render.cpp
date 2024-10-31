#include "Engine.h"
#include "Geometry.h"

#include <d3d12.h>
#include <dxgi.h>


// ------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// MAIN
//
// ------------------------------------------------------------------------------------------------------------------------------------------------------------
void Engine::RenderThread_Main()
{
	Log::Info("RenderThread_Main()");
	RenderThread_Inititalize();


	bool bQuit = false;
	while (!this->mbStopAllThreads && !bQuit)
	{
		RenderThread_HandleEvents();

		RenderThread_WaitForUpdateThread();

#if DEBUG_LOG_THREAD_SYNC_VERBOSE
		Log::Info(/*"RenderThread_Tick() : */"r%d (u=%llu)", mNumRenderLoopsExecuted.load(), mNumUpdateLoopsExecuted.load());
#endif

		RenderThread_PreRender();
		RenderThread_Render();

		++mNumRenderLoopsExecuted;

		RenderThread_SignalUpdateThread();

		RenderThread_HandleEvents();
	}

	RenderThread_Exit();
	Log::Info("RenderThread_Main() : Exit");
}


void Engine::RenderThread_WaitForUpdateThread()
{
#if DEBUG_LOG_THREAD_SYNC_VERBOSE
	Log::Info("r:wait : u=%llu, r=%llu", mNumUpdateLoopsExecuted.load(), mNumRenderLoopsExecuted.load());
#endif

	mpSemRender->Wait();
}

void Engine::RenderThread_SignalUpdateThread()
{
	mpSemUpdate->Signal();
}

static bool CheckInitialSwapchainResizeRequired(std::unordered_map<HWND, bool>& map, const FWindowSettings& setting, HWND hwnd)
{
	const bool bExclusiveFullscreen = setting.DisplayMode == EDisplayMode::EXCLUSIVE_FULLSCREEN;
	if (bExclusiveFullscreen)
	{
		map[hwnd] = true;
	}
	return bExclusiveFullscreen;
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// INITIALIZE
//
// ------------------------------------------------------------------------------------------------------------------------------------------------------------
void Engine::RenderThread_Inititalize()
{
	const bool bExclusiveFullscreen_MainWnd = CheckInitialSwapchainResizeRequired(mInitialSwapchainResizeRequiredWindowLookup,
																					mSettings.WndMain, 
																					mpWinMain->GetHWND());

	FRendererInitializeParameters params = {};
	params.Settings = mSettings.gfx;
	params.Windows.push_back(FWindowRepresentation(mpWinMain, mSettings.gfx.bVsync, bExclusiveFullscreen_MainWnd));

	mRenderer.Initialize(params);
	mbRenderThreadInitialized.store(true);

	mNumRenderLoopsExecuted.store(0);

	InitializeBuiltinMeshes();
	mRenderer.Load();

	mResources_MainWnd.DSV_MainViewDepth = mRenderer.CreateDSV();


	// ---------- Window initialziation
	if (!mpWinMain) return;

	// Borderless fullscreen transitions are handled through Window object
	// Exclusive  fullscreen transitions are handled through the Swapchain
	if (mpWinMain)
	{
		// Borderless fullscreen transitions are handled through Window object
		// Exclusive  fullscreen transitions are handled through the Swapchain
		if (mSettings.WndMain.DisplayMode == EDisplayMode::BORDERLESS_FULLSCREEN)
		{
			#if 0 // TODO: Initial borderless fullscreen window rect is bugged. Default to Primary Display.
				mpWinMain->ToggleWindowedFullscreen(&mRenderer.GetWindowSwapChain(pWin->GetHWND()));
			#else
				mpWinMain->ToggleWindowedFullscreen();
			#endif
		}
		if (mSettings.WndMain.DisplayMode == EDisplayMode::EXCLUSIVE_FULLSCREEN)
		{
			mpWinMain->SetMouseCapture(true);
		}
	}

	RenderThread_LoadWindowSizeDependentResources(mpWinMain->GetHWND(), mpWinMain->GetWidth(), mpWinMain->GetHeight());
}

void Engine::RenderThread_Exit()
{
	mRenderer.Unload();
	mRenderer.Exit();
}

void Engine::InitializeBuiltinMeshes()
{
	{
		GeometryGenerator::GeometryData<FVertexWithColorAndAlpha> data = GeometryGenerator::Triangle<FVertexWithColorAndAlpha>(1.0f);
		mBuiltinMeshNames[EBuiltInMeshes::TRIANGLE] = "Triangle";
		mBuiltinMeshes[EBuiltInMeshes::TRIANGLE] = Mesh(&mRenderer, data.Vertices, data.Indices, mBuiltinMeshNames[EBuiltInMeshes::TRIANGLE]);
	}
	{
		GeometryGenerator::GeometryData<FVertexWithColorAndAlpha> data = GeometryGenerator::Cube<FVertexWithColorAndAlpha>();
		mBuiltinMeshNames[EBuiltInMeshes::CUBE] = "Cube";
		mBuiltinMeshes[EBuiltInMeshes::CUBE] = Mesh(&mRenderer, data.Vertices, data.Indices, mBuiltinMeshNames[EBuiltInMeshes::CUBE]);
	}

	// ...
}


// ------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// LOAD
//
// ------------------------------------------------------------------------------------------------------------------------------------------------------------
void Engine::RenderThread_LoadWindowSizeDependentResources(HWND hwnd, int Width, int Height)
{
	if (hwnd == mpWinMain->GetHWND())
	{
		RenderingResources_MainWindow& r = mResources_MainWnd;

		// Main depth stencil view
		D3D12_RESOURCE_DESC d = CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_R32_TYPELESS
			, Width
			, Height
			, 1 // Array Size
			, 0 // MIP levels
			, 1 // MSAA SampleCount
			, 0 // MSAA SampleQuality
			, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
		);
		r.Tex_MainViewDepth = mRenderer.CreateTexture("SceneDepth", d);
		mRenderer.InitializeDSV(r.DSV_MainViewDepth, 0u, r.Tex_MainViewDepth);

	}

	// TODO: generic implementation of other window procedures for load
}

void Engine::RenderThread_UnloadWindowSizeDependentResources(HWND hwnd)
{
	if (hwnd == mpWinMain->GetHWND())
	{
		RenderingResources_MainWindow& r = mResources_MainWnd;

		// sync GPU
		auto& ctx = mRenderer.GetWindowRenderContext(hwnd);
		ctx.SwapChain.WaitForGPU();

		mRenderer.DestroyTexture(r.Tex_MainViewDepth);
	}

	// TODO: generic implementation of other window procedures for unload
}


// ------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// RENDER
//
// ------------------------------------------------------------------------------------------------------------------------------------------------------------
void Engine::RenderThread_PreRender()
{
}

void Engine::RenderThread_Render()
{
	RenderThread_RenderMainWindow();
}


void Engine::RenderThread_RenderMainWindow()
{
	const int NUM_BACK_BUFFERS = mRenderer.GetSwapChainBackBufferCount(mpWinMain);
	const int FRAME_DATA_INDEX = mNumRenderLoopsExecuted % NUM_BACK_BUFFERS;

	HRESULT hr = S_OK; 

	FWindowRenderContext& ctx = mRenderer.GetWindowRenderContext(mpWinMain->GetHWND());
	hr = mbLoadingLevel
		? RenderThread_RenderMainWindow_LoadingScreen(ctx)
		: RenderThread_RenderMainWindow_Scene(ctx);

	// TODO: remove copy paste and use encapsulation of context rendering properly
	// currently check only for hr0 for the mainWindow
	if (hr == DXGI_STATUS_OCCLUDED)
	{
		if (mpWinMain->IsFullscreen())
		{
			mpWinMain->SetFullscreen(false);
			mpWinMain->Show();
		}
	}
}

HRESULT Engine::RenderThread_RenderMainWindow_LoadingScreen(FWindowRenderContext& ctx)
{
	HRESULT hr = S_OK;
	const int NUM_BACK_BUFFERS = ctx.SwapChain.GetNumBackBuffers();
	const int BACK_BUFFER_INDEX = ctx.SwapChain.GetCurrentBackBufferIndex();
	const int FRAME_DATA_INDEX = mNumRenderLoopsExecuted % NUM_BACK_BUFFERS;
	assert(mScene_MainWnd.mLoadingScreenData.size() > 0);
	const FLoadingScreenData& FrameData = mScene_MainWnd.mLoadingScreenData[FRAME_DATA_INDEX];
	assert(ctx.mCommandAllocatorsGFX.size() >= NUM_BACK_BUFFERS);
	// ----------------------------------------------------------------------------

	//
	// PRE RENDER
	//
	// Command list allocators can only be reset when the associated 
	// command lists have finished execution on the GPU; apps should use 
	// fences to determine GPU execution progress.
	ID3D12CommandAllocator* pCmdAlloc = ctx.mCommandAllocatorsGFX[BACK_BUFFER_INDEX];
	ThrowIfFailed(pCmdAlloc->Reset());

	// However, when ExecuteCommandList() is called on a particular command 
	// list, that command list can then be reset at any time and must be before 
	// re-recording.
	ID3D12PipelineState* pInitialState = nullptr;
	ThrowIfFailed(ctx.pCmdList_GFX->Reset(pCmdAlloc, pInitialState));

	//
	// RENDER
	//
	ID3D12GraphicsCommandList* pCmd = ctx.pCmdList_GFX;

	// Transition SwapChain RT
	ID3D12Resource* pSwapChainRT = ctx.SwapChain.GetCurrentBackBufferRenderTarget();
	pCmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pSwapChainRT
		, D3D12_RESOURCE_STATE_PRESENT
		, D3D12_RESOURCE_STATE_RENDER_TARGET)
	);

	// Clear RT
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle = ctx.SwapChain.GetCurrentBackBufferRTVHandle();
	const float clearColor[] =
	{
		FrameData.SwapChainClearColor[0],
		FrameData.SwapChainClearColor[1],
		FrameData.SwapChainClearColor[2],
		FrameData.SwapChainClearColor[3]
	};
	pCmd->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

	// Draw Triangle
	const float           RenderResolutionX = static_cast<float>(ctx.MainRTResolutionX);
	const float           RenderResolutionY = static_cast<float>(ctx.MainRTResolutionY);
	D3D12_VIEWPORT        viewport          { 0.0f, 0.0f, RenderResolutionX, RenderResolutionY, 0.0f, 1.0f };
	const auto            VBIBIDs           = mBuiltinMeshes[EBuiltInMeshes::TRIANGLE].GetIABufferIDs();
	const BufferID&       IB_ID             = VBIBIDs.second;
	const IBV&            ib                = mRenderer.GetIndexBufferView(IB_ID);
	ID3D12DescriptorHeap* ppHeaps[]         = { mRenderer.GetDescHeap(EResourceHeapType::CBV_SRV_UAV_HEAP) };
	D3D12_RECT            scissorsRect      { 0, 0, (LONG)RenderResolutionX, (LONG)RenderResolutionY };

	pCmd->OMSetRenderTargets(1, &rtvHandle, FALSE, NULL);

	pCmd->SetPipelineState(mRenderer.GetPSO(EBuiltinPSOs::LOADING_SCREEN_PSO));
	pCmd->SetGraphicsRootSignature(mRenderer.GetRootSignature(1));

	pCmd->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	pCmd->SetGraphicsRootDescriptorTable(0, mRenderer.GetShaderResourceView(FrameData.SRVLoadingScreen).GetGPUDescHandle());


	pCmd->RSSetViewports(1, &viewport);
	pCmd->RSSetScissorRects(1, &scissorsRect);

	pCmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pCmd->IASetVertexBuffers(0, 1, NULL);
	pCmd->IASetIndexBuffer(&ib);

	pCmd->DrawIndexedInstanced(3, 1, 0, 0, 0);

	pCmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pSwapChainRT
		, D3D12_RESOURCE_STATE_RENDER_TARGET
		, D3D12_RESOURCE_STATE_PRESENT)
	); // Transition SwapChain for Present

	pCmd->Close();

	ID3D12CommandList* ppCommandLists[] = { ctx.pCmdList_GFX };
	ctx.PresentQueue.pQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);


	//
	// PRESENT
	//
	hr = ctx.SwapChain.Present(ctx.bVsync);
	ctx.SwapChain.MoveToNextFrame();
	return hr;
}

HRESULT Engine::RenderThread_RenderMainWindow_Scene(FWindowRenderContext& ctx)
{
	HRESULT hr = S_OK;
	const int NUM_BACK_BUFFERS  = ctx.SwapChain.GetNumBackBuffers();
	const int BACK_BUFFER_INDEX = ctx.SwapChain.GetCurrentBackBufferIndex();
	const int FRAME_DATA_INDEX  = mNumRenderLoopsExecuted % NUM_BACK_BUFFERS;
	const FFrameData& FrameData = mScene_MainWnd.mFrameData[FRAME_DATA_INDEX];
	assert(ctx.mCommandAllocatorsGFX.size() >= NUM_BACK_BUFFERS);
	// ----------------------------------------------------------------------------

	//
	// PRE RENDER
	//
		
	// Dynamic constant buffer maintenance
	ctx.mDynamicHeap_ConstantBuffer.OnBeginFrame();

	// Command list allocators can only be reset when the associated 
	// command lists have finished execution on the GPU; apps should use 
	// fences to determine GPU execution progress.
	ID3D12CommandAllocator* pCmdAlloc = ctx.mCommandAllocatorsGFX[BACK_BUFFER_INDEX];
	ThrowIfFailed(pCmdAlloc->Reset());

	// However, when ExecuteCommandList() is called on a particular command 
	// list, that command list can then be reset at any time and must be before 
	// re-recording.
	ID3D12PipelineState* pInitialState = nullptr;
	ThrowIfFailed(ctx.pCmdList_GFX->Reset(pCmdAlloc, pInitialState));

	ID3D12GraphicsCommandList* pCmd = ctx.pCmdList_GFX;

	//
	// RENDER
	//

	// Transition SwapChain RT
	ID3D12Resource* pSwapChainRT = ctx.SwapChain.GetCurrentBackBufferRenderTarget();

	CD3DX12_RESOURCE_BARRIER barriers[] =
	{
		  CD3DX12_RESOURCE_BARRIER::Transition(pSwapChainRT, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET)
		//, CD3DX12_RESOURCE_BARRIER::Transition(
	};

	pCmd->ResourceBarrier(_countof(barriers), barriers);

	// Clear RT
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle = ctx.SwapChain.GetCurrentBackBufferRTVHandle();
	D3D12_CPU_DESCRIPTOR_HANDLE   dsvHandle = mRenderer.GetDSV(mResources_MainWnd.DSV_MainViewDepth).GetCPUDescHandle();

	D3D12_CLEAR_FLAGS DSVClearFlags = D3D12_CLEAR_FLAGS::D3D12_CLEAR_FLAG_DEPTH;

	const float clearColor[] =
	{
		FrameData.SwapChainClearColor[0],
		FrameData.SwapChainClearColor[1],
		FrameData.SwapChainClearColor[2],
		FrameData.SwapChainClearColor[3]
	};
	pCmd->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	pCmd->ClearDepthStencilView(dsvHandle, DSVClearFlags, 1.0f, 0, 0, nullptr);

	const float RenderResolutionX = static_cast<float>(ctx.MainRTResolutionX);
	const float RenderResolutionY = static_cast<float>(ctx.MainRTResolutionY);
	D3D12_VIEWPORT viewport{ 0.0f, 0.0f, RenderResolutionX, RenderResolutionY, 0.0f, 1.0f };

	D3D12_RECT scissorsRect{ 0, 0, (LONG)RenderResolutionX, (LONG)RenderResolutionY };
	pCmd->RSSetViewports(1, &viewport);
	pCmd->RSSetScissorRects(1, &scissorsRect);

	const Mesh& mesh = mBuiltinMeshes[EBuiltInMeshes::CUBE];
	const auto VBIBIDs = mesh.GetIABufferIDs();
	const uint32 NumIndices = mesh.GetNumIndices();
	const uint32 NumInstances = 1;
	const BufferID& VB_ID = VBIBIDs.first;
	const BufferID& IB_ID = VBIBIDs.second;
	const VBV& vb = mRenderer.GetVertexBufferView(VB_ID);
	const IBV& ib = mRenderer.GetIndexBufferView(IB_ID);

	ID3D12DescriptorHeap* ppHeaps[] = { mRenderer.GetDescHeap(EResourceHeapType::CBV_SRV_UAV_HEAP) };
	// set constant buffer data
	using namespace DirectX;
	const XMMATRIX mMVP
		= FrameData.TFCube.WorldTransformationMatrix()
		* FrameData.SceneCamera.GetViewMatrix()
		* FrameData.SceneCamera.GetProjectionMatrix();
	struct Consts { XMMATRIX matModelViewProj; } consts{ mMVP };

	D3D12_GPU_VIRTUAL_ADDRESS cbAddr = {};
	void* pMem = nullptr;
	ctx.mDynamicHeap_ConstantBuffer.AllocConstantBuffer(sizeof(Consts), &pMem, &cbAddr);
	memcpy(pMem, &consts, sizeof(Consts));

	pCmd->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
	pCmd->SetPipelineState(mRenderer.GetPSO(EBuiltinPSOs::HELLO_WORLD_CUBE_PSO));
	pCmd->SetGraphicsRootSignature(mRenderer.GetRootSignature(2));
	pCmd->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	pCmd->SetGraphicsRootDescriptorTable(0, mRenderer.GetSRV(FrameData.CubeTexture).GetGPUDescHandle());
	pCmd->SetGraphicsRootConstantBufferView(1, cbAddr);

	pCmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pCmd->IASetVertexBuffers(0, 1, &vb);
	pCmd->IASetIndexBuffer(&ib);

	pCmd->DrawIndexedInstanced(NumIndices, NumInstances, 0, 0, 0);


	// Transition SwapChain for Present
	pCmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pSwapChainRT
		, D3D12_RESOURCE_STATE_RENDER_TARGET
		, D3D12_RESOURCE_STATE_PRESENT)
	);

	pCmd->Close();

	ID3D12CommandList* ppCommandLists[] = { ctx.pCmdList_GFX };
	ctx.PresentQueue.pQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);


	//
	// PRESENT
	//
	hr = ctx.SwapChain.Present(ctx.bVsync);
	ctx.SwapChain.MoveToNextFrame();
	return hr;
}



// ------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// EVENT HANDLING
//
// ------------------------------------------------------------------------------------------------------------------------------------------------------------
void Engine::RenderThread_HandleEvents()
{
	if (mbStopAllThreads)
		return;

	// Swap event recording buffers so we can read & process a limited number of events safely.
	// Otherwise, theoretically the producer (Main) thread could keep adding new events 
	// while we're spinning on the queue items below, and cause render thread to stall while, say, resizing.
	mWinEventQueue.SwapBuffers();
	std::queue<EventPtr_t>& q = mWinEventQueue.GetBackContainer();
	if (q.empty())
		return;

	// process the events
	std::shared_ptr<IEvent> pEvent = nullptr;
	std::shared_ptr<WindowResizeEvent> pResizeEvent = nullptr;
	while (!q.empty())
	{
		pEvent = std::move(q.front());
		q.pop();

		switch (pEvent->mType)
		{
		case EEventType::WINDOW_RESIZE_EVENT: pResizeEvent = std::static_pointer_cast<WindowResizeEvent>(pEvent); break;
		case EEventType::TOGGLE_FULLSCREEN_EVENT: RenderThread_HandleToggleFullscreenEvent(pEvent.get()); break;
		case EEventType::WINDOW_CLOSE_EVENT: RenderThread_HandleWindowCloseEvent(pEvent.get()); break;
		}
	}
	
	if (pResizeEvent) // Process only one Window Resize, if any.
	{
		RenderThread_HandleWindowResizeEvent(pResizeEvent.get());
	}
	
}

void Engine::RenderThread_HandleWindowResizeEvent(const IEvent* pEvent)
{
	const WindowResizeEvent* pResizeEvent = static_cast<const WindowResizeEvent*>(pEvent);
	const HWND&                      hwnd = pResizeEvent->hwnd;
	const int                       WIDTH = pResizeEvent->width;
	const int                      HEIGHT = pResizeEvent->height;
	SwapChain&                  Swapchain = mRenderer.GetWindowSwapChain(hwnd);
	std::unique_ptr<Window>&         pWnd = GetWindow(hwnd);

	if (pWnd->IsClosed())
	{
		Log::Warning("RenderThread: Ignoring WindowResizeEvent as Window<%x> is closed.", pWnd->GetHWND());
		return;
	}

	Log::Info("RenderThread: Handle Resize event, set resolution to %dx%d", WIDTH , HEIGHT);

	Swapchain.WaitForGPU();
	Swapchain.Resize(WIDTH, HEIGHT);
	pWnd->OnResize(WIDTH, HEIGHT);
	mRenderer.OnWindowSizeChanged(hwnd, WIDTH, HEIGHT);

	RenderThread_UnloadWindowSizeDependentResources(hwnd);
	RenderThread_LoadWindowSizeDependentResources(hwnd, WIDTH, HEIGHT);
}

void Engine::RenderThread_HandleWindowCloseEvent(const IEvent* pEvent)
{
	const WindowCloseEvent* pWindowCloseEvent = static_cast<const WindowCloseEvent*>(pEvent);
	const HWND& hwnd = pWindowCloseEvent->hwnd;
	SwapChain& Swapchain = mRenderer.GetWindowSwapChain(hwnd);
	std::unique_ptr<Window>& pWnd = GetWindow(hwnd);
	Log::Info("RenderThread: Handle Window Close event <%x>", hwnd);
	RenderThread_UnloadWindowSizeDependentResources(hwnd);
	pWindowCloseEvent->mCondVar.notify_all();
	if (hwnd == mpWinMain->GetHWND())
	{
		mbStopAllThreads.store(true);
		RenderThread_SignalUpdateThread();
	}
}


void Engine::RenderThread_HandleToggleFullscreenEvent(const IEvent* pEvent)
{
	const ToggleFullscreenEvent* pToggleFSEvent = static_cast<const ToggleFullscreenEvent*>(pEvent);
	HWND                                   hwnd = pToggleFSEvent->hwnd;
	SwapChain&                        Swapchain = mRenderer.GetWindowSwapChain(pToggleFSEvent->hwnd);
	const FWindowSettings&          WndSettings = GetWindowSettings(hwnd);
	const bool   bExclusiveFullscreenTransition = WndSettings.DisplayMode == EDisplayMode::EXCLUSIVE_FULLSCREEN;
	const bool            bFullscreenStateToSet = !Swapchain.IsFullscreen();
	std::unique_ptr<Window>&               pWnd = GetWindow(hwnd);

	Log::Info("RenderThread: Handle Fullscreen(exclusiveFS=%s), %s %dx%d"
		, (bFullscreenStateToSet ? "true" : "false")
		, (bFullscreenStateToSet ? "RestoreSize: " : "Transition to: ")
		, WndSettings.Width
		, WndSettings.Height
	);

	// if we're transitioning into Fullscreen, save the current window dimensions
	if (bFullscreenStateToSet)
	{
		FWindowSettings& WndSettings_ = GetWindowSettings(hwnd);
		WndSettings_.Width  = pWnd->GetWidth();
		WndSettings_.Height = pWnd->GetHeight();
	}

	Swapchain.WaitForGPU(); // make sure GPU is finished


	//
	// EXCLUSIVE FULLSCREEN
	//
	if (bExclusiveFullscreenTransition)
	{
		// Swapchain handles resizing the window through SetFullscreenState() call
		Swapchain.SetFullscreen(bFullscreenStateToSet, WndSettings.Width, WndSettings.Height);

		pWnd->SetMouseCapture(bFullscreenStateToSet);
		
		if (!bFullscreenStateToSet)
		{
			// If the Swapchain is created in fullscreen mode, the WM_PAINT message will not be 
			// received upon switching to windowed mode (ALT+TAB or ALT+ENTER) and the window
			// will be visible, but not interactable and also not visible in taskbar.
			// Explicitly calling Show() here fixes the situation.
			pWnd->Show();

			// Handle one-time transition: Swapchains starting in exclusive fullscreen will not trigger
			// a Resize() event on the first Alt+Tab (Fullscreen -> Windowed). Doing it always in Swapchain.Resize()
			// will result flickering from double resizing.
			auto it = mInitialSwapchainResizeRequiredWindowLookup.find(hwnd);
			const bool bWndNeedsResize = it != mInitialSwapchainResizeRequiredWindowLookup.end() && it->second;
			if (bWndNeedsResize)
			{
				mInitialSwapchainResizeRequiredWindowLookup.erase(it);
				Swapchain.Resize(WndSettings.Width, WndSettings.Height);
			}
		}
	}

	//
	// BORDERLESS FULLSCREEN
	//
	else 
	{
		pWnd->ToggleWindowedFullscreen(&Swapchain);
	}

	const int WIDTH  = bFullscreenStateToSet ? pWnd->GetFullscreenWidth()  : pWnd->GetWidth();
	const int HEIGHT = bFullscreenStateToSet ? pWnd->GetFullscreenHeight() : pWnd->GetHeight();
	RenderThread_UnloadWindowSizeDependentResources(hwnd);
	RenderThread_LoadWindowSizeDependentResources(hwnd, WIDTH, HEIGHT);
}

