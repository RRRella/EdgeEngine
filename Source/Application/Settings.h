#pragma once

enum EDisplayMode
{
	WINDOWED = 0,
	BORDERLESS_FULLSCREEN,
	EXCLUSIVE_FULLSCREEN,

	NUM_DISPLAY_MODES
};

struct FGraphicsSettings
{
	bool bVsync              = false;
	bool bUseTripleBuffering = false;

	float RenderScale = 1.0f;

	uint64_t HeapDefaultSize = 1024 * 1024 * 300;
};

struct FWindowSettings
{
	int Width                 = -1;
	int Height                = -1;
	EDisplayMode DisplayMode  = EDisplayMode::WINDOWED;
	unsigned PreferredDisplay = 0;
	char Title[64]           = "";

	inline bool IsDisplayModeFullscreen() const { return DisplayMode == EDisplayMode::EXCLUSIVE_FULLSCREEN || DisplayMode == EDisplayMode::BORDERLESS_FULLSCREEN; }
};

struct FEngineSettings
{
	FGraphicsSettings gfx;

	FWindowSettings WndMain;
};