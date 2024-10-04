#pragma once

#include <Windows.h>

//
// As this is a threaded application, we'll need to utilize an enum-based
// messaging system to handle events on different threads (say, render thread)
// than the threads recording the event (main thread). 
//
// Instead of dynamic casting the pointer and check-null, we'll use an enum
// in each event to distinguish events from one another.
//
enum EEventType
{
	// Windows window-related events
	WINDOW_RESIZE_EVENT = 0,
	TOGGLE_FULLSCREEN_EVENT,
	SET_FULLSCREEN_EVENT,

	NUM_EVENT_TYPES
};

struct IEvent
{
	IEvent(EEventType Type) : mType(Type) {}

	EEventType mType = EEventType::NUM_EVENT_TYPES;
};


struct WindowResizeEvent : public IEvent
{
	WindowResizeEvent() : IEvent(EEventType::WINDOW_RESIZE_EVENT) {}
	WindowResizeEvent(int w, int h, HWND hwnd_) : IEvent(EEventType::WINDOW_RESIZE_EVENT), width(w), height(h), hwnd(hwnd_) {}

	int width  = 0;
	int height = 0; 
	HWND hwnd = 0;
};

struct ToggleFullscreenEvent : public IEvent
{
	ToggleFullscreenEvent(HWND hwnd_) : IEvent(EEventType::TOGGLE_FULLSCREEN_EVENT), hwnd(hwnd_) {}

	HWND hwnd = 0;
};


struct SetFullscreenEvent : public IEvent
{
	SetFullscreenEvent() : IEvent(EEventType::SET_FULLSCREEN_EVENT) {}
	SetFullscreenEvent(bool bFullscreen) : IEvent(EEventType::SET_FULLSCREEN_EVENT), bToggleValue(bFullscreen) {}
	bool bToggleValue = false;
};