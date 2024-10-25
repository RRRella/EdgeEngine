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
	SRV_ID CubeTexture = INVALID_ID;
	std::array<float, 4> SwapChainClearColor;
};
struct FLoadingScreenData
{
	std::array<float, 4> SwapChainClearColor;
	// TODO: loading screen background img resource
	// TODO: animation resources
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
	void Update() override;
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
	void OnWindowCreate(IWindow* pWnd) override;
	void OnWindowResize(HWND hWnd) override;
	void OnToggleFullscreen(HWND hWnd) override;
	void OnWindowMinimize(IWindow* pWnd) override;
	void OnWindowFocus(IWindow* pWindow) override;
	void OnWindowClose(IWindow* pWindow) override;
	void OnWindowKeyDown(HWND hwnd, WPARAM wParam) override;
	void OnWindowKeyUp(HWND hwnd, WPARAM wParam) override;
	

	void MainThread_Tick();

	// ---------------------------------------------------------
	// Render Thread
	// ---------------------------------------------------------
	void RenderThread_Main();
	void RenderThread_Inititalize();
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
	void RenderThread_RenderMainWindow();

	// Processes the event queue populated by the Engine_Main.cpp thread
	void RenderThread_HandleEvents();

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
	void UpdateThread_UpdateAppState(const float& dt);
	void UpdateThread_UpdateScene(const float dt);

	// - Computes visibility per SceneView
	void UpdateThread_PostUpdate();

	void UpdateThread_HandleEvents();
//-----------------------------------------------------------------------

private:
	using BuiltinMeshArray_t     = std::array<Mesh      , EBuiltInMeshes::NUM_BUILTIN_MESHES>;
	using BuiltinMeshNameArray_t = std::array<std::string, EBuiltInMeshes::NUM_BUILTIN_MESHES>;
	using EventQueue_t = BufferedContainer<std::queue<std::unique_ptr<IEvent>>, std::unique_ptr<IEvent>>;
	using UpdateContextLookup_t = std::unordered_map<HWND, IWindowUpdateContext*>;

	// threads
	std::thread                mRenderThread;
	std::thread                mUpdateThread;
	ThreadPool                 mUpdateWorkerThreads;
	ThreadPool                 mRenderWorkerThreads;

	// sync
	std::atomic<bool>          mbStopAllThreads;
	std::unique_ptr<Semaphore> mpSemUpdate;
	std::unique_ptr<Semaphore> mpSemRender;
	
	// windows
	std::unique_ptr<Window>    mpWinMain;
	// todo: generic window mngmt

	// render
	Renderer                   mRenderer;
	BuiltinMeshArray_t         mBuiltinMeshes;
	BuiltinMeshNameArray_t     mBuiltinMeshNames;

	// data / state
	std::atomic<bool>          mbRenderThreadInitialized;
	std::atomic<uint64>        mNumRenderLoopsExecuted;
	std::atomic<uint64>        mNumUpdateLoopsExecuted;
	std::atomic<bool>          mbLoadingLevel;
	FEngineSettings            mSettings;
	EAppState                  mAppState;
	SystemInfo::FSystemInfo    mSysInfo;
	Timer mTimer;

	// scene
	MainWindowScene					mScene_MainWnd;
	UpdateContextLookup_t			mWindowUpdateContextLookup;
	RenderingResources_MainWindow   mResources_MainWnd;

	//input
	Input                          mInput;

	// events
	EventQueue_t	mWinEventQueue;
	EventQueue_t    mInputEventQueue;

	// Reads EngineSettings.ini from next to the executable and returns a 
	// FStartupParameters struct as it readily has override booleans for engine settings
	static FStartupParameters ParseEngineSettingsFile();

	void                     InititalizeEngineSettings(const FStartupParameters& Params);
	void                     InitializeApplicationWindows(const FStartupParameters& Params);

	void                     InitializeThreads();
	void                     ExitThreads();

	void                     InitializeBuiltinMeshes();
	void                     LoadLoadingScreenData(); // data is loaded in parallel but it blocks the calling thread until load is complete
	void                     Load_SceneData_Dispatch();
	void					 LoadSceneData();
	
	HRESULT                  RenderThread_RenderMainWindow_LoadingScreen(FWindowRenderContext& ctx);
	HRESULT                  RenderThread_RenderMainWindow_Scene(FWindowRenderContext& ctx);
	
	void                     RenderThread_HandleResizeWindowEvent(const IEvent* pEvent);
	void                     RenderThread_HandleToggleFullscreenEvent(const IEvent* pEvent);
	
	std::unique_ptr<Window>& GetWindow(HWND hwnd);
	const FWindowSettings&   GetWindowSettings(HWND hwnd) const;
	FWindowSettings&         GetWindowSettings(HWND hwnd);
};