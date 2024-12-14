#include "Window.h"
#include "../Renderer/SwapChain.h"
#include "../Utils/Source/Log.h"
#include "../Utils/Source/utils.h"
#include "Data/Resources/resource.h"

#include <dxgi1_6.h>

static RECT CenterScreen(const RECT& screenRect, const RECT& wndRect)
{
    RECT centered = {};

    const int szWndX = wndRect.right - wndRect.left;
    const int szWndY = wndRect.bottom - wndRect.top;
    const int offsetX = (screenRect.right - screenRect.left - szWndX) / 2;
    const int offsetY = (screenRect.bottom - screenRect.top - szWndY) / 2;

    centered.left = screenRect.left + offsetX;
    centered.right = centered.left + szWndX;
    centered.top = screenRect.top + offsetY;
    centered.bottom = centered.top + szWndY;

    return centered;
}

static RECT GetScreenRectOnPreferredDisplay(const RECT& preferredRect, int PreferredDisplayIndex)
{
    // handle preferred display
    struct MonitorEnumCallbackParams
    {
        int PreferredMonitorIndex = 0;
        const RECT* pRectOriginal = nullptr;
        RECT* pRectNew = nullptr;
    };
    RECT preferredScreenRect = { CW_USEDEFAULT , CW_USEDEFAULT , CW_USEDEFAULT , CW_USEDEFAULT };
    MonitorEnumCallbackParams p = {};
    p.PreferredMonitorIndex = PreferredDisplayIndex;
    p.pRectOriginal = &preferredRect;
    p.pRectNew = &preferredScreenRect;

    auto fnCallbackMonitorEnum = [](HMONITOR Arg1, HDC Arg2, LPRECT Arg3, LPARAM Arg4) -> BOOL
        {
            BOOL b = TRUE;
            MonitorEnumCallbackParams* pParam = (MonitorEnumCallbackParams*)Arg4;

            MONITORINFOEX monitorInfo = {};
            monitorInfo.cbSize = sizeof(MONITORINFOEX);
            GetMonitorInfo(Arg1, &monitorInfo);

            // get monitor index from monitor name
            std::string monitorName(monitorInfo.szDevice); // monitorName is usually something like "///./DISPLAY1"
            monitorName = StrUtil::split(monitorName, { '/', '\\', '.' })[0];         // strMonitorIndex is "1" for "///./DISPLAY1"
            std::string strMonitorIndex = monitorName.substr(monitorName.size() - 1); // monitorIndex    is  0  for "///./DISPLAY1"
            const int monitorIndex = std::atoi(strMonitorIndex.c_str()) - 1;        // -1 so it starts from 0

            // copy over the desired monitor's rect
            if (monitorIndex == pParam->PreferredMonitorIndex)
            {
                *pParam->pRectNew = *Arg3;
            }
            return b;
        };

    EnumDisplayMonitors(NULL, NULL, fnCallbackMonitorEnum, (LPARAM)&p);
    const bool bPreferredDisplayNotFound =
        (preferredScreenRect.right == preferredScreenRect.left == preferredScreenRect.top == preferredScreenRect.bottom)
        && (preferredScreenRect.right == CW_USEDEFAULT);

    return bPreferredDisplayNotFound ? preferredScreenRect : CenterScreen(preferredScreenRect, preferredRect);
}



///////////////////////////////////////////////////////////////////////////////
IWindow::~IWindow()
{
}

