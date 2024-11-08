#pragma once

#include "Types.h"
#include "Platform.h"
#include "Window.h"
#include "Settings.h"
#include "Events.h"
#include "Mesh.h"
#include "Transform.h"
#include "Camera.h"

#include "../Utils/Source/Multithreading.h"
#include "../Utils/Source/Timer.h"
#include "Source/Renderer/Renderer.h"
#include "Input.h"

#include <memory>

// Outputs Render/Update thread sync values on each Tick()
#define DEBUG_LOG_THREAD_SYNC_VERBOSE 0

struct FFrameData
{
	Camera SceneCamera;
	Transform TFCube;
	std::array<float, 4> SwapChainClearColor;
	SRV_ID SRVObjectTex = INVALID_ID;
};
struct FLoadingScreenData
{
	std::array<float, 4> SwapChainClearColor;
	SRV_ID SRVLoadingScreen = INVALID_ID;
};

class IWindowUpdateContext
{
public:
	virtual void Update() = 0;

	std::vector<FFrameData> mFrameData;
	std::vector<FLoadingScreenData> mLoadingScreenData;

protected:
	HWND hwnd;
};

class MainWindowScene : public IWindowUpdateContext
{
public:
	void Update() override {}
};

struct RenderingResources_MainWindow
{
	TextureID Tex_MainViewDepth = INVALID_ID;
	DSV_ID    DSV_MainViewDepth = INVALID_ID;
};

enum EAppState
{
	INITIALIZING = 0,
	LOADING,
	SIMULATING,
	UNLOADING,
	EXITING,
	NUM_APP_STATES
};


class Engine : public IWindowOwner
{
public:

public:

	// ---------------------------------------------------------
	// Main Thread
	// ---------------------------------------------------------
	bool Initialize(const FStartupParameters& Params);
	void Exit();

	// Window event callbacks for the main Window
	void OnWindowCreate(HWND hWnd) override;
	void OnWindowResize(HWND hWnd) override;
	void OnWindowMinimize(HWND hwnd) override;
	void OnWindowFocus(HWND hwnd) override;
	void OnWindowLoseFocus(HWND hwnd) override;
	void OnWindowClose(HWND hwnd) override;
	void OnToggleFullscreen(HWND hWnd) override;
	void OnWindowActivate(HWND hWnd) override;
	void OnWindowDeactivate(HWND hWnd) override;

	void OnKeyDown(HWND hwnd, WPARAM wParam) override;
	void OnKeyUp(HWND hwnd, WPARAM wParam) override;

	void OnMouseButtonDown(HWND hwnd, WPARAM wParam, bool bIsDoubleClick) override;
	void OnMouseButtonUp(HWND hwnd, WPARAM wParam) override;
	void OnMouseScroll(HWND hwnd, short scroll) override;
	void OnMouseMove(HWND hwnd, long x, long y) override;
	void OnMouseInput(HWND hwnd, LPARAM lParam) override;

	void MainThread_Tick();

	// ---------------------------------------------------------
	// Render Thread
	// ---------------------------------------------------------
	void RenderThread_Main();
	void RenderThread_Inititalize();
	void RenderThread_Initialize_Imgui();
	void RenderThread_Exit();
	void RenderThread_WaitForUpdateThread();
	void RenderThread_SignalUpdateThread();

	void RenderThread_LoadWindowSizeDependentResources(HWND hwnd, int Width, int Height);
	void RenderThread_UnloadWindowSizeDependentResources(HWND hwnd);

	// - TBA
	void RenderThread_PreRender();

	// RENDER()
	// - Records command lists in parallel per SceneView
	// - Submits commands to the GPU
	// - Presents SwapChain
	void RenderThread_Render();
	void RenderThread_DrawImguiWidgets();
	void RenderThread_RenderMainWindow();

	// ---------------------------------------------------------
	// Update Thread
	// ---------------------------------------------------------
	void UpdateThread_Main();
	void UpdateThread_Inititalize();
	void UpdateThread_Exit();
	void UpdateThread_WaitForRenderThread();
	void UpdateThread_SignalRenderThread();

	// - Updates timer
	// - Updates input state reading from Main Thread's input queue
	void UpdateThread_PreUpdate(float& dt);
	
	// - Updates program state (init/load/sim/unload/exit)
	// - Starts loading tasks
	// - Animates loading screen
	// - Updates scene data
	void UpdateThread_UpdateAppState(const float dt);
	void UpdateThread_UpdateScene(const float dt);

	// - Computes visibility per SceneView
	void UpdateThread_PostUpdate();
//-----------------------------------------------------------------------

	void                       SetWindowName(HWND hwnd, const std::string& name);
	void                       SetWindowName(const std::unique_ptr<Window>& pWin, const std::string& name);
	const std::string&		   GetWindowName(HWND hwnd) const;
	inline const std::string&  GetWindowName(const std::unique_ptr<Window>& pWin) const { return GetWindowName(pWin->GetHWND()); }
	inline const std::string&  GetWindowName(const Window* pWin) const { return GetWindowName(pWin->GetHWND()); }
private:
	//-------------------------------------------------------------------------------------------------
	using BuiltinMeshArray_t = std::array<Mesh, EBuiltInMeshes::NUM_BUILTIN_MESHES>;
	using BuiltinMeshNameArray_t = std::array<std::string, EBuiltInMeshes::NUM_BUILTIN_MESHES>;
	//-------------------------------------------------------------------------------------------------
	using EventPtr_t = std::shared_ptr<IEvent>;
	using EventQueue_t = BufferedContainer<std::queue<EventPtr_t>, EventPtr_t>;
	//-------------------------------------------------------------------------------------------------
	using UpdateContextLookup_t = std::unordered_map<HWND, IWindowUpdateContext*>;
	using RenderingResourcesLookup_t = std::unordered_map<HWND, std::shared_ptr<RenderingResources_MainWindow>>;
	using WindowLookup_t = std::unordered_map<HWND, std::unique_ptr<Window>>;
	using WindowNameLookup_t = std::unordered_map<HWND, std::string>;
	//-------------------------------------------------------------------------------------------------
	
	// threads
	std::thread                     mRenderThread;
	std::thread                     mUpdateThread;
	ThreadPool                      mUpdateWorkerThreads;
	ThreadPool                      mRenderWorkerThreads;

	// sync
	std::atomic<bool>               mbStopAllThreads;
	std::unique_ptr<Semaphore>      mpSemUpdate;
	std::unique_ptr<Semaphore>      mpSemRender;

	// windows
#if 0 // TODO
	WindowLookup_t                  mpWindows;
#else
	std::unique_ptr<Window>         mpWinMain;
#endif
	WindowNameLookup_t              mWinNameLookup;
	POINT                           mMouseCapturePosition;

	// render
	Renderer                        mRenderer;
	BuiltinMeshArray_t              mBuiltinMeshes;
	BuiltinMeshNameArray_t          mBuiltinMeshNames;

	// state
	std::atomic<bool>               mbRenderThreadInitialized;
	std::atomic<uint64>             mNumRenderLoopsExecuted;
	std::atomic<uint64>             mNumUpdateLoopsExecuted;
	std::atomic<bool>               mbLoadingLevel;
	EAppState                       mAppState;

	// system & settings
	FEngineSettings                 mSettings;
	SystemInfo::FSystemInfo         mSysInfo;

	// scene
	MainWindowScene                 mScene_MainWnd;
	UpdateContextLookup_t           mWindowUpdateContextLookup;

#if 0
	RenderingResourcesLookup_t      mRenderingResources;
#else
	RenderingResources_MainWindow  mResources_MainWnd;
#endif

	// input
	std::unordered_map<HWND, Input> mInputStates;

	// events 
	EventQueue_t                    mEventQueue_WinToEdgine_Renderer;
	EventQueue_t                    mEventQueue_WinToEdgine_Update;
	EventQueue_t                    mEventQueue_EdgineToWin_Main;

	// timer / profiler
	Timer                           mTimer;

	// misc.
	// One Swapchain.Resize() call is required for the first time 
	// transition of swapchains which are initialzied fullscreen.
	std::unordered_map<HWND, bool>  mInitialSwapchainResizeRequiredWindowLookup;


private:
	void                            InitializeEngineSettings(const FStartupParameters& Params);
	void                            InitializeWindows(const FStartupParameters& Params);

	void                            InitializeThreads();
	void                            ExitThreads();

	void                            HandleWindowTransitions(std::unique_ptr<Window>& pWin, const FWindowSettings& settings);
	void                            SetMouseCaptureForWindow(HWND hwnd, bool bCaptureMouse);
	inline void                     SetMouseCaptureForWindow(Window* pWin, bool bCaptureMouse) { this->SetMouseCaptureForWindow(pWin->GetHWND(), bCaptureMouse); };

	void                            InitializeBuiltinMeshes();
	void                            LoadLoadingScreenData(); // data is loaded in parallel but it blocks the calling thread until load is complete
	void                            Load_SceneData_Dispatch();
	void                            Load_SceneData_Join();

	HRESULT                         RenderThread_RenderMainWindow_LoadingScreen(FWindowRenderContext& ctx);
	HRESULT                         RenderThread_RenderMainWindow_Scene(FWindowRenderContext& ctx);

	void                            UpdateThread_HandleEvents();
	void                            RenderThread_HandleEvents();
	void                            MainThread_HandleEvents();

	void                            RenderThread_HandleWindowResizeEvent(const std::shared_ptr<IEvent>& pEvent);
	void                            RenderThread_HandleWindowCloseEvent(const IEvent* pEvent);
	void                            RenderThread_HandleToggleFullscreenEvent(const IEvent* pEvent);

	void                            UpdateThread_HandleWindowResizeEvent(const std::shared_ptr<IEvent>& pEvent);

	std::unique_ptr<Window>&		GetWindow(HWND hwnd);
	const FWindowSettings&			GetWindowSettings(HWND hwnd) const;
	FWindowSettings&				GetWindowSettings(HWND hwnd);
	FFrameData&						GetCurrentFrameData(HWND hwnd);

	void                            RegisterWindowForInput(const std::unique_ptr<Window>& pWnd);
	void                            UnregisterWindowForInput(const std::unique_ptr<Window>& pWnd);

	void                            HandleEngineInput();

	bool                            IsWindowRegistered(HWND hwnd) const;


	// Reads EngineSettings.ini from next to the executable and returns a 
	// FStartupParameters struct as it readily has override booleans for engine settings
	static FStartupParameters       ParseEngineSettingsFile();
};
