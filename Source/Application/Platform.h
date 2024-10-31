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
	
	Log::LogInitializeParams  LogInitParams;

	FEngineSettings EngineSettings;

	uint8 bOverrideGFXSetting_RenderScale;
	uint8 bOverrideGFXSetting_bVSync;
	uint8 bOverrideGFXSetting_bUseTripleBuffering;

	uint8 bOverrideENGSetting_MainWindowHeight;
	uint8 bOverrideENGSetting_MainWindowWidth;
	uint8 bOverrideENGSetting_bFullscreen;
	uint8 bOverrideENGSetting_PreferredDisplay;
};

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// -------------------------------------------------------------------------------
