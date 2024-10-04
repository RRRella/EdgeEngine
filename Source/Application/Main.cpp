#include <ctime>
#include <cstdlib>
#include <Windows.h>
#include <vector>
#include <string>

#include "../Utils/Source/Log.h"
#include "../Utils/Source/utils.h"
#include "Window.h"

#include "Platform.h"
#include "Engine.h"


void ParseCommandLineParameters(FStartupParameters& refStartupParams, PSTR pScmdl)
{
	const std::string StrCmdLineParams = pScmdl;
	const std::vector<std::string> params = StrUtil::split(StrCmdLineParams, ' ');
	for (const std::string& param : params)
	{
		const std::vector<std::string> paramNameValue = StrUtil::split(param, '=');
		const std::string& paramName = paramNameValue.front();
		std::string  paramValue = paramNameValue.size() > 1 ? paramNameValue[1] : "";

		//
		// Log Settings
		//
		if (paramName == "-LogConsole")
		{
			refStartupParams.LogInitParams.bLogConsole = true;
		}
		if (paramName == "-LogFile")
		{
			refStartupParams.LogInitParams.bLogFile = true;
			refStartupParams.LogInitParams.LogFilePath = std::move(paramValue);
		}

		//
		// Engine Settings
		//
		if (paramName == "-Test")
		{
			refStartupParams.bOverrideENGSetting_bAutomatedTest = true;
			refStartupParams.EngineSettings.bAutomatedTestRun = true;
		}
		if (paramName == "-TestFrames")
		{
			refStartupParams.bOverrideENGSetting_bAutomatedTest = true;
			refStartupParams.EngineSettings.bAutomatedTestRun = true;

			refStartupParams.bOverrideENGSetting_bTestFrames  = true;
			if (paramValue.empty())
			{
				constexpr int NUM_DEFAULT_TEST_FRAMES = 100;
				Log::Warning("Empty -TestFrames value specified, using default: %d", NUM_DEFAULT_TEST_FRAMES);
				refStartupParams.EngineSettings.NumAutomatedTestFrames = NUM_DEFAULT_TEST_FRAMES;
			}
			else
			{
				refStartupParams.EngineSettings.NumAutomatedTestFrames = std::atoi(paramValue.c_str());
			}
		}
		if (paramName == "-Width" || paramName == "-W")
		{
			refStartupParams.bOverrideENGSetting_MainWindowWidth = true;
			refStartupParams.EngineSettings.WndMain.Width = std::atoi(paramValue.c_str());

		}
		if (paramName == "-Height" || paramName == "-H")
		{
			refStartupParams.bOverrideENGSetting_MainWindowHeight = true;
			refStartupParams.EngineSettings.WndMain.Height = std::atoi(paramValue.c_str());
		}
		if (paramName == "-DebugWindow" || paramName == "-dwnd")
		{
			//refStartupParams.
		}

		//
		// Graphics Settings
		//
		if (paramName == "-Fullscreen" || paramName == "-FullScreen" || paramName == "-fullscreen")
		{
			refStartupParams.bOverrideENGSetting_bFullscreen = true;
			refStartupParams.EngineSettings.WndMain.DisplayMode = EDisplayMode::EXCLUSIVE_FULLSCREEN;
		}
		if (paramName == "-VSync" || paramName == "-vsync" || paramName == "-Vsync")
		{
			refStartupParams.bOverrideGFXSetting_bVSync= true;
			refStartupParams.EngineSettings.gfx.bVsync= true;
		}
		if (paramName == "-TripleBuffering")
		{
			refStartupParams.bOverrideGFXSetting_bUseTripleBuffering = true;
			refStartupParams.EngineSettings.gfx.bUseTripleBuffering = true;
		}
		if (paramName == "-DoubleBuffering")
		{
			refStartupParams.bOverrideGFXSetting_bUseTripleBuffering = true;
			refStartupParams.EngineSettings.gfx.bUseTripleBuffering = false;
		}
	}
}


int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, PSTR pScmdl, int iCmdShow)
{
	FStartupParameters StartupParameters = {};
	StartupParameters.hExeInstance = hInst;
	
	srand(static_cast<unsigned>(time(NULL)));
	
	ParseCommandLineParameters(StartupParameters, pScmdl);

	Log::Initialize(StartupParameters.LogInitParams);

	{
		Engine Engine = {};
		Engine.Initialize(StartupParameters);

		MSG msg;
		bool bQuit = false;
		while (!bQuit)
		{
			while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);

				if (msg.message == WM_QUIT)
				{
					Log::Info("WM_QUIT!");
					bQuit = true;
					break;
				}
			}

			Engine.MainThread_Tick();
		}

		Engine.Exit();
	}
	Log::Destroy();

	//MessageBox(NULL, "EXIT", "Exit", MB_OK); // quick debugging

	return 0;
}
