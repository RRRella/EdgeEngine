#include "Input.h"
#include "../Utils/Source/Log.h"
#include <algorithm>
#include <cassert>
#define VERBOSE_LOGGING 0
#define NOMINMAX
// keyboard mapping for windows keycodes.
// use case: this->IsKeyDown("F4")

static constexpr bool IsMouseKey(WPARAM wparam)
{
	return wparam == static_cast<WPARAM>(MOUSE_BUTTON_LEFT)
		|| wparam == static_cast<WPARAM>(MOUSE_BUTTON_RIGHT)
		|| wparam == static_cast<WPARAM>(MOUSE_BUTTON_MIDDLE)

		|| wparam == static_cast<WPARAM>(static_cast<uint16_t>(MOUSE_BUTTON_LEFT)   | 
										 static_cast<uint16_t>(MOUSE_BUTTON_RIGHT))

		|| wparam == static_cast<WPARAM>(static_cast<uint16_t>(MOUSE_BUTTON_MIDDLE) | 
										 static_cast<uint16_t>(MOUSE_BUTTON_RIGHT))

		|| wparam == static_cast<WPARAM>(static_cast<uint16_t>(MOUSE_BUTTON_MIDDLE) | 
										 static_cast<uint16_t>(MOUSE_BUTTON_LEFT))

		|| wparam == static_cast<WPARAM>(static_cast<uint16_t>(MOUSE_BUTTON_MIDDLE) | 
										 static_cast<uint16_t>(MOUSE_BUTTON_LEFT)	| static_cast<uint16_t>(MOUSE_BUTTON_RIGHT));
}

bool Input::ReadRawInput_Mouse(LPARAM lParam, MouseInputEventData* pData)
{
	constexpr UINT RAW_INPUT_SIZE_IN_BYTES = 48;
	UINT rawInputSize = RAW_INPUT_SIZE_IN_BYTES;
	LPBYTE inputBuffer[RAW_INPUT_SIZE_IN_BYTES];
	ZeroMemory(inputBuffer, RAW_INPUT_SIZE_IN_BYTES);

	// https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getrawinputdata
	GetRawInputData(
		(HRAWINPUT)lParam,
		RID_INPUT,
		inputBuffer,
		&rawInputSize,
		sizeof(RAWINPUTHEADER));

	// https://docs.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-rawmouse
	RAWINPUT* raw = (RAWINPUT*)inputBuffer;    
	assert(raw);
	RAWMOUSE rawMouse = raw->data.mouse;
	bool bIsMouseInput = false;

	// Handle Wheel
	if ((rawMouse.usButtonFlags & RI_MOUSE_WHEEL) == RI_MOUSE_WHEEL ||
		(rawMouse.usButtonFlags & RI_MOUSE_HWHEEL) == RI_MOUSE_HWHEEL)
	{
		static const unsigned long defaultScrollLinesPerWheelDelta = 3;
		static const unsigned long defaultScrollCharsPerWheelDelta = 1;
		float wheelDelta = (float)(short)rawMouse.usButtonData;
		float numTicks = wheelDelta / WHEEL_DELTA;
		bool isHorizontalScroll = (rawMouse.usButtonFlags & RI_MOUSE_HWHEEL) == RI_MOUSE_HWHEEL;
		bool isScrollByPage = false;
		float scrollDelta = numTicks;
		if (isHorizontalScroll)
		{
			pData->scrollChars = defaultScrollCharsPerWheelDelta;
			SystemParametersInfo(SPI_GETWHEELSCROLLCHARS, 0, &pData->scrollChars, 0);
			scrollDelta *= pData->scrollChars;
		}
		else
		{
			pData->scrollLines = defaultScrollLinesPerWheelDelta;
			SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &pData->scrollLines, 0);
			if (pData->scrollLines == WHEEL_PAGESCROLL)
				isScrollByPage = true;
			else
				scrollDelta *= pData->scrollLines;
		}
		pData->scrollDelta = scrollDelta;
		bIsMouseInput = true;
	}
	// Handle Move
	if ((rawMouse.usFlags & MOUSE_MOVE_ABSOLUTE) == MOUSE_MOVE_ABSOLUTE)
	{
		bool isVirtualDesktop = (rawMouse.usFlags & MOUSE_VIRTUAL_DESKTOP) == MOUSE_VIRTUAL_DESKTOP;
		int width = GetSystemMetrics(isVirtualDesktop ? SM_CXVIRTUALSCREEN : SM_CXSCREEN);
		int height = GetSystemMetrics(isVirtualDesktop ? SM_CYVIRTUALSCREEN : SM_CYSCREEN);
		int absoluteX = int((rawMouse.lLastX / 65535.0f) * width);
		int absoluteY = int((rawMouse.lLastY / 65535.0f) * height);
	}
	else if (rawMouse.lLastX != 0 || rawMouse.lLastY != 0)
	{
		pData->relativeX = rawMouse.lLastX;
		pData->relativeY = rawMouse.lLastY;
		bIsMouseInput = true;
	}
#if LOG_RAW_INPUT
	char szTempOutput[1024];
	StringCchPrintf(szTempOutput, STRSAFE_MAX_CCH, TEXT("%u  Mouse: usFlags=%04x ulButtons=%04x usButtonFlags=%04x usButtonData=%04x ulRawButtons=%04x lLastX=%04x lLastY=%04x ulExtraInformation=%04x\r\n"),
		rawInputSize,
		raw->data.mouse.usFlags,
		raw->data.mouse.ulButtons,
		raw->data.mouse.usButtonFlags,
		raw->data.mouse.usButtonData,
		raw->data.mouse.ulRawButtons,
		raw->data.mouse.lLastX,
		raw->data.mouse.lLastY,
		raw->data.mouse.ulExtraInformation);
	OutputDebugString(szTempOutput);
#endif
	return bIsMouseInput;
}

void Input::InitRawInputDevices(HWND hwnd)
{
	// register mouse for raw input
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ms645565.aspx
	RAWINPUTDEVICE Rid[1];
	Rid[0].usUsagePage = (USHORT)0x01;	// HID_USAGE_PAGE_GENERIC;
	Rid[0].usUsage = (USHORT)0x02;	// HID_USAGE_GENERIC_MOUSE;
	Rid[0].dwFlags = 0;
	Rid[0].hwndTarget = hwnd;
	if (FALSE == (RegisterRawInputDevices(Rid, 1, sizeof(Rid[0]))))	// Cast between semantically different integer types : a Boolean type to HRESULT.
	{
		OutputDebugString("Failed to register raw input device!");
	}
	// get devices and print info
	//-----------------------------------------------------
	UINT numDevices = 0;
	GetRawInputDeviceList(
		NULL, &numDevices, sizeof(RAWINPUTDEVICELIST));
	if (numDevices == 0) return;
	std::vector<RAWINPUTDEVICELIST> deviceList(numDevices);
	GetRawInputDeviceList(
		&deviceList[0], &numDevices, sizeof(RAWINPUTDEVICELIST));
	std::vector<wchar_t> deviceNameData;
	std::wstring deviceName;
	for (UINT i = 0; i < numDevices; ++i)
	{
		const RAWINPUTDEVICELIST& device = deviceList[i];
		if (device.dwType == RIM_TYPEMOUSE)
		{
			char info[1024];
			sprintf_s(info, "Mouse: Handle=0x%08p\n", device.hDevice);
			OutputDebugString(info);
			UINT dataSize = 0;
			GetRawInputDeviceInfo(
				device.hDevice, RIDI_DEVICENAME, nullptr, &dataSize);
			if (dataSize)
			{
				deviceNameData.resize(dataSize);
				UINT result = GetRawInputDeviceInfo(
					device.hDevice, RIDI_DEVICENAME, &deviceNameData[0], &dataSize);
				if (result != UINT_MAX)
				{
					deviceName.assign(deviceNameData.begin(), deviceNameData.end());
					char info[1024];
					std::string ndeviceName(deviceName.begin(), deviceName.end());
					sprintf_s(info, "  Name=%s\n", ndeviceName.c_str());
					OutputDebugString(info);
				}
			}
			RID_DEVICE_INFO deviceInfo;
			deviceInfo.cbSize = sizeof deviceInfo;
			dataSize = sizeof deviceInfo;
			UINT result = GetRawInputDeviceInfo(
				device.hDevice, RIDI_DEVICEINFO, &deviceInfo, &dataSize);
			if (result != UINT_MAX)
			{
			#ifdef _DEBUG
				assert(deviceInfo.dwType == RIM_TYPEMOUSE);
			#endif
				char info[1024];
				sprintf_s(info,
					"  Id=%u, Buttons=%u, SampleRate=%u, HorizontalWheel=%s\n",
					deviceInfo.mouse.dwId,
					deviceInfo.mouse.dwNumberOfButtons,
					deviceInfo.mouse.dwSampleRate,
					deviceInfo.mouse.fHasHorizontalWheel ? "1" : "0");
				OutputDebugString(info);
			}
		}
	}
}

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

	mMouseButtonDoubleClicks = mMouseButtonsPrevious = mMouseButtons;
}

void Input::PostUpdate()
{
	mKeysPrevious = mKeys;
	mMouseButtonsPrevious = mMouseButtons;
	// Reset Mouse Data
	mMouseDelta[0] = mMouseDelta[1] = 0;
	mMouseScroll = 0;
	Log::Info("Input::PostUpdate() : scroll=%d", mMouseScroll);
}

void Input::UpdateKeyDown(KeyDownEventData data)
{
	const auto& key = data.mouse.wparam;
	if (IsMouseKey(key))
	{
		const MouseCode mouseBtn = static_cast<MouseCode>(key);
		// if left & right mouse is clicked the same time, @key will be
		// Input::EMouseButtons::MOUSE_BUTTON_LEFT | Input::EMouseButtons::MOUSE_BUTTON_RIGHT
		if (mouseBtn & MOUSE_BUTTON_LEFT)   mMouseButtons[MOUSE_BUTTON_LEFT] = true;
		if (mouseBtn & MOUSE_BUTTON_RIGHT)  mMouseButtons[MOUSE_BUTTON_RIGHT] = true;
		if (mouseBtn & MOUSE_BUTTON_MIDDLE) mMouseButtons[MOUSE_BUTTON_MIDDLE] = true;
		if (data.mouse.bDoubleClick)
		{
			if (mouseBtn & MOUSE_BUTTON_LEFT)   mMouseButtonDoubleClicks[MOUSE_BUTTON_LEFT] = true;
			if (mouseBtn & MOUSE_BUTTON_RIGHT)  mMouseButtonDoubleClicks[MOUSE_BUTTON_RIGHT] = true;
			if (mouseBtn & MOUSE_BUTTON_MIDDLE) mMouseButtonDoubleClicks[MOUSE_BUTTON_MIDDLE] = true;
#if VERBOSE_LOGGING
			Log::Info("Double Click!!");
#endif
		}
	}
	else
		mKeys[(KeyCode)key] = true;
}


void Input::UpdateKeyUp(KeyCode key)
{
	if (IsMouseKey(static_cast<WPARAM>(key)))
	{
		const MouseCode mouseBtn = static_cast<MouseCode>(key);

		mMouseButtons[mouseBtn] = false;
		mMouseButtonDoubleClicks[mouseBtn] = false;
#if VERBOSE_LOGGING
		Log::Info("Mouse Key Up %x", key);
#endif
	}
	else
		mKeys[key] = false;
}

void Input::UpdateMousePos(long x, long y, short scroll)
{
	mMouseDelta[0] = static_cast<float>(max(-1l, min(x - mMousePosition[0], 1l)));
	mMouseDelta[1] = static_cast<float>(max(-1l, min(y - mMousePosition[1], 1l)));

#if defined(_DEBUG) && VERBOSE_LOGGING
	Log::Info("Mouse Delta: (%d, %d)\tMouse Position: (%d, %d)\tMouse Scroll: (%d)",
		mMouseDelta[0], mMouseDelta[1],
		mMousePosition[0], mMousePosition[1],
		(int)scroll);
#endif
	mMouseScroll = scroll;
}

void Input::UpdateMousePos_Raw(int relativeX, int relativeY, short scroll, bool bMouseCaptured)
{
	if (bMouseCaptured)
	{
		//SetCursorPos(setting.width / 2, setting.height / 2);
		mMouseDelta[0] = static_cast<float>(relativeX);
		mMouseDelta[1] = static_cast<float>(relativeY);
		// unused for now
		mMousePosition[0] = 0;
		mMousePosition[1] = 0;
		mMouseScroll = scroll;
		if (scroll != 0)
		{
			Log::Info("Scroll: %d", mMouseScroll);
		}
	}
}


bool Input::IsKeyDown(KeyCode key) const
{
	return (GetAsyncKeyState((int)key)) != 0;
}
bool Input::IsKeyUp(KeyCode key) const
{
	return (!IsKeyDown(key) && mKeysPrevious.at(key)) && !mbIgnoreInput;
}

bool Input::IsKeyTriggered(KeyCode key) const
{
	return !mKeysPrevious.at(key) && IsKeyDown(key) && !mbIgnoreInput;
}

int Input::MouseDeltaX() const { return !mbIgnoreInput ? (int)mMouseDelta[0] : 0; }
int Input::MouseDeltaY() const { return !mbIgnoreInput ? (int)mMouseDelta[1] : 0; }

bool Input::IsMouseDown(MouseCode mbtn) const
{
	return !mbIgnoreInput && mMouseButtons.at(mbtn);
}

bool Input::IsMouseDoubleClick(MouseCode mbtn) const
{
	return !mbIgnoreInput && mMouseButtonDoubleClicks.at(mbtn);
}

// called at the end of the frame

bool Input::IsMouseUp(MouseCode mbtn) const
{
	const bool bButtonUp = !mMouseButtons.at(mbtn) && mMouseButtonsPrevious.at(mbtn);
	return !mbIgnoreInput && bButtonUp;
}

bool Input::IsMouseTriggered(MouseCode mbtn) const
{
	const bool bButtonTriggered = mMouseButtons.at(mbtn) && !mMouseButtonsPrevious.at(mbtn);
	return !mbIgnoreInput && bButtonTriggered;
}
bool Input::IsMouseScrollUp() const
{
	return mMouseScroll > 0 && !mbIgnoreInput;
}
bool Input::IsMouseScrollDown() const
{
	Log::Info("Input::IsMouseScrollDown() : scroll=%d", mMouseScroll);
	return mMouseScroll < 0 && !mbIgnoreInput;
}
