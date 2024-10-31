#pragma once

#include <Windows.h>
#include "../Utils/Source/Multithreading.h"
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
	WINDOW_CLOSE_EVENT,
	TOGGLE_FULLSCREEN_EVENT,
	SET_FULLSCREEN_EVENT,
	KEY_DOWN_EVENT,
	KEY_UP_EVENT,
	MOUSE_MOVE_EVENT,
	MOUSE_INPUT_EVENT,
	MOUSE_SCROLL_EVENT,

	NUM_EVENT_TYPES
};

struct IEvent
{
	IEvent(EEventType Type, HWND hwnd_) : mType(Type), hwnd(hwnd_) {}

	EEventType mType = EEventType::NUM_EVENT_TYPES;
	HWND       hwnd = 0;
};


struct WindowResizeEvent : public IEvent
{
	WindowResizeEvent(int w, int h, HWND hwnd_) : IEvent(EEventType::WINDOW_RESIZE_EVENT, hwnd_), width(w), height(h) {}

	int width  = 0;
	int height = 0; 
};

struct WindowCloseEvent : public IEvent
{
	WindowCloseEvent(HWND hwnd_) : IEvent(EEventType::WINDOW_CLOSE_EVENT, hwnd_) {}

	mutable std::mutex mMtx;
	mutable std::condition_variable mCondVar;
};

struct ToggleFullscreenEvent : public IEvent
{
	ToggleFullscreenEvent(HWND hwnd_) : IEvent(EEventType::TOGGLE_FULLSCREEN_EVENT, hwnd_) {}
};

struct SetFullscreenEvent : public IEvent
{
	SetFullscreenEvent(HWND hwnd_, bool bFullscreen) : IEvent(EEventType::SET_FULLSCREEN_EVENT, hwnd_), bToggleValue(bFullscreen) {}
	bool bToggleValue = false;
};

enum class KeyDownEventData_Type
{
	KEYBOARD_DATA,
	MOUSE_DATA
};

class KeyDownEventData
{
public:
	using Tag = KeyDownEventData_Type;

	KeyDownEventData(WPARAM wp) : mTag(Tag::KEYBOARD_DATA), keyboard(wp)
	{}

	KeyDownEventData(WPARAM wp, bool cl) : mTag(Tag::MOUSE_DATA), mouse(wp,cl)
	{}

	Tag mTag;

	union
	{
		struct Keyboard
		{
			Keyboard(WPARAM wp) : wparam(wp) {}
			WPARAM wparam = 0;
		} keyboard;
		struct Mouse
		{
			Mouse(WPARAM wp, bool cl) : wparam(wp), bDoubleClick(cl) {}
			WPARAM wparam = 0;
			bool   bDoubleClick = false;
		} mouse;
	};
};


struct KeyDownEvent : public IEvent
{
	KeyDownEventData data;
	KeyDownEvent(HWND hwnd_, WPARAM wparam_, bool bDoubleClick_ = false)
		: IEvent(EEventType::KEY_DOWN_EVENT, hwnd_)
		, data(wparam_)
	{}
};
struct KeyUpEvent : public IEvent
{
	KeyUpEvent(HWND hwnd_, WPARAM wparam_) : IEvent(EEventType::KEY_UP_EVENT, hwnd_), wparam(wparam_) {}

	WPARAM wparam = 0;
};

struct MouseMoveEvent : public IEvent
{
	MouseMoveEvent(HWND hwnd_, long x_, long y_) : IEvent(EEventType::MOUSE_MOVE_EVENT, hwnd_), x(x_), y(y_) {}
	long x = 0;
	long y = 0;
};
struct MouseScrollEvent : public IEvent
{
	MouseScrollEvent(HWND hwnd_, short scr) : IEvent(EEventType::MOUSE_SCROLL_EVENT, hwnd_), scroll(scr) {}
	short scroll = 0;
};
struct MouseInputEventData
{
	int relativeX = 0;
	int relativeY = 0;
	union
	{
		unsigned long scrollChars;
		unsigned long scrollLines;
	};
	float scrollDelta = 0.0f;
};
struct MouseInputEvent : public IEvent
{
	MouseInputEvent(const MouseInputEventData& d, HWND hwnd_) : IEvent(EEventType::MOUSE_INPUT_EVENT, hwnd_), data(d) {}
	MouseInputEventData data;
};