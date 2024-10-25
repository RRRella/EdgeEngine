#pragma once
#include <array>
#include <unordered_map>
#include <string>
#include "MouseCodes.h"
#include "KeyCodes.h"

#define ENABLE_RAW_INPUT
// A char[] is used for key mappings, large enough array will do
#define NUM_MAX_KEYS 256
#include "Window.h"

class Input
{
public:
	using KeyMapping = std::unordered_map<std::string_view, KeyCode>;
	
	Input();
	Input(const Input&) = default;
	~Input() = default;
	inline void ToggleInputBypassing() { mbIgnoreInput = !mbIgnoreInput; }
	// update state
	void UpdateKeyDown(KeyCode);
	void UpdateKeyUp(KeyCode);
	void UpdateButtonDown(MouseCode);
	void UpdateButtonUp(MouseCode);
	void UpdateMousePos(long x, long y, short scroll);
	bool IsKeyDown(KeyCode);
	bool IsKeyUp(KeyCode key);
	bool IsKeyTriggered(KeyCode);
	int  MouseDeltaX() const;
	int  MouseDeltaY() const;
	bool IsScrollUp() const;
	bool IsScrollDown() const;
	void PostUpdate();
	inline const std::array<float, 2>& GetMouseDelta() const { return mMouseDelta; }
private: // On/Off state is represented as char (8-bit) instead of bool (32-bit)
	// state
	bool mbIgnoreInput;
	// keyboard
	std::unordered_map<KeyCode, bool> mKeys;
	std::unordered_map<KeyCode, bool> mKeysPrevious;
	// mouse
	std::unordered_map < MouseCode, bool > mMouseButtons;
	std::array<float, 2> mMouseDelta;
	std::array<long, 2> mMousePosition;
	short mMouseScroll;
};