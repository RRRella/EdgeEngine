#pragma once

#include "Events.h"
#include <array>
#include <unordered_map>
#include <string>
#include "MouseCodes.h"
#include "KeyCodes.h"

#define ENABLE_RAW_INPUT
#define ENABLE_RAW_INPUT 1

// A char[] is used for key mappings, large enough array will do
#define NUM_MAX_KEYS 256

#include "Window.h"

class Input
{
public:
	using KeyMapping = std::unordered_map<std::string_view, KeyCode>;

	static void InitRawInputDevices(HWND hwnd);
	static bool ReadRawInput_Mouse(LPARAM lParam, MouseInputEventData* pData);
	Input();
	Input(const Input&) = default;
	~Input() = default;

	inline void ToggleInputBypassing() { mbIgnoreInput = !mbIgnoreInput; }

	// update state
	void UpdateKeyDown(KeyDownEventData); // includes mouse button
	void UpdateKeyUp(KeyCode);	 // includes mouse button
	void UpdateMousePos(long x, long y, short scroll);
	void UpdateMousePos_Raw(int relativeX, int relativeY, short scroll, bool bMouseCaptured);

	bool IsKeyDown(KeyCode) const;
	bool IsKeyUp(KeyCode) const;
	bool IsKeyTriggered(KeyCode) const;
	int  MouseDeltaX() const;
	int  MouseDeltaY() const;
	bool IsMouseDown(MouseCode) const;
	bool IsMouseUp(MouseCode) const;
	bool IsMouseDoubleClick(MouseCode) const;
	bool IsMouseTriggered(MouseCode) const;
	bool IsMouseScrollUp() const;
	bool IsMouseScrollDown() const;

	void PostUpdate();
	inline const std::array<float, 2>& GetMouseDelta() const { return mMouseDelta; }

	private: // On/Off state is represented as char (8-bit) instead of bool (32-bit)
	// state
	bool mbIgnoreInput;
	// keyboard
	std::unordered_map<KeyCode, bool> mKeys;
	std::unordered_map<KeyCode, bool> mKeysPrevious;
	// mouse
	std::unordered_map<MouseCode, bool> mMouseButtons;
	std::unordered_map<MouseCode, bool> mMouseButtonsPrevious;
	std::unordered_map<MouseCode, bool> mMouseButtonDoubleClicks;

	short mMouseScroll;

	std::array<float, 2> mMouseDelta;
	std::array<long, 2> mMousePosition;
};