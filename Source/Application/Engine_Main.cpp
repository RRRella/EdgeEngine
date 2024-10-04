#include "Engine.h"

#include "../Utils/Source/utils.h"

#include <fstream>
#include <sstream>
#include <cassert>

#ifdef _DEBUG
constexpr char* BUILD_CONFIG = "Debug";
#else
constexpr char* BUILD_CONFIG = "Release";
#endif
constexpr char* ENGINE_VERSION = "v0.1.0";


void Engine::MainThread_Tick()
{
	if (this->mSettings.bAutomatedTestRun)
	{
		if (this->mSettings.NumAutomatedTestFrames <= mNumRenderLoopsExecuted)
		{
			PostQuitMessage(0);
		}
	}

	// TODO: populate input queue and signal Update thread 
	//       to drain the buffered input from the queue
}

bool Engine::Initialize(const FStartupParameters& Params)
{
	// -------------------------------------------------------------------------
	// TODO: When this function is called, EnumerateAdapters function misbehaves
	//       by not populating the std::vector after push_back(). Looks like 
	//       a module/linking/project dependency issue. Commenting out this line
	//       and then recompiling fixes the issue, even though the same program
	//       when compiled initially will not function correctly.
	#if 0
	this->mSysInfo = SystemInfo::GetSystemInfo();
	#endif
	// -------------------------------------------------------------------------
	InititalizeEngineSettings(Params);
	InitializeApplicationWindows(Params);
	InitializeThreads();

	return true;
}

void Engine::Exit()
{
	ExitThreads();

	
}



