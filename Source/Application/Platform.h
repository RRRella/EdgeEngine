#pragma once

#include "Types.h"
#include <Windows.h>
#include <d3d12.h>

#include <mutex>
#include <condition_variable>
#include <vector>

#include "../Utils/Source/Log.h"

#include "Settings.h"

struct IDXGIAdapter1;
struct IDXGIOutput;

// -------------------------------------------------------------------------------

struct FStartupParameters
{
	HINSTANCE                 hExeInstance;
	int                       iCmdShow;
	Log::LogInitializeParams  LogInitParams;

	FEngineSettings EngineSettings;
	bool bOverrideGFXSetting_RenderScale;
	bool bOverrideGFXSetting_bVSync;
	bool bOverrideGFXSetting_bUseTripleBuffering;
	bool bOverrideGFXSetting_ResourceHeapDefaultSize;

	bool bOverrideENGSetting_MainWindowHeight;
	bool bOverrideENGSetting_MainWindowWidth;
	bool bOverrideENGSetting_bFullscreen;
	bool bOverrideENGSetting_PreferredDisplay;
};

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// -------------------------------------------------------------------------------