///////////////////////////////////////////////////////////////////////////////
Window::Window(const std::string& title, FWindowDesc& initParams)
    : IWindow(initParams.pWndOwner)
    , width_(initParams.width)
    , height_(initParams.height)
    , isFullscreen_(initParams.bFullscreen)
{
    // https://docs.microsoft.com/en-us/windows/win32/winmsg/window-styles
    UINT FlagWindowStyle = WS_OVERLAPPEDWINDOW;

    ::RECT rect;
    ::SetRect(&rect, 0, 0, width_, height_);
    ::AdjustWindowRect(&rect, FlagWindowStyle, FALSE);

    HWND hwnd_parent = NULL;

    windowClass_.reset(new WindowClass("WindowClass", initParams.hInst, initParams.pfnWndProc));

    RECT preferredScreenRect = GetScreenRectOnPreferredDisplay(rect, initParams.preferredDisplay);


    // set fullscreen width & height based on the selected monitor
    this->FSwidth_ = preferredScreenRect.right - preferredScreenRect.left;
    this->FSheight_ = preferredScreenRect.bottom - preferredScreenRect.top;

    // https://docs.microsoft.com/en-us/windows/win32/learnwin32/creating-a-window
    // Create the main window.
    hwnd_ = CreateWindowEx(NULL,
        windowClass_->GetName().c_str(),
        title.c_str(),
        FlagWindowStyle,
        preferredScreenRect.left,      // positions //CW_USEDEFAULT,
        preferredScreenRect.top,      // positions //CW_USEDEFAULT,
        rect.right - rect.left, // size
        rect.bottom - rect.top, // size
        hwnd_parent,
        NULL,    // we aren't using menus, NULL
        initParams.hInst,   // application handle
        NULL);   // used with multiple windows, NULL

    if (initParams.pRegistrar && initParams.pfnRegisterWindowName)
    {
        (initParams.pRegistrar->*initParams.pfnRegisterWindowName)(hwnd_, initParams.windowName);
    }

    windowStyle_ = FlagWindowStyle;

    // TODO: initial Show() sets the resolution low for the first frame.
    //       Workaround: RenderThread() calls HandleEvents() right before looping to handle the first ShowWindow() before Present().
    ::ShowWindow(hwnd_, initParams.iShowCmd);

    // set the data after the window shows up the first time.
    // otherwise, the Focus event will be sent on ::ShowWindow() before 
    // this function returns, causing issues dereferencing the smart
    // pointer calling this ctor.
    ::SetWindowLongPtr(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR> (this));
}

///////////////////////////////////////////////////////////////////////////////
bool IWindow::IsClosed()     const { return IsClosedImpl(); }
bool IWindow::IsFullscreen() const { return IsFullscreenImpl(); }
bool IWindow::IsMouseCaptured() const { return IsMouseCapturedImpl(); }

///////////////////////////////////////////////////////////////////////////////
HWND Window::GetHWND() const
{
    return hwnd_;
}

void Window::Show()
{
    ::ShowWindow(hwnd_, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd_);
}

void Window::Minimize()
{

    ::ShowWindow(hwnd_, SW_MINIMIZE);
}

void Window::Close()
{
    Log::Info("Window: Closing<%x>", this->hwnd_);
    this->isClosed_ = true;
    ::ShowWindow(hwnd_, FALSE);
    ::DestroyWindow(hwnd_);
}