void Engine::InititalizeEngineSettings(const FStartupParameters& Params)
{
	const FEngineSettings& p = Params.EngineSettings;

	// Defaults
	FEngineSettings& s = mSettings;
	s.gfx.bVsync = false;
	s.gfx.bUseTripleBuffering = true;
	s.gfx.RenderScale = 1.0f;
	
	s.WndMain.Width = 1920;
	s.WndMain.Height = 1080;
	s.WndMain.DisplayMode = EDisplayMode::WINDOWED;
	s.WndMain.PreferredDisplay = 0;
	sprintf_s(s.WndMain.Title , "Engine %s-%s", ENGINE_VERSION, BUILD_CONFIG);

	s.WndDebug.Width  = 600;
	s.WndDebug.Height = 600;
	s.WndDebug.DisplayMode = EDisplayMode::WINDOWED;
	s.WndDebug.PreferredDisplay = 1;
	sprintf_s(s.WndDebug.Title, "Debugging");

	s.bAutomatedTestRun = false;
	s.NumAutomatedTestFrames = 100; // default num frames to run if -Test is specified in cmd line params


	// Override #0 : from file
	FStartupParameters paramFile = Engine::ParseEngineSettingsFile();
	const FEngineSettings& pf = paramFile.EngineSettings;
	if (paramFile.bOverrideGFXSetting_bVSync     )                 s.gfx.bVsync              = pf.gfx.bVsync;
	if (paramFile.bOverrideGFXSetting_bUseTripleBuffering)         s.gfx.bUseTripleBuffering = pf.gfx.bUseTripleBuffering;
	if (paramFile.bOverrideGFXSetting_RenderScale)                 s.gfx.RenderScale         = pf.gfx.RenderScale;

	if (paramFile.bOverrideENGSetting_MainWindowWidth)             s.WndMain.Width            = pf.WndMain.Width;
	if (paramFile.bOverrideENGSetting_MainWindowHeight)            s.WndMain.Height           = pf.WndMain.Height;
	if (paramFile.bOverrideENGSetting_bFullscreen)                 s.WndMain.DisplayMode      = pf.WndMain.DisplayMode;
	if (paramFile.bOverrideENGSetting_PreferredDisplay)            s.WndMain.PreferredDisplay = pf.WndMain.PreferredDisplay;

	if (paramFile.bOverrideENGSetting_bDebugWindowEnable)          s.bShowDebugWindow          = pf.bShowDebugWindow;
	if (paramFile.bOverrideENGSetting_DebugWindowWidth)            s.WndDebug.Width            = pf.WndDebug.Width;
	if (paramFile.bOverrideENGSetting_DebugWindowHeight)           s.WndDebug.Height           = pf.WndDebug.Height;
	if (paramFile.bOverrideENGSetting_DebugWindowbFullscreen)      s.WndDebug.DisplayMode      = pf.WndDebug.DisplayMode;
	if (paramFile.bOverrideENGSetting_DebugWindowPreferredDisplay) s.WndDebug.PreferredDisplay = pf.WndDebug.PreferredDisplay;

	if (paramFile.bOverrideENGSetting_bAutomatedTest)              s.bAutomatedTestRun = pf.bAutomatedTestRun;
	if (paramFile.bOverrideENGSetting_bTestFrames)
	{ 
		s.bAutomatedTestRun = true; 
		s.NumAutomatedTestFrames = pf.NumAutomatedTestFrames; 
	}


	// Override #1 : if there's command line params
	if (Params.bOverrideGFXSetting_bVSync     )                 s.gfx.bVsync              = p.gfx.bVsync;
	if (Params.bOverrideGFXSetting_bUseTripleBuffering)         s.gfx.bUseTripleBuffering = p.gfx.bUseTripleBuffering;
	if (Params.bOverrideGFXSetting_RenderScale)                 s.gfx.RenderScale         = p.gfx.RenderScale;

	if (Params.bOverrideENGSetting_MainWindowWidth)             s.WndMain.Width            = p.WndMain.Width;
	if (Params.bOverrideENGSetting_MainWindowHeight)            s.WndMain.Height           = p.WndMain.Height;
	if (Params.bOverrideENGSetting_bFullscreen)                 s.WndMain.DisplayMode      = p.WndMain.DisplayMode;
	if (Params.bOverrideENGSetting_PreferredDisplay)            s.WndMain.PreferredDisplay = p.WndMain.PreferredDisplay;
	
	if (Params.bOverrideENGSetting_bDebugWindowEnable)          s.bShowDebugWindow          = p.bShowDebugWindow;
	if (Params.bOverrideENGSetting_DebugWindowWidth)            s.WndDebug.Width            = p.WndDebug.Width;
	if (Params.bOverrideENGSetting_DebugWindowHeight)           s.WndDebug.Height           = p.WndDebug.Height;
	if (Params.bOverrideENGSetting_DebugWindowbFullscreen)      s.WndDebug.DisplayMode      = p.WndDebug.DisplayMode;
	if (Params.bOverrideENGSetting_DebugWindowPreferredDisplay) s.WndDebug.PreferredDisplay = p.WndDebug.PreferredDisplay;

	if (Params.bOverrideENGSetting_bAutomatedTest)     s.bAutomatedTestRun    = p.bAutomatedTestRun;
	if (Params.bOverrideENGSetting_bTestFrames)
	{
		s.bAutomatedTestRun = true;
		s.NumAutomatedTestFrames = Params.EngineSettings.NumAutomatedTestFrames;
	}
}

void Engine::InitializeApplicationWindows(const FStartupParameters& Params)
{
	auto fnInitializeWindows = [&](const FWindowSettings& settings, HINSTANCE hInstance, std::unique_ptr<Window>& pWin)
	{
		FWindowDesc desc = {};
		desc.width = settings.Width;
		desc.height= settings.Height;
		desc.hInst = hInstance;
		desc.pWndOwner  = this;
		desc.pfnWndProc = WndProc;
		desc.bFullscreen = settings.DisplayMode == EDisplayMode::EXCLUSIVE_FULLSCREEN;
		desc.preferredDisplay = settings.PreferredDisplay;
		pWin.reset(new Window(settings.Title, desc));
	};

	fnInitializeWindows(mSettings.WndMain, Params.hExeInstance, mpWinMain);
	Log::Info("Created main window<0x%x>: %dx%d", mpWinMain->GetHWND(), mpWinMain->GetWidth(), mpWinMain->GetHeight());

}

void Engine::InitializeThreads()
{
	const int NUM_SWAPCHAIN_BACKBUFFERS = mSettings.gfx.bUseTripleBuffering ? 3 : 2;
	mpSemUpdate.reset(new Semaphore(NUM_SWAPCHAIN_BACKBUFFERS, NUM_SWAPCHAIN_BACKBUFFERS));
	mpSemRender.reset(new Semaphore(0                        , NUM_SWAPCHAIN_BACKBUFFERS));

	mbStopAllThreads.store(false);
	mRenderThread = std::thread(&Engine::RenderThread_Main, this);
	mUpdateThread = std::thread(&Engine::UpdateThread_Main, this);

	const size_t HWThreads = ThreadPool::sHardwareThreadCount;
	const size_t HWCores   = HWThreads/2;
	const size_t NumWorkers = HWCores - 2; // reserve 2 cores for (Update + Render) + Main threads
	mUpdateWorkerThreads.Initialize(NumWorkers, mUpdateWorkerThreads.GetThreadPoolName());
	mRenderWorkerThreads.Initialize(NumWorkers, mRenderWorkerThreads.GetThreadPoolName());
}

void Engine::ExitThreads()
{
	mbStopAllThreads.store(true);
	mRenderThread.join();
	mUpdateThread.join();

	mUpdateWorkerThreads.Destroy();
	mRenderWorkerThreads.Destroy();
}



std::unique_ptr<Window>& Engine::GetWindow(HWND hwnd)
{
	if (mpWinMain->GetHWND() == hwnd)
		return mpWinMain;

	// TODO: handle other windows here when they're implemented

	Log::Warning("Engine::GetWindow() : Invalid hwnd=0x%x, returning Main Window", hwnd);
	return mpWinMain;
}

const FWindowSettings& Engine::GetWindowSettings(HWND hwnd) const
{
	if (mpWinMain->GetHWND() == hwnd)
		return mSettings.WndMain;
	
	// TODO: handle other windows here when they're implemented

	Log::Warning("Engine::GetWindowSettings() : Invalid hwnd=0x%x, returning Main Window Settings", hwnd);
	return mSettings.WndMain;
}
FWindowSettings& Engine::GetWindowSettings(HWND hwnd)
{
	if (mpWinMain->GetHWND() == hwnd)
		return mSettings.WndMain;
	
	// TODO: handle other windows here when they're implemented

	Log::Warning("Engine::GetWindowSettings() : Invalid hwnd=0x%x, returning Main Window Settings", hwnd);
	return mSettings.WndMain;
}


static std::pair<std::string, std::string> ParseLineINI(const std::string& iniLine)
{
	assert(!iniLine.empty());
	std::pair<std::string, std::string> SettingNameValuePair;

	const bool bSectionTag = iniLine[0] == '[';

	if (bSectionTag)
	{
		auto vecSettingNameValue = StrUtil::split(iniLine.substr(1), ']');
		SettingNameValuePair.first = vecSettingNameValue[0];
	}
	else
	{
		auto vecSettingNameValue = StrUtil::split(iniLine, '=');
		assert(vecSettingNameValue.size() >= 2);
		SettingNameValuePair.first  = vecSettingNameValue[0];
		SettingNameValuePair.second = vecSettingNameValue[1];
	}


	return SettingNameValuePair;
}
static bool ParseBool(const std::string& s) { bool b; std::istringstream(s) >> std::boolalpha >> b; return b; }
static int  ParseInt(const std::string& s) { return std::atoi(s.c_str()); }
static float ParseFloat(const std::string& s) { return static_cast<float>(std::atof(s.c_str())); }

static std::unordered_map<std::string, EDisplayMode> S_LOOKUP_STR_TO_DISPLAYMODE =
{
	  { "Fullscreen"           , EDisplayMode::EXCLUSIVE_FULLSCREEN   }
	, { "Borderless"           , EDisplayMode::BORDERLESS_FULLSCREEN  }
	, { "BorderlessFullscreen" , EDisplayMode::BORDERLESS_FULLSCREEN  }
	, { "BorderlessWindowed"   , EDisplayMode::BORDERLESS_FULLSCREEN  }
	, { "Windowed"             , EDisplayMode::WINDOWED               }
};

FStartupParameters Engine::ParseEngineSettingsFile()
{
	constexpr char* ENGINE_SETTINGS_FILE_NAME = "Data/EngineSettings.ini";
	FStartupParameters params = {};

	std::ifstream file(ENGINE_SETTINGS_FILE_NAME);
	if (file.is_open())
	{
		std::string line;
		std::string currSection;
		while (std::getline(file, line))
		{
			if (line[0] == ';') continue; // skip comment lines
			if (line == "") continue; // skip empty lines

			const std::pair<std::string, std::string> SettingNameValuePair = ParseLineINI(line);
			const std::string& SettingName  = SettingNameValuePair.first;
			const std::string& SettingValue = SettingNameValuePair.second;

			// Header sections;
			if (SettingName == "Graphics" || SettingName == "Engine")
			{
				currSection = SettingName;
				continue;
			}


			// 
			// Graphics
			//
			if (SettingName == "VSync")
			{
				params.bOverrideGFXSetting_bVSync = true;
				params.EngineSettings.gfx.bVsync = ParseBool(SettingValue);
			}
			if (SettingName == "RenderScale")
			{
				params.bOverrideGFXSetting_RenderScale = true;
				params.EngineSettings.gfx.RenderScale = ParseFloat(SettingValue);
			}
			if (SettingName == "TripleBuffer")
			{
				params.bOverrideGFXSetting_bUseTripleBuffering = true;
				params.EngineSettings.gfx.bUseTripleBuffering = ParseBool(SettingValue);
			}


			// 
			// Engine
			//
			if (SettingName == "Width")
			{
				params.bOverrideENGSetting_MainWindowWidth = true;
				params.EngineSettings.WndMain.Width = ParseInt(SettingValue);
			}
			if (SettingName == "Height")
			{
				params.bOverrideENGSetting_MainWindowHeight = true;
				params.EngineSettings.WndMain.Height = ParseInt(SettingValue);
			}
			if (SettingName == "DisplayMode")
			{
				if (S_LOOKUP_STR_TO_DISPLAYMODE.find(SettingValue) != S_LOOKUP_STR_TO_DISPLAYMODE.end())
				{
					params.bOverrideENGSetting_bFullscreen = true;
					params.EngineSettings.WndMain.DisplayMode = S_LOOKUP_STR_TO_DISPLAYMODE.at(SettingValue);
				}
			}
			if (SettingName == "PreferredDisplay")
			{
				params.bOverrideENGSetting_PreferredDisplay = true;
				params.EngineSettings.WndMain.PreferredDisplay = ParseInt(SettingValue);
			}


			if (SettingName == "DebugWindow")
			{
				params.bOverrideENGSetting_bDebugWindowEnable = true;
				params.EngineSettings.bShowDebugWindow = ParseBool(SettingValue);
			}
			if (SettingName == "DebugWindowWidth")
			{
				params.bOverrideENGSetting_DebugWindowWidth = true;
				params.EngineSettings.WndDebug.Width = ParseInt(SettingValue);
			}
			if (SettingName == "DebugWindowHeight")
			{
				params.bOverrideENGSetting_DebugWindowHeight = true;
				params.EngineSettings.WndDebug.Height = ParseInt(SettingValue);
			}
			if (SettingName == "DebugWindowDisplayMode")
			{
				if (S_LOOKUP_STR_TO_DISPLAYMODE.find(SettingValue) != S_LOOKUP_STR_TO_DISPLAYMODE.end())
				{
					params.bOverrideENGSetting_DebugWindowbFullscreen = true;
					params.EngineSettings.WndDebug.DisplayMode = S_LOOKUP_STR_TO_DISPLAYMODE.at(SettingValue);
				}
			}
			if (SettingName == "DebugWindowPreferredDisplay")
			{
				params.bOverrideENGSetting_DebugWindowPreferredDisplay = true;
				params.EngineSettings.WndDebug.PreferredDisplay = ParseInt(SettingValue);
			}
			
		}
	}
	else
	{
		Log::Warning("Cannot find settings file %s in current directory: %s", ENGINE_SETTINGS_FILE_NAME, DirectoryUtil::GetCurrentPath().c_str());
		Log::Warning("Will use default settings for Engine & Graphics.", ENGINE_SETTINGS_FILE_NAME, DirectoryUtil::GetCurrentPath().c_str());
	}

	return params;
}
