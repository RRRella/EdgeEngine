#include "Engine.h"
#include "Geometry.h"

#include "Libs/imgui/backends/imgui_impl_win32.h"
#include "Libs/imgui/backends/imgui_impl_dx12.h"
#include "Libs/imgui/imgui.h"

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

	RenderThread_HandleEvents();

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



// ------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// INITIALIZE
//
// ------------------------------------------------------------------------------------------------------------------------------------------------------------
static bool CheckInitialSwapchainResizeRequired(std::unordered_map<HWND, bool>& map, const FWindowSettings& setting, HWND hwnd)
{
	const bool bExclusiveFullscreen = setting.DisplayMode == EDisplayMode::EXCLUSIVE_FULLSCREEN;
	if (bExclusiveFullscreen)
	{
		map[hwnd] = true;
	}
	return bExclusiveFullscreen;
}
void Engine::RenderThread_Inititalize()
{
	const bool bExclusiveFullscreen_MainWnd = CheckInitialSwapchainResizeRequired(mInitialSwapchainResizeRequiredWindowLookup, mSettings.WndMain, mpWinMain->GetHWND());

	mNumRenderLoopsExecuted.store(0);

	// Initialize Renderer: Device, Queues, Heaps
	FRendererInitializeParameters params = {};
	params.Settings = mSettings.gfx;

	params.Windows.push_back(FWindowRepresentation(mpWinMain , mSettings.gfx.bVsync, bExclusiveFullscreen_MainWnd));
	mRenderer.Initialize(params);

	// Initialize swapchains & respective render targets
	for ( FWindowRepresentation& wnd : params.Windows)
	{
		mRenderer.InitializeRenderContext(wnd, params.Settings.bUseTripleBuffering ? 3 : 2);
		mEventQueue_EdgineToWin_Main.AddItem(std::make_shared<HandleWindowTransitionsEvent>(wnd.hwnd));
	}

	mbRenderThreadInitialized.store(true);


	//
	// TODO: THREADED LOADING
	// 
	// initialize builtin meshes
	InitializeBuiltinMeshes();

	// load renderer resources
	mRenderer.LoadPSOs();
	mRenderer.Load();

	// TODO:
	mResources_MainWnd.DSV_MainViewDepth = mRenderer.CreateDSV();

	// load window resources
	const bool bFullscreen = mpWinMain->IsFullscreen();
	const int W = bFullscreen ? mpWinMain->GetFullscreenWidth() : mpWinMain->GetWidth();
	const int H = bFullscreen ? mpWinMain->GetFullscreenHeight() : mpWinMain->GetHeight();
	RenderThread_LoadWindowSizeDependentResources(mpWinMain->GetHWND(), W, H);

	const std::string TextureFilePath = "Data/missing.jpg";
	TextureID texID = mRenderer.CreateTextureFromFile(TextureFilePath.c_str());
	SRV_ID    srvID = mRenderer.CreateAndInitializeSRV(texID);

	for (int i = 0; i < mScene_MainWnd.mFrameData.size(); ++i)
		mScene_MainWnd.mFrameData[i].SRVObjectTex = srvID;

	RenderThread_Initialize_Imgui();
}

void Engine::RenderThread_Initialize_Imgui()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(mpWinMain->GetHWND());
	ImGui_ImplDX12_Init(mRenderer.GetWindowRenderContext(mpWinMain->GetHWND()).pDevice->GetDevicePtr(), mRenderer.GetSwapChainBackBufferCount(mpWinMain->GetHWND()),
		DXGI_FORMAT_R8G8B8A8_UNORM, mRenderer.GetImguiHeap(),
		mRenderer.GetImguiHeap()->GetCPUDescriptorHandleForHeapStart(),
		mRenderer.GetImguiHeap()->GetGPUDescriptorHandleForHeapStart());
}

void Engine::RenderThread_Exit()
{
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	mRenderer.Unload();
	mRenderer.Exit();
}

