#pragma once

#include <string>
#include <Windows.h>
#include <memory>


struct IWindow;
class SwapChain;


/**
* Encapsulate a window class.
*
* Calls <c>::RegisterClass()</c> on create, and <c>::UnregisterClass()</c>
* on destruction.
*/
struct WindowClass final
{
public:
	WindowClass(const std::string& name,
		HINSTANCE hInst,
		::WNDPROC procedure = ::DefWindowProc);
	~WindowClass();

	const std::string& GetName() const;

	WindowClass(const WindowClass&) = delete;
	WindowClass& operator= (const WindowClass&) = delete;

private:
	std::string name_;
};

class IWindowOwner
{
public:
	virtual void OnWindowCreate(IWindow* pWnd) = 0;
	virtual void OnWindowResize(HWND) = 0;
	virtual void OnToggleFullscreen(HWND) = 0;
	virtual void OnWindowMinimize(IWindow* pWnd) = 0;
	virtual void OnWindowFocus(IWindow* pWnd) = 0;
	virtual void OnWindowClose(IWindow* pWnd) = 0;
	virtual void OnWindowKeyDown(WPARAM) = 0;
};

struct IWindow
{
public:
	IWindow(IWindowOwner* pOwner_) : pOwner(pOwner_) {}
	IWindow() = default;
	IWindow(const IWindow&) = delete;
	IWindow& operator=(const IWindow&) = delete;

	virtual ~IWindow();

	virtual void Show()     = 0;
	virtual void ToggleWindowedFullscreen(SwapChain* pSwapChain = nullptr) = 0;
	virtual void Minimize() = 0;
	virtual void Close()    = 0;

	bool IsClosed() const;
	bool IsFullscreen() const;

	inline int GetWidth() const            { return GetWidthImpl(); }
	inline int GetHeight() const           { return GetHeightImpl(); }
	inline int GetFullscreenWidth() const  { return GetFullscreenWidthImpl(); }
	inline int GetFullscreenHeight() const { return GetFullscreenHeightImpl(); }

	IWindowOwner* pOwner;
private:
	virtual bool IsClosedImpl() const = 0;
	virtual bool IsFullscreenImpl() const = 0;
	virtual int GetWidthImpl() const = 0;
	virtual int GetHeightImpl() const = 0;
	virtual int GetFullscreenWidthImpl() const = 0;
	virtual int GetFullscreenHeightImpl() const = 0;
};


using pfnWndProc_t = LRESULT(CALLBACK*)(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

struct FWindowDesc
{
	int width  = -1;
	int height = -1;
	HINSTANCE hInst = NULL;
	pfnWndProc_t pfnWndProc = nullptr;
	IWindowOwner* pWndOwner = nullptr;
	bool bFullscreen = false;
	int preferredDisplay = 0;
};

class Window : public IWindow
{
public:
	Window(const std::string& title, FWindowDesc& initParams);

	HWND GetHWND() const;

	void Show() override;
	void Minimize() override;
	void Close() override;
	void ToggleWindowedFullscreen(SwapChain* pSwapChain = nullptr) override;

	// updates width_ and height_ members
	inline void OnResize(int w, int h) { width_ = w; height_ = h; }
	inline void SetFullscreen(bool b) { isFullscreen_ = b; }

private:
	inline bool IsClosedImpl()  const override { return isClosed_; }
	inline bool IsFullscreenImpl() const override { return isFullscreen_; }
	inline int  GetWidthImpl()  const override { return width_; }
	inline int  GetHeightImpl() const override { return height_; }
	inline int  GetFullscreenWidthImpl()  const override { return FSwidth_; }
	inline int  GetFullscreenHeightImpl() const override { return FSheight_; }

	std::unique_ptr<WindowClass> windowClass_;
	HWND hwnd_ = 0;
	RECT rect_;
	bool isClosed_ = false;
	int width_ = -1, height_ = -1;
	bool isFullscreen_ = false;
	UINT windowStyle_;
	int FSwidth_ = -1, FSheight_ = -1;
};