// from MS D3D12Fullscreen sample
void Window::ToggleWindowedFullscreen(SwapChain* pSwapChain /*= nullptr*/)
{
    if (isFullscreen_)
    {
        // Restore the window's attributes and size.
        SetWindowLong(hwnd_, GWL_STYLE, windowStyle_);

        SetWindowPos(
            hwnd_,
            HWND_NOTOPMOST,
            rect_.left,
            rect_.top,
            rect_.right - rect_.left,
            rect_.bottom - rect_.top,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);

        ShowWindow(hwnd_, SW_NORMAL);
    }
    else
    {
        // Save the old window rect so we can restore it when exiting fullscreen mode.
        GetWindowRect(hwnd_, &rect_);

        // Make the window borderless so that the client area can fill the screen.
        SetWindowLong(hwnd_, GWL_STYLE, windowStyle_ & ~(WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_THICKFRAME));

        RECT fullscreenWindowRect;

        if (pSwapChain)
        {
            // Get the settings of the display on which the app's window is currently displayed
            IDXGIOutput* pOutput = nullptr;
            pSwapChain->mpSwapChain->GetContainingOutput(&pOutput);
            DXGI_OUTPUT_DESC Desc;
            pOutput->GetDesc(&Desc);
            fullscreenWindowRect = Desc.DesktopCoordinates;
            pOutput->Release();
        }
        else
        {
            // Fallback to EnumDisplaySettings implementation
            // Get the settings of the primary display
            DEVMODE devMode = {};
            devMode.dmSize = sizeof(DEVMODE);
            EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &devMode);

            fullscreenWindowRect = {
                devMode.dmPosition.x,
                devMode.dmPosition.y,
                devMode.dmPosition.x + static_cast<LONG>(devMode.dmPelsWidth),
                devMode.dmPosition.y + static_cast<LONG>(devMode.dmPelsHeight)
            };
        }

        SetWindowPos(
            hwnd_,
            HWND_TOPMOST,
            fullscreenWindowRect.left,
            fullscreenWindowRect.top,
            fullscreenWindowRect.right,
            fullscreenWindowRect.bottom,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);

        ShowWindow(hwnd_, SW_MAXIMIZE);

        // save fullscreen width & height 
        this->FSwidth_ = fullscreenWindowRect.right - fullscreenWindowRect.left;
        this->FSheight_ = fullscreenWindowRect.bottom - fullscreenWindowRect.top;
    }

    isFullscreen_ = !isFullscreen_;
}

void Window::SetMouseCapture(bool bCapture)
{
#if VERBOSE_LOGGING
    Log::Warning("Capture Mouse: %d", bCapture);
#endif

    isMouseCaptured_ = bCapture;
    if (bCapture)
    {
        RECT rcClip;
        GetWindowRect(hwnd_, &rcClip);

        // keep clip cursor rect inside pixel area
        // TODO: properly do it with rects
        constexpr int PX_OFFSET = 15;
        constexpr int PX_WND_TITLE_OFFSET = 30;
        rcClip.left += PX_OFFSET;
        rcClip.right -= PX_OFFSET;
        rcClip.top += PX_OFFSET + PX_WND_TITLE_OFFSET;
        rcClip.bottom -= PX_OFFSET;

        // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showcursor
        int hr = ShowCursor(TRUE);
        switch (hr)
        {
        case -1: Log::Warning("ShowCursor(FALSE): No mouse is installed!"); break;
        case 0: break;
            //default: Log::Info("ShowCursor(FALSE): %d", hr); break;
        }

        ClipCursor(&rcClip);
        SetForegroundWindow(hwnd_);
        SetFocus(hwnd_);
    }
    else
    {
        ClipCursor(nullptr);
        while (ShowCursor(TRUE) <= 0);
        SetForegroundWindow(NULL);
        // SetFocus(NULL);	// we still want to register events
    }

}

/////////////////////////////////////////////////////////////////////////
WindowClass::WindowClass(const std::string& name, HINSTANCE hInst, ::WNDPROC procedure)
    : name_(name)
{
    ::WNDCLASSEX wc = {};

    // Register the window class for the main window.
    // https://docs.microsoft.com/en-us/windows/win32/winmsg/window-class-styles
    wc.style = CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = procedure;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInst;
    wc.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON1));
    if (wc.hIcon == NULL)
    {
        DWORD dw = GetLastError();
        Log::Warning("Couldn't load icon for window: 0x%x", dw);
    }
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszMenuName = NULL;
    wc.lpszClassName = name_.c_str();
    wc.cbSize = sizeof(WNDCLASSEX);

    ::RegisterClassEx(&wc);
}

/////////////////////////////////////////////////////////////////////////
const std::string& WindowClass::GetName() const
{
    return name_;
}

/////////////////////////////////////////////////////////////////////////
WindowClass::~WindowClass()
{
    ::UnregisterClassA(name_.c_str(), (HINSTANCE)::GetModuleHandle(NULL));
}