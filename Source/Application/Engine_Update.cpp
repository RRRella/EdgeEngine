#include "Engine.h"

#include "../Utils/Source/utils.h"

using namespace DirectX;

void Engine::UpdateThread_Main()
{
	Log::Info("UpdateThread_Main()");
	UpdateThread_Inititalize();

	bool bQuit = false;
	while (!mbStopAllThreads && !bQuit)
	{
		UpdateThread_PreUpdate();

#if DEBUG_LOG_THREAD_SYNC_VERBOSE
		Log::Info(/*"UpdateThread_Tick() : */"u%d (r=%llu)", mNumUpdateLoopsExecuted.load(), mNumRenderLoopsExecuted.load());
#endif

		UpdateThread_UpdateAppState();

		UpdateThread_PostUpdate();

		++mNumUpdateLoopsExecuted;

		UpdateThread_SignalRenderThread();

		UpdateThread_WaitForRenderThread();
	}

	UpdateThread_Exit();
	Log::Info("UpdateThread_Main() : Exit");
}

void Engine::UpdateThread_Inititalize()
{
	mNumUpdateLoopsExecuted.store(0);

	// busy lock until render thread is initialized
	while (!mbRenderThreadInitialized); 
	LoadLoadingScreenData();

	// Do not show windows until we have the loading screen data ready.
	mpWinMain->Show();
}

void Engine::UpdateThread_Exit()
{
}



void Engine::UpdateThread_WaitForRenderThread()
{
#if DEBUG_LOG_THREAD_SYNC_VERBOSE
	Log::Info("u:wait : u=%llu, r=%llu", mNumUpdateLoopsExecuted.load(), mNumRenderLoopsExecuted.load());
#endif

	mpSemUpdate->Wait();
}

void Engine::UpdateThread_SignalRenderThread()
{
	mpSemRender->Signal();
}

void Engine::UpdateThread_PreUpdate()
{
	// update timer

	// update input

}

void Engine::UpdateThread_UpdateAppState()
{
	assert(mbRenderThreadInitialized);

	if (mAppState == EAppState::INITIALIZING)
	{
		// start loading
		Log::Info("Main Thread starts loading...");

		// start load level
		Load_SceneData_Dispatch();
		mAppState = EAppState::LOADING;

		mbLoadingLevel.store(true);
	}

	if (mbLoadingLevel)
	{
		// animate loading screen


		// check if loading is done
		const int NumActiveTasks = mUpdateWorkerThreads.GetNumActiveTasks();
		const bool bLoadDone = NumActiveTasks == 0;
		if (bLoadDone)
		{
			Log::Info("Main Thread loaded");
			mAppState = EAppState::SIMULATING;
			mbLoadingLevel.store(false);
		}

	}


	else
	{
		// update scene data
	}

}

void Engine::UpdateThread_PostUpdate()
{
	// compute visibility 
}


void Engine::Load_SceneData_Dispatch()
{
	//mUpdateWorkerThreads.AddTask([&]() { Sleep(2000); Log::Info("Worker SLEEP done!"); }); // simulate 2second loading time
	mUpdateWorkerThreads.AddTask([&]()
	{
		const int NumBackBuffer_WndMain = mRenderer.GetSwapChainBackBufferCount(mpWinMain);

		// TODO: initialize window scene data here for now, should update this to proper location later on (Scene probably?)
		FFrameData data[2];
		data[0].SwapChainClearColor = { 0.07f, 0.07f, 0.07f, 1.0f };
		
		// Cube Data
		constexpr XMFLOAT3 CUBE_POSITION = XMFLOAT3(0, 0, 4);
		constexpr float    CUBE_SCALE = 1.0f;
		constexpr XMFLOAT3 CUBE_ROTATION_VECTOR = XMFLOAT3(1, 1, 1);
		constexpr float    CUBE_ROTATION_DEGREES = 60.0f;
		const XMVECTOR     CUBE_ROTATION_AXIS = XMVector3Normalize(XMLoadFloat3(&CUBE_ROTATION_VECTOR));
		data[0].TFCube = Transform(
			CUBE_POSITION
			, Quaternion::FromAxisAngle(CUBE_ROTATION_AXIS, CUBE_ROTATION_DEGREES * DEG2RAD)
			, XMFLOAT3(CUBE_SCALE, CUBE_SCALE, CUBE_SCALE)
		);
		CameraData camData = {};
		camData.nearPlane = 0.01f;
		camData.farPlane = 1000.0f;
		camData.x = 0.0f; camData.y = 3.0f; camData.z = -5.0f;
		camData.pitch = 10.0f;
		camData.yaw = 0.0f;
		camData.fovH_Degrees = 60.0f;
		data[0].SceneCamera.InitializeCamera(camData, mpWinMain->GetWidth(), mpWinMain->GetHeight());

		mScene_MainWnd.mFrameData.resize(NumBackBuffer_WndMain, data[0]);

		data[1].SwapChainClearColor = { 0.20f, 0.21f, 0.21f, 1.0f };

		mWindowUpdateContextLookup[mpWinMain->GetHWND()] = &mScene_MainWnd;
	});
}

void Engine::Load_SceneData_Join()
{
}


void Engine::LoadLoadingScreenData()
{
	FLoadingScreenData data;

	data.SwapChainClearColor = { 0.0f, 0.2f, 0.4f, 1.0f };

	srand(static_cast<unsigned>(time(NULL)));
	const std::string LoadingScreenTextureFilePath = "Data/Textures/LoadingScreen/0.png";
	TextureID texID = mRenderer.CreateTextureFromFile(LoadingScreenTextureFilePath.c_str());
	SRV_ID    srvID = mRenderer.CreateAndInitializeSRV(texID);
	data.SRVLoadingScreen = srvID;

	const int NumBackBuffer_WndMain = mRenderer.GetSwapChainBackBufferCount(mpWinMain);
	mScene_MainWnd.mLoadingScreenData.resize(NumBackBuffer_WndMain, data);
}


// ===============================================================================================================================


void MainWindowScene::Update()
{
}


