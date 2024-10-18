#include "Engine.h"

#include "../Utils/Source/utils.h"

using namespace DirectX;

void Engine::UpdateThread_Main()
{
	Log::Info("UpdateThread_Main()");
	UpdateThread_Inititalize();

	bool bQuit = false;
	float dt = 0.0f;
	while (!mbStopAllThreads && !bQuit)
	{
		UpdateThread_PreUpdate(dt);

#if DEBUG_LOG_THREAD_SYNC_VERBOSE
		Log::Info(/*"UpdateThread_Tick() : */"u%d (r=%llu)", mNumUpdateLoopsExecuted.load(), mNumRenderLoopsExecuted.load());
#endif

		UpdateThread_UpdateAppState(dt);

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

	mTimer.Reset();
	mTimer.Start();
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

void Engine::UpdateThread_PreUpdate(float& dt)
{
	// update timer
	dt = mTimer.Tick();

	// update input

}

void Engine::UpdateThread_UpdateAppState(const float& dt)
{
	assert(mbRenderThreadInitialized);

	if (mAppState == EAppState::INITIALIZING)
	{
		// start loading
		Log::Info("Main Thread starts loading...");

		// start load level
		Load_SceneData_Dispatch();
		mAppState = EAppState::LOADING;
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
		}

	}

	else
	{
		const int NUM_BACK_BUFFERS = mRenderer.GetSwapChainBackBufferCount(mpWinMain->GetHWND());
		const int FRAME_DATA_INDEX = mNumUpdateLoopsExecuted % NUM_BACK_BUFFERS;
		// update scene data
		mScene_MainWnd.mFrameData[FRAME_DATA_INDEX].TFCube.RotateAroundLocalYAxisDegrees(dt * 100);
	}

}

void Engine::UpdateThread_PostUpdate()
{
	if (mbLoadingLevel)
	{
		return;
	}
	// compute visibility 

	// extract scene view

	// copy over state for next frame

	const int NUM_BACK_BUFFERS = mRenderer.GetSwapChainBackBufferCount(mpWinMain->GetHWND());
	const int FRAME_DATA_INDEX = mNumUpdateLoopsExecuted % NUM_BACK_BUFFERS;
	const int FRAME_DATA_NEXT_INDEX = ((mNumUpdateLoopsExecuted % NUM_BACK_BUFFERS) + 1) % NUM_BACK_BUFFERS;
	mScene_MainWnd.mFrameData[FRAME_DATA_NEXT_INDEX] = mScene_MainWnd.mFrameData[FRAME_DATA_INDEX];
}

void Engine::Load_SceneData_Dispatch()
{
	mbLoadingLevel.store(true);
	mUpdateWorkerThreads.AddTask([&]()
		{
			const int NumBackBuffer_WndMain = mRenderer.GetSwapChainBackBufferCount(mpWinMain);
			const int currentBackBufferIDX = mRenderer.GetWindowSwapChain(mpWinMain->GetHWND()).GetCurrentBackBufferIndex();

			// TODO: initialize window scene data here for now
			FFrameData data;
			data.SwapChainClearColor = { 0.07f, 0.07f, 0.07f, 1.0f };

			// Cube Data
			constexpr XMFLOAT3 CUBE_POSITION = XMFLOAT3(0, 0, 4);
			constexpr float    CUBE_SCALE = 1.0f;
			constexpr XMFLOAT3 CUBE_ROTATION_VECTOR = XMFLOAT3(1, 1, 1);
			constexpr float    CUBE_ROTATION_DEGREES = 60.0f;
			const XMVECTOR     CUBE_ROTATION_AXIS = XMVector3Normalize(XMLoadFloat3(&CUBE_ROTATION_VECTOR));
			data.TFCube = Transform(
				CUBE_POSITION
				, XMQuaternionRotationAxis(CUBE_ROTATION_AXIS, CUBE_ROTATION_DEGREES * DEG2RAD)
				, XMFLOAT3(CUBE_SCALE, CUBE_SCALE, CUBE_SCALE)
			);
			CameraData camData = {};
			camData.nearPlane = 0.01f;
			camData.farPlane = 1000.0f;
			camData.x = 0.0f; camData.y = 3.0f; camData.z = -5.0f;
			camData.pitch = 10.0f;
			camData.yaw = 0.0f;
			camData.fovH_Degrees = 60.0f;
			data.SceneCamera.InitializeCamera(camData, mpWinMain->GetWidth(), mpWinMain->GetHeight());

			TextureID texID = mRenderer.CreateTextureFromFile("Data/Textures/1.png");
			SRV_ID    srvID = mRenderer.CreateAndInitializeSRV(texID);
			data.CubeTexture = srvID;

			mScene_MainWnd.mFrameData.resize(NumBackBuffer_WndMain, data);

			mWindowUpdateContextLookup[mpWinMain->GetHWND()] = &mScene_MainWnd;
			mbLoadingLevel.store(false);
		}
	);
}

void Engine::LoadSceneData()
{
	
}


void Engine::LoadLoadingScreenData()
{
	FLoadingScreenData data;

	data.SwapChainClearColor = { 0.0f, 0.2f, 0.4f, 1.0f };

	const std::string LoadingScreenTextureFilePath = "Data/Textures/0.png";
	TextureID texID = mRenderer.CreateTextureFromFile(LoadingScreenTextureFilePath.c_str());
	SRV_ID    srvID = mRenderer.CreateAndInitializeSRV(texID);
	data.SRVLoadingScreen = srvID;

	const int NumBackBuffer_WndMain = mRenderer.GetSwapChainBackBufferCount(mpWinMain);
	mScene_MainWnd.mLoadingScreenData.resize(NumBackBuffer_WndMain, data);
}

void MainWindowScene::Update()
{
}


