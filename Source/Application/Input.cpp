#include "Input.h"
#include <algorithm>
#define VERBOSE_LOGGING 0
// keyboard mapping for windows keycodes.
// use case: this->IsKeyDown("F4")
Input::Input()
	:
	mbIgnoreInput(false),
	mMouseDelta{ 0, 0 },
	mMousePosition{ 0, 0 },
	mMouseScroll(0)
{
	mMouseButtons[MOUSE_BUTTON_LEFT]   = false;
	mMouseButtons[MOUSE_BUTTON_RIGHT]  = false;
	mMouseButtons[MOUSE_BUTTON_MIDDLE] = false;
}
void Input::UpdateKeyDown(KeyCode key)
{
	mKeys[key] = true;
}
void Input::UpdateKeyUp(KeyCode key)
{
	mKeys[key] = false;
}
void Input::UpdateButtonDown(MouseCode btn)
{
	mMouseButtons[btn] = true;
}
void Input::UpdateButtonUp(MouseCode btn)
{
	mMouseButtons[btn] = false;
}
void Input::UpdateMousePos(long x, long y, short scroll)
{

	mMouseDelta[0] = static_cast<float>(x);
	mMouseDelta[1] = static_cast<float>(y);
	// unused for now
	mMousePosition[0] = 0;
	mMousePosition[1] = 0;

#if defined(_DEBUG) && VERBOSE_LOGGING
	Log::Info("Mouse Delta: (%d, %d)\tMouse Position: (%d, %d)\tMouse Scroll: (%d)",
		mMouseDelta[0], mMouseDelta[1],
		mMousePosition[0], mMousePosition[1],
		(int)scroll);
#endif
	mMouseScroll = scroll;
}
bool Input::IsScrollUp() const
{
	return mMouseScroll > 0 && !mbIgnoreInput;
}
bool Input::IsScrollDown() const
{
	return mMouseScroll < 0 && !mbIgnoreInput;
}
bool Input::IsKeyDown(KeyCode key)
{
	return (GetAsyncKeyState((int)key)) != 0;
}
bool Input::IsKeyUp(KeyCode key)
{
	return (!IsKeyDown(key) && mKeysPrevious.at(key)) && !mbIgnoreInput;
}

bool Input::IsKeyTriggered(KeyCode key)
{
	return !mKeysPrevious.at(key) && IsKeyDown(key) && !mbIgnoreInput;
}

int Input::MouseDeltaX() const
{
	return !mbIgnoreInput ? mMouseDelta[0] : 0;
}
int Input::MouseDeltaY() const
{
	return !mbIgnoreInput ? mMouseDelta[1] : 0;
}

// called at the end of the frame

void Input::PostUpdate()
{
	mKeysPrevious = mKeys;
	mMouseDelta[0] = mMouseDelta[1] = 0;
	mMouseScroll = 0;
	mMouseButtons[MOUSE_BUTTON_LEFT]   = false;
	mMouseButtons[MOUSE_BUTTON_RIGHT]  = false;
	mMouseButtons[MOUSE_BUTTON_MIDDLE] = false;
}