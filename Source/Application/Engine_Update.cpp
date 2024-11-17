#define NOMINMAX

#include "Engine.h"
#include "Math.h"

#include "../Utils/Source/utils.h"

#include <algorithm>

using namespace DirectX;

// temporary hardcoded initialization until scene is data driven
static CameraData GenerateCameraInitializationParameters(const std::unique_ptr<Window>& pWin)
{
	assert(pWin);
	CameraData camData = {};
	camData.x = 0.0f; camData.y = 3.0f; camData.z = -5.0f;
	camData.pitch = 10.0f;
	camData.yaw = 0.0f;
	camData.bPerspectiveProjection = true;
	camData.fovV_Degrees = 60.0f;
	camData.nearPlane = 0.01f;
	camData.farPlane = 1000.0f;
	camData.width  = static_cast<float>(pWin->GetWidth() );
	camData.height = static_cast<float>(pWin->GetHeight());
	return camData;
}

void Engine::UpdateThread_Main()
{
	Log::Info("UpdateThread_Main()");

	UpdateThread_Inititalize();

	bool bQuit = false;
	float dt = 0.0f;
	while (!mbStopAllThreads && !bQuit)
	{
		UpdateThread_HandleEvents();

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

#if ENABLE_RAW_INPUT
	// initialize raw input
	Input::InitRawInputDevices(mpWinMain->GetHWND());
#endif

	// initialize input states
	RegisterWindowForInput(mpWinMain);

	// busy lock until render thread is initialized
	while (!mbRenderThreadInitialized); 

	// immediately load loading screen texture
	LoadLoadingScreenData();

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

	if (mbStopAllThreads)
		return;

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

	// system-wide input (esc/mouse click on wnd)
	HandleEngineInput();
}

void Engine::HandleEngineInput()
{
	for (decltype(mInputStates)::iterator it = mInputStates.begin(); it != mInputStates.end(); ++it)
	{
		HWND   hwnd  = it->first;
		Input& input = it->second;
		auto&  pWin  = this->GetWindow(hwnd);

		if (input.IsKeyDown(KeyCode::Escape))
		{
			if (pWin->IsMouseCaptured())
			{
				mEventQueue_EdgineToWin_Main.AddItem(std::make_shared<SetMouseCaptureEvent>(hwnd, false, true));
			}
		}
		if (input.IsAnyMouseDown())
		{
			Input& inp = mInputStates.at(hwnd); // non const ref
			if (inp.GetInputBypassing())
			{
				inp.SetInputBypassing(false);

				// capture mouse only when main window is clicked
				if(hwnd == mpWinMain->GetHWND())
					mEventQueue_EdgineToWin_Main.AddItem(std::make_shared<SetMouseCaptureEvent>(hwnd, true, false));
			}
		}
	}
}

bool Engine::IsWindowRegistered(HWND hwnd) const
{
	auto it = mWinNameLookup.find(hwnd);
	return it != mWinNameLookup.end();
}

void Engine::SetWindowName(HWND hwnd, const std::string& name){	mWinNameLookup[hwnd] = name; }
void Engine::SetWindowName(const std::unique_ptr<Window>& pWin, const std::string& name) { SetWindowName(pWin->GetHWND(), name); }

const std::string& Engine::GetWindowName(HWND hwnd) const
{
#if _DEBUG
	auto it = mWinNameLookup.find(hwnd);
	if (it == mWinNameLookup.end())
	{
		Log::Error("Couldn't find window<%x> name: HWND not called with SetWindowName()!", hwnd);
		assert(false); // gonna crash at .at() call anyways.
	}
#endif
	return mWinNameLookup.at(hwnd);
}

void Engine::UpdateThread_UpdateAppState(const float dt)
{
	assert(mbRenderThreadInitialized);


	if (mAppState == EAppState::INITIALIZING)
	{
		// start loading
		Log::Info("Main Thread starts loading...");

		// start load level
		Load_SceneData_Dispatch();
		mAppState = EAppState::LOADING;// not thread-safe

		mbLoadingLevel.store(true);    // thread-safe
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
		// TODO: threaded?
		UpdateThread_UpdateScene(dt);
	}

}



FFrameData& Engine::GetCurrentFrameData(HWND hwnd)
{
	const int NUM_BACK_BUFFERS = mRenderer.GetSwapChainBackBufferCount(hwnd);
	const int FRAME_DATA_INDEX = mNumUpdateLoopsExecuted % NUM_BACK_BUFFERS;
	return mWindowUpdateContextLookup.at(hwnd)->mFrameData[FRAME_DATA_INDEX];

}

void Engine::UpdateThread_UpdateScene(const float dt)
{
	std::unique_ptr<Window>& pWin = mpWinMain;
	HWND hwnd                     = pWin->GetHWND();
	FFrameData& FrameData         = GetCurrentFrameData(hwnd);
	const Input& input            = mInputStates.at(hwnd);
	
	// handle input
	if(!FrameData.SceneCamera.mIsInitialized)
		FrameData.SceneCamera.InitializeCamera(GenerateCameraInitializationParameters(mpWinMain));

	constexpr float CAMERA_MOVEMENT_SPEED_MULTIPLER = 0.75f;
	XMVECTOR LocalSpaceTranslation = XMVectorSet(0, 0, 0, 0);

	if (input.IsKeyDown(KeyCode::LeftAlt))
	{
		if (input.IsMouseDown(MOUSE_BUTTON_RIGHT))
		{
			LocalSpaceTranslation += XMVECTOR{-1.0f,0.0f,0.0f} * input.GetMouseDelta()[0] * 0.1f;
			LocalSpaceTranslation += XMVECTOR{ 0.0f,1.0f,0.0f} * input.GetMouseDelta()[1] * 0.1f;

			LocalSpaceTranslation *= CAMERA_MOVEMENT_SPEED_MULTIPLER;
		}
		if (input.IsMouseDown(MOUSE_BUTTON_LEFT))
		{
			FrameData.SceneCamera.Rotate(input.GetMouseDelta()[0], input.GetMouseDelta()[1], dt);
		}
		if (input.IsMouseScrollDown() || input.IsMouseScrollUp())
		{
			LocalSpaceTranslation += XMVECTOR{ 0.0f,0.0f,1.0f } * input.GetMouseScroll() * 0.5f;
		}
	}
	
	// update camera
	CameraInput camInput(LocalSpaceTranslation);
	camInput.DeltaMouseXY = input.GetMouseDelta();

	FrameData.SceneCamera.Update(dt, camInput);
}

void Engine::UpdateThread_PostUpdate()
{
	const int NUM_BACK_BUFFERS      = mRenderer.GetSwapChainBackBufferCount(mpWinMain->GetHWND());
	const int FRAME_DATA_INDEX      = mNumUpdateLoopsExecuted % NUM_BACK_BUFFERS;
	const int FRAME_DATA_NEXT_INDEX = ((mNumUpdateLoopsExecuted % NUM_BACK_BUFFERS) + 1) % NUM_BACK_BUFFERS;

	if (mbLoadingLevel)
	{
		return;
	}

	// compute visibility 

	// extract scene view

	// copy over state for next frame
	mScene_MainWnd.mFrameData[FRAME_DATA_NEXT_INDEX] = mScene_MainWnd.mFrameData[FRAME_DATA_INDEX];

	// input post update
	for (auto it = mInputStates.begin(); it != mInputStates.end(); ++it)
	{
		const HWND& hwnd = it->first;
		mInputStates.at(hwnd).PostUpdate(); // non-const accessor
	}
}

void Engine::Load_SceneData_Dispatch()
{
	mUpdateWorkerThreads.AddTask([&]() { Sleep(1000); Log::Info("Worker SLEEP done!"); }); // simulate 1second loading time
	mUpdateWorkerThreads.AddTask([&]()
	{
		const int NumBackBuffer_WndMain = mRenderer.GetSwapChainBackBufferCount(mpWinMain);

		// TODO: initialize window scene data here for now, should update this to proper location later on (Scene probably?)
		FFrameData data;
		data.SwapChainClearColor = { 0.4f, 0.4f, 0.5f, 1.0f };

		// Cube Data
		constexpr XMFLOAT3 CUBE_POSITION         = XMFLOAT3(0, -15, 40);
		constexpr float    CUBE_SCALE            = 1.0f;

		constexpr XMFLOAT3 CUBE_ROTATION_VECTOR  = XMFLOAT3(-1, 0, 0);
		constexpr float    CUBE_ROTATION_DEGREES = 90.0f;
		const XMVECTOR     CUBE_ROTATION_AXIS    = XMVector3Normalize(XMLoadFloat3(&CUBE_ROTATION_VECTOR));
		XMVECTOR q = XMQuaternionRotationAxis(CUBE_ROTATION_AXIS, CUBE_ROTATION_DEGREES * DEG2RAD);

		constexpr XMFLOAT3 CUBE_ROTATION_VECTOR1 = XMFLOAT3(0, -1, 0);
		constexpr float    CUBE_ROTATION_DEGREES1 = 180.0f;
		const XMVECTOR     CUBE_ROTATION_AXIS1 = XMVector3Normalize(XMLoadFloat3(&CUBE_ROTATION_VECTOR1));

		XMVECTOR q1 = XMQuaternionRotationAxis(CUBE_ROTATION_AXIS1, CUBE_ROTATION_DEGREES1 * DEG2RAD);

		data.TFCube = Transform(
			  CUBE_POSITION
			, XMQuaternionMultiply(q , q1)
			, XMFLOAT3(CUBE_SCALE, CUBE_SCALE, CUBE_SCALE)
		);

		CameraData camData = GenerateCameraInitializationParameters(mpWinMain);
		data.SceneCamera.InitializeCamera(camData);

		const std::string TextureFilePath = "Data/Textures/Skull.jpg";
		TextureID texID = mRenderer.CreateTextureFromFile(TextureFilePath.c_str());
		SRV_ID    srvID = mRenderer.CreateAndInitializeSRV(texID);
		data.SRVObjectTex = srvID;

		mScene_MainWnd.mFrameData.resize(NumBackBuffer_WndMain, data);

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

	const std::string LoadingScreenTextureFilePath = "Data/Textures/0.png";
	TextureID texID = mRenderer.CreateTextureFromFile(LoadingScreenTextureFilePath.c_str());
	SRV_ID    srvID = mRenderer.CreateAndInitializeSRV(texID);
	data.SRVLoadingScreen = srvID;

	const int NumBackBuffer_WndMain = mRenderer.GetSwapChainBackBufferCount(mpWinMain);
	mScene_MainWnd.mLoadingScreenData.resize(NumBackBuffer_WndMain, data);
}

