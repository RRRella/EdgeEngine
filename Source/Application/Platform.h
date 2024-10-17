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

	uint8 bOverrideGFXSetting_RenderScale         : 1;
	uint8 bOverrideGFXSetting_bVSync              : 1;
	uint8 bOverrideGFXSetting_bUseTripleBuffering : 1;

	uint8 bOverrideENGSetting_MainWindowHeight    : 1;
	uint8 bOverrideENGSetting_MainWindowWidth     : 1;
	uint8 bOverrideENGSetting_bFullscreen         : 1;
	uint8 bOverrideENGSetting_PreferredDisplay    : 1;

	uint8 bOverrideENGSetting_bAutomatedTest      : 1;
	uint8 bOverrideENGSetting_bTestFrames         : 1;
};

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// -------------------------------------------------------------------------------