void Engine::InitializeBuiltinMeshes()
{
	{
		GeometryGenerator::GeometryData<FVertexWithColorAndAlpha> data = GeometryGenerator::Triangle<FVertexWithColorAndAlpha>(1.0f);
		mBuiltinMeshNames[EBuiltInMeshes::TRIANGLE] = "Triangle";
		mBuiltinMeshes[EBuiltInMeshes::TRIANGLE] = std::make_shared<Mesh>(&mRenderer, data.Vertices, data.Indices, mBuiltinMeshNames[EBuiltInMeshes::TRIANGLE]);
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
	assert(Width >= 1 && Height >= 1);

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
		ctx.mSwapChain.WaitForGPU();

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
	const int NUM_BACK_BUFFERS = mRenderer.GetSwapChainBackBufferCount(mpWinMain);
	const int FRAME_DATA_INDEX  = mNumRenderLoopsExecuted % NUM_BACK_BUFFERS;
	
	RenderThread_DrawImguiWidgets();

	RenderThread_RenderMainWindow();
}

void Engine::RenderThread_DrawImguiWidgets()
{
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();


	//Setting
	ImGui::Begin("Settings");

	ImGui::DragFloat("Dragging Sensitivity", &mMouseDragSensitivity, 0.01f, 0.5f, 10.0f, "%.2f");
	ImGui::DragFloat("Rotational Sensitivity", &mMouseRotSensitivity, 0.01f, 0.01f, 10.0f, "%.2f");
	ImGui::DragFloat("Zoom Sensitivity", &mMouseZoomSensitivity, 0.01f, 1.0f, 10.0f, "%.2f");
	ImGui::DragFloat("Scale", &mScale, 0.01f, 0.001f, 10.0f, "%.3f");

	ImGui::End();

	//Menu bar
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Load Mesh"))
			{
				if (!mBuiltinMeshes[EBuiltInMeshes::OBJ_FILE])
				{
					const char* filter = "OBJ(*.obj)\0*.obj";

					GeometryGenerator::GeometryData<FVertexWithNormal> data;
					GeometryGenerator::LoadObjFromFile<FVertexWithNormal>(OpenFile(filter), data);

					mBuiltinMeshNames[EBuiltInMeshes::OBJ_FILE] = "OBJ_File";
					mBuiltinMeshes[EBuiltInMeshes::OBJ_FILE] = std::make_shared<Mesh>(&mRenderer, data.Vertices, data.Indices, mBuiltinMeshNames[EBuiltInMeshes::OBJ_FILE]);
				}
				else
				{
					Log::Warning("Clear the current mesh then load another");
				}
			}
			if (ImGui::MenuItem("Change Texture"))
			{
				const char* filter = "Image Files (*.jpg;*.png)\0*.jpg;*.png";

				const std::string TextureFilePath = OpenFile(filter);
				TextureID texID = mRenderer.CreateTextureFromFile(TextureFilePath.c_str());
				SRV_ID    srvID = mRenderer.CreateAndInitializeSRV(texID);

				for (int i = 0; i < mScene_MainWnd.mFrameData.size(); ++i)
					mScene_MainWnd.mFrameData[i].SRVObjectTex = srvID;

				mRenderer.SetStallTexture(mRenderer.GetSwapChainCurrentBackBufferIndex(mpWinMain->GetHWND()) , texID - 1);
			}
			if (ImGui::MenuItem("Clear"))
			{
				mBuiltinMeshes[EBuiltInMeshes::OBJ_FILE].reset();
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	ImGui::Render();
}


void Engine::RenderThread_RenderMainWindow()
{
	const int NUM_BACK_BUFFERS = mRenderer.GetSwapChainBackBufferCount(mpWinMain);
	const int FRAME_DATA_INDEX = mNumRenderLoopsExecuted % NUM_BACK_BUFFERS;

	HRESULT hr = S_OK;

	FWindowRenderContext& ctx = mRenderer.GetWindowRenderContext(mpWinMain->GetHWND());
	hr = RenderThread_RenderMainWindow_Scene(ctx);

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


//HRESULT Engine::RenderThread_RenderMainWindow_LoadingScreen(FWindowRenderContext& ctx)
//{
//	HRESULT hr = S_OK;
//	const int NUM_BACK_BUFFERS = ctx.SwapChain.GetNumBackBuffers();
//	const int BACK_BUFFER_INDEX = ctx.SwapChain.GetCurrentBackBufferIndex();
//	const int FRAME_DATA_INDEX = mNumRenderLoopsExecuted % NUM_BACK_BUFFERS;
//	assert(mScene_MainWnd.mLoadingScreenData.size() > 0);
//	const FLoadingScreenData& FrameData = mScene_MainWnd.mLoadingScreenData[FRAME_DATA_INDEX];
//	assert(ctx.mCommandAllocatorsGFX.size() >= NUM_BACK_BUFFERS);
//	// ----------------------------------------------------------------------------
//
//	//
//	// PRE RENDER
//	//
//	// Command list allocators can only be reset when the associated 
//	// command lists have finished execution on the GPU; apps should use 
//	// fences to determine GPU execution progress.
//	ID3D12CommandAllocator* pCmdAlloc = ctx.mCommandAllocatorsGFX[BACK_BUFFER_INDEX];
//	ThrowIfFailed(pCmdAlloc->Reset());
//
//	// However, when ExecuteCommandList() is called on a particular command 
//	// list, that command list can then be reset at any time and must be before 
//	// re-recording.
//	ID3D12PipelineState* pInitialState = nullptr;
//	ThrowIfFailed(ctx.pCmdList_GFX->Reset(pCmdAlloc, pInitialState));
//
//	//
//	// RENDER
//	//
//	ID3D12GraphicsCommandList* pCmd = ctx.pCmdList_GFX;
//
//	// Transition SwapChain RT
//	ID3D12Resource* pSwapChainRT = ctx.SwapChain.GetCurrentBackBufferRenderTarget();
//	pCmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pSwapChainRT
//		, D3D12_RESOURCE_STATE_PRESENT
//		, D3D12_RESOURCE_STATE_RENDER_TARGET)
//	);
//
//	// Clear RT
//	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle = ctx.SwapChain.GetCurrentBackBufferRTVHandle();
//	const float clearColor[] =
//	{
//		FrameData.SwapChainClearColor[0],
//		FrameData.SwapChainClearColor[1],
//		FrameData.SwapChainClearColor[2],
//		FrameData.SwapChainClearColor[3]
//	};
//	pCmd->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
//
//	// Draw Triangle
//	const float           RenderResolutionX = static_cast<float>(ctx.MainRTResolutionX);
//	const float           RenderResolutionY = static_cast<float>(ctx.MainRTResolutionY);
//	D3D12_VIEWPORT        viewport          { 0.0f, 0.0f, RenderResolutionX, RenderResolutionY, 0.0f, 1.0f };
//	const auto            VBIBIDs           = mBuiltinMeshes[EBuiltInMeshes::TRIANGLE]->GetIABufferIDs();
//	const BufferID&       IB_ID             = VBIBIDs.second;
//	const IBV&            ib                = mRenderer.GetIndexBufferView(IB_ID);
//	ID3D12DescriptorHeap* ppHeaps[]         = { mRenderer.GetDescHeap(EResourceHeapType::CBV_SRV_UAV_HEAP) };
//	D3D12_RECT            scissorsRect      { 0, 0, (LONG)RenderResolutionX, (LONG)RenderResolutionY };
//
//	pCmd->OMSetRenderTargets(1, &rtvHandle, FALSE, NULL);
//
//	pCmd->SetPipelineState(mRenderer.GetPSO(EBuiltinPSOs::LOADING_SCREEN_PSO));
//
//	// hardcoded roog signature for now until shader reflection and rootsignature management is implemented
//	pCmd->SetGraphicsRootSignature(mRenderer.GetRootSignature(1));
//
//	pCmd->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
//	pCmd->SetGraphicsRootDescriptorTable(0, mRenderer.GetShaderResourceView(FrameData.SRVLoadingScreen).GetGPUDescHandle());
//
//	pCmd->RSSetViewports(1, &viewport);
//	pCmd->RSSetScissorRects(1, &scissorsRect);
//
//	pCmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
//	pCmd->IASetVertexBuffers(0, 1, NULL);
//	pCmd->IASetIndexBuffer(&ib);
//
//	pCmd->DrawIndexedInstanced(3, 1, 0, 0, 0);
//
//	pCmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pSwapChainRT
//		, D3D12_RESOURCE_STATE_RENDER_TARGET
//		, D3D12_RESOURCE_STATE_PRESENT)
//	); // Transition SwapChain for Present
//
//	pCmd->Close();
//
//	ID3D12CommandList* ppCommandLists[] = { ctx.pCmdList_GFX };
//	ctx.PresentQueue.pQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
//
//
//	//
//	// PRESENT
//	//
//	hr = ctx.SwapChain.Present(ctx.bVsync);
//	ctx.SwapChain.MoveToNextFrame();
//	return hr;
//}

HRESULT Engine::RenderThread_RenderMainWindow_Scene(FWindowRenderContext& ctx)
{
	HRESULT hr = S_OK;
	const int NUM_BACK_BUFFERS = ctx.mSwapChain.GetNumBackBuffers();
	const int BACK_BUFFER_INDEX = ctx.mSwapChain.GetCurrentBackBufferIndex();
	const int FRAME_DATA_INDEX = mNumRenderLoopsExecuted % NUM_BACK_BUFFERS;
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
	ID3D12Resource* pSwapChainRT = ctx.mSwapChain.GetCurrentBackBufferRenderTarget();
	CD3DX12_RESOURCE_BARRIER barriers[] =
	{
		  CD3DX12_RESOURCE_BARRIER::Transition(pSwapChainRT, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET)
		  //, CD3DX12_RESOURCE_BARRIER::Transition(
	};
	pCmd->ResourceBarrier(_countof(barriers), barriers);


	// Clear RT
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle = ctx.mSwapChain.GetCurrentBackBufferRTVHandle();
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


	// Set Viewport & Scissors
	const float RenderResolutionX = static_cast<float>(ctx.MainRTResolutionX);
	const float RenderResolutionY = static_cast<float>(ctx.MainRTResolutionY);
	D3D12_VIEWPORT viewport{ 0.0f, 0.0f, RenderResolutionX, RenderResolutionY, 0.0f, 1.0f };
	D3D12_RECT scissorsRect{ 0, 0, (LONG)RenderResolutionX, (LONG)RenderResolutionY };
	pCmd->RSSetViewports(1, &viewport);
	pCmd->RSSetScissorRects(1, &scissorsRect);

	// TODO: Draw Environment Map w/ HDR Swapchain
	// TBA

	// Draw Object
	const auto mesh = mBuiltinMeshes[EBuiltInMeshes::OBJ_FILE];

	pCmd->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	if (mesh)
	{
		auto VBIBIDs = mesh->GetIABufferIDs();
		uint32 NumIndices = mesh->GetNumIndices();
		uint32 NumInstances = 1;

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
		*((Consts*)pMem) = consts;

		pCmd->SetPipelineState(mRenderer.GetPSO(EBuiltinPSOs::HELLO_WORLD_FILE_PSO));

		// hardcoded roog signature for now until shader reflection and rootsignature management is implemented
		pCmd->SetGraphicsRootSignature(mRenderer.GetRootSignature(3));

		pCmd->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
		pCmd->SetGraphicsRootDescriptorTable(0, mRenderer.GetSRV(FrameData.SRVObjectTex).GetGPUDescHandle());
		pCmd->SetGraphicsRootConstantBufferView(1, cbAddr);


		pCmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCmd->IASetVertexBuffers(0, 1, &vb);
		pCmd->IASetIndexBuffer(&ib);

		pCmd->DrawIndexedInstanced(NumIndices, NumInstances, 0, 0, 0);

	}
	//Imgui
	ID3D12DescriptorHeap* heap = mRenderer.GetImguiHeap();
	pCmd->SetDescriptorHeaps(1, &heap);
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), pCmd);
	//

	// TODO: PostProcess Pass
	//TBA

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
	hr = ctx.mSwapChain.Present(ctx.bVsync);
	ctx.mSwapChain.MoveToNextFrame();

	mRenderer.ReleaseStallTexture(ctx.mSwapChain.GetCurrentBackBufferIndex());

	return hr;
}


