#include "Engine.h"
#include "Math.h"
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
	RegisterWindowForInput(mpWinMain);

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
		UpdateThread_UpdateScene(dt);
	}

}

void Engine::UpdateThread_UpdateScene(const float dt)
{
	const int NUM_BACK_BUFFERS = mRenderer.GetSwapChainBackBufferCount(mpWinMain->GetHWND());
	const int FRAME_DATA_INDEX = mNumUpdateLoopsExecuted % NUM_BACK_BUFFERS;
	FFrameData& FrameData = mScene_MainWnd.mFrameData[FRAME_DATA_INDEX];
	const Input& input = mInputStates.at(mpWinMain->GetHWND());

	// handle input
	constexpr float CAMERA_MOVEMENT_SPEED_MULTIPLER = 0.75f;
	constexpr float CAMERA_MOVEMENT_SPEED_SHIFT_MULTIPLER = 2.0f;
	XMVECTOR LocalSpaceTranslation = XMVectorSet(0, 0, 0, 0);

	if (input.IsKeyDown(KeyCode::A))					LocalSpaceTranslation += XMLoadFloat3(&LeftVector);
	if (input.IsKeyDown(KeyCode::D))					LocalSpaceTranslation += XMLoadFloat3(&RightVector);
	if (input.IsKeyDown(KeyCode::W))					LocalSpaceTranslation += XMLoadFloat3(&ForwardVector);
	if (input.IsKeyDown(KeyCode::S))					LocalSpaceTranslation += XMLoadFloat3(&BackVector);
	if (input.IsKeyDown(KeyCode::E))					LocalSpaceTranslation += XMLoadFloat3(&UpVector);
	if (input.IsKeyDown(KeyCode::Q))					LocalSpaceTranslation += XMLoadFloat3(&DownVector);
	if (input.IsKeyDown(static_cast<KeyCode>
						(KeyCode::LeftShift
						& 
						KeyCode::RightShift)))	        LocalSpaceTranslation *= CAMERA_MOVEMENT_SPEED_SHIFT_MULTIPLER;

	LocalSpaceTranslation *= CAMERA_MOVEMENT_SPEED_MULTIPLER;
	constexpr float MOUSE_BUTTON_ROTATION_SPEED_MULTIPLIER = 1.0f;
	if (input.IsMouseDown(MouseCode::ButtonLeft))   FrameData.TFCube.RotateAroundAxisRadians(ZAxis, dt * PI * MOUSE_BUTTON_ROTATION_SPEED_MULTIPLIER);
	if (input.IsMouseDown(MouseCode::ButtonRight))  FrameData.TFCube.RotateAroundAxisRadians(YAxis, dt * PI * MOUSE_BUTTON_ROTATION_SPEED_MULTIPLIER);
	if (input.IsMouseDown(MouseCode::ButtonMiddle)) FrameData.TFCube.RotateAroundAxisRadians(XAxis, dt * PI * MOUSE_BUTTON_ROTATION_SPEED_MULTIPLIER);
	constexpr float DOUBLE_CLICK_MULTIPLIER = 4.0f;
	if (input.IsMouseDoubleClick(MouseCode::ButtonLeft))   FrameData.TFCube.RotateAroundAxisRadians(ZAxis, dt * PI * DOUBLE_CLICK_MULTIPLIER);
	if (input.IsMouseDoubleClick(MouseCode::ButtonRight))  FrameData.TFCube.RotateAroundAxisRadians(YAxis, dt * PI * DOUBLE_CLICK_MULTIPLIER);
	if (input.IsMouseDoubleClick(MouseCode::ButtonMiddle)) FrameData.TFCube.RotateAroundAxisRadians(XAxis, dt * PI * DOUBLE_CLICK_MULTIPLIER);

	constexpr float SCROLL_ROTATION_MULTIPLIER = 0.5f; // 90 degs | 0.5 rads
	if (input.IsMouseScrollUp()) FrameData.TFCube.RotateAroundAxisRadians(XAxis, PI * SCROLL_ROTATION_MULTIPLIER);
	if (input.IsMouseScrollDown()) FrameData.TFCube.RotateAroundAxisRadians(XAxis, -PI * SCROLL_ROTATION_MULTIPLIER);
   
	// update camera
	CameraInput camInput(LocalSpaceTranslation);
	camInput.DeltaMouseXY = input.GetMouseDelta();
	FrameData.SceneCamera.Update(dt, camInput);

	// update scene data
	FrameData.TFCube.RotateAroundAxisRadians(YAxis, dt * 0.2f * PI);
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

	// input post update;
	Input& i = mInputStates.at(mpWinMain->GetHWND()); // inline copies Input(), hence explicitly get refs
	i.PostUpdate();
}

void Engine::UpdateThread_HandleEvents()
{
	// Swap event recording buffers so we can read & process a limited number of events safely.
	mInputEventQueue.SwapBuffers();
	std::queue<EventPtr_t>& q = mInputEventQueue.GetBackContainer();
	if (q.empty())
		return;
	// process the events
	std::shared_ptr<IEvent> pEvent = nullptr;
	while (!q.empty())
	{
		pEvent = std::move(q.front());
		q.pop();
		switch (pEvent->mType)
		{
			case KEY_DOWN_EVENT:
			{
				KeyDownEvent* p = static_cast<KeyDownEvent*>(pEvent.get());
				mInputStates.at(p->hwnd).UpdateKeyDown(p->data);
			} break;
			case KEY_UP_EVENT:
			{
				KeyUpEvent* p = static_cast<KeyUpEvent*>(pEvent.get());
				mInputStates.at(p->hwnd).UpdateKeyUp(static_cast<KeyCode>(p->wparam));
			} break;
			case MOUSE_MOVE_EVENT:
			{
				std::shared_ptr<MouseMoveEvent> p = std::static_pointer_cast<MouseMoveEvent>(pEvent);
				mInputStates.at(p->hwnd).UpdateMousePos(p->x, p->y, 0);
			} break;
			case MOUSE_INPUT_EVENT:
			{
				std::shared_ptr<MouseInputEvent> p = std::static_pointer_cast<MouseInputEvent>(pEvent);
				mInputStates.at(p->hwnd).UpdateMousePos_Raw(
					p->data.relativeX
					, p->data.relativeY
					, static_cast<short>(p->data.scrollDelta)
					, GetWindow(p->hwnd)->IsMouseCaptured() || 1
				);
			} break;
			case MOUSE_SCROLL_EVENT:
			{
				std::shared_ptr<MouseScrollEvent> p = std::static_pointer_cast<MouseScrollEvent>(pEvent);
				mInputStates.at(p->hwnd).UpdateMousePos(0, 0, p->scroll);
			} break;

		}
	}
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


