#include "WindowCapture.h"
#include <cstdlib>

namespace SonicVisualSplitBase {

// WindowCapture is based on https://github.com/sturkmen72/opencv_samples/blob/master/Screen-Capturing.cpp
WindowCapture::WindowCapture(HWND hwindow) {
    hwnd = hwindow;
    hwindowDC = GetDC(hwnd);
    hwindowCompatibleDC = CreateCompatibleDC(hwindowDC);
    SetStretchBltMode(hwindowCompatibleDC, COLORONCOLOR);

    EnsureWindowReadyForCapture();
    RECT windowSize;
    GetClientRect(hwnd, &windowSize);
    width = windowSize.right;
    height = windowSize.bottom;

    image.create(height, width, CV_8UC4);

    // create a bitmap
    hbwindow = CreateCompatibleBitmap(hwindowDC, width, height);
    bmpInfo.biSize = sizeof(BITMAPINFOHEADER);  // http://msdn.microsoft.com/en-us/library/windows/window/dd183402%28v=vs.85%29.aspx
    bmpInfo.biWidth = width;
    bmpInfo.biHeight = -height;  // this is the line that makes it draw upside down or not
    bmpInfo.biPlanes = 1;
    bmpInfo.biBitCount = 32;
    bmpInfo.biCompression = BI_RGB;
    bmpInfo.biSizeImage = 0;
    bmpInfo.biXPelsPerMeter = 0;
    bmpInfo.biYPelsPerMeter = 0;
    bmpInfo.biClrUsed = 0;
    bmpInfo.biClrImportant = 0;

    // use the previously created device context with the bitmap
    SelectObject(hwindowCompatibleDC, hbwindow);
};

WindowCapture::~WindowCapture() {
    DeleteObject(hbwindow);
    DeleteDC(hwindowCompatibleDC);
    ReleaseDC(hwnd, hwindowDC);
    delete fakeMinimize;
};


void WindowCapture::getScreenshot() {
    EnsureWindowReadyForCapture();
    // copy from the window device context to the bitmap device context
    BitBlt(hwindowCompatibleDC, 0, 0, width, height, hwindowDC, 0, 0, SRCCOPY);
    GetDIBits(hwindowCompatibleDC, hbwindow, 0, height, image.data, (BITMAPINFO*) &bmpInfo, DIB_RGB_COLORS);  // copy from hwindowCompatibleDC to hbwindow
};

bool SetMinimizeMaximizeAnimation(bool enabled) {
    ANIMATIONINFO animationInfo;
    animationInfo.cbSize = sizeof(animationInfo);
    animationInfo.iMinAnimate = enabled;
    SystemParametersInfo(SPI_GETANIMATION, animationInfo.cbSize, &animationInfo, 0);
    bool old = animationInfo.iMinAnimate;

    if ((bool) animationInfo.iMinAnimate != enabled) {
        animationInfo.iMinAnimate = enabled;
        SystemParametersInfo(SPI_SETANIMATION, animationInfo.cbSize, &animationInfo, SPIF_SENDCHANGE);
    }
    return old;
}

// Make sure the window isn't minimized; check the size and position of the window
void WindowCapture::EnsureWindowReadyForCapture() {
    bool redrawWindow = false;
    if (IsIconic(hwnd)) {
        delete fakeMinimize;
        fakeMinimize = new FakeMinimize(hwnd);
        redrawWindow = true;
    }

    RECT oldWindowPos;
    GetWindowRect(hwnd, &oldWindowPos);
    int width = oldWindowPos.right - oldWindowPos.left;
    int height = oldWindowPos.bottom - oldWindowPos.top;
    RECT windowPos = oldWindowPos;
    // check the size of the window (i.e. stream preview isn't too small)
    if (height < 720) {
        height = 720;
        windowPos.right = windowPos.left + height;
    }
    // mare sure the window is not off-screen
    HMONITOR hMonitor = MonitorFromRect(&windowPos, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    GetMonitorInfo(hMonitor, &mi);
    RECT rc = mi.rcMonitor;
    windowPos.left = std::max(rc.left, std::min(rc.right - width, windowPos.left));
    windowPos.top = std::max(rc.top, std::min(rc.bottom - height, windowPos.top));
    windowPos.right = windowPos.left + width;
    windowPos.bottom = windowPos.top + height;


    if (oldWindowPos.left != windowPos.left || oldWindowPos.top != windowPos.top
        || oldWindowPos.right != windowPos.right || oldWindowPos.bottom != windowPos.bottom) {
        SetWindowPos(hwnd, nullptr, windowPos.left, windowPos.top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
        redrawWindow = true;
    }

    if (redrawWindow) {
        RedrawWindow(hwnd, 0, 0, RDW_INVALIDATE | RDW_UPDATENOW);
    }
}

// FakeMinimize.
// We cannot make a screenshot of a minimized window, because of WinAPI limitations.
// So, what we are doing, is actually restoring a window, but making it transparent and click-through.
// The user doesn't see the window, so it's the same as if it's still minimized.
// When the user tries to "restore" the window (it's restored already), we intercept it with a hook
// and return the window to its original state.

FakeMinimize::FakeMinimize(HWND hwnd) {
    assert(IsIconic(hwnd));
    HWND oldActive = GetForegroundWindow();
    bool oldAnimStatus = SetMinimizeMaximizeAnimation(false);

    // enable transparency
    LONG winLong = GetWindowLong(hwnd, GWL_EXSTYLE);
    SetWindowLong(hwnd, GWL_EXSTYLE, winLong | WS_EX_LAYERED | WS_EX_TRANSPARENT);
    SetLayeredWindowAttributes(hwnd, 0, 1, LWA_ALPHA);

    ShowWindow(hwnd, SW_RESTORE);
    RECT position;
    GetWindowRect(hwnd, &position);
    {
        std::lock_guard<std::mutex> guard(originalWindowPositionsMutex);
        originalWindowPositions[hwnd] = {position.left, position.top};
    }
    const int SCREEN_POSITION = 0;
    SetWindowPos(hwnd, GetDesktopWindow(), SCREEN_POSITION, SCREEN_POSITION, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
    SetForegroundWindow(oldActive);
    SetMinimizeMaximizeAnimation(oldAnimStatus);
    addRestoreHook();
    registerAtExit();
}

FakeMinimize::~FakeMinimize() {
    std::lock_guard<std::mutex> guard(threadTerminationMutex);
    TerminateThread(messageLoopThread, 0);
}

void FakeMinimize::addRestoreHook() {
    messageLoopThread = CreateThread(0, 0, addRestoreHookProc, 0, 0, 0);
}

DWORD WINAPI FakeMinimize::addRestoreHookProc(LPVOID lpParameter) {
    SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, onForegroundWindowChanged, 0, 0, WINEVENT_OUTOFCONTEXT);
    MSG msg;
    BOOL bRet;

    while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0) {
        std::lock_guard<std::mutex> guard(threadTerminationMutex);
        if (bRet == -1) {  // error
            return 0;
        }
        else {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return 0;
}

void CALLBACK FakeMinimize::onForegroundWindowChanged(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG idObject, LONG idChild,
    DWORD dwEventThread, DWORD dwmsEventTime) {

    POINT originalPosition;
    {
        std::lock_guard<std::mutex> guard(originalWindowPositionsMutex);
        auto findIter = originalWindowPositions.find(hwnd);
        if (findIter == originalWindowPositions.end())  // hwnd is not the OBS window
            return;
        originalPosition = findIter->second;
        originalWindowPositions.erase(findIter);
    }
    restoreWindow({hwnd, originalPosition});
    UnhookWinEvent(hook);
    return;
}

// Return the window to the original position and remove transparency (as if the user restored the window)
void FakeMinimize::restoreWindow(std::pair<HWND, POINT> windowAndOriginalPosition) {
    const auto& [hwnd, originalPosition] = windowAndOriginalPosition;
    LONG winLong = GetWindowLong(hwnd, GWL_EXSTYLE);
    SetWindowLong(hwnd, GWL_EXSTYLE, winLong & (~WS_EX_LAYERED) & (~WS_EX_TRANSPARENT));
    SetWindowPos(hwnd, nullptr, originalPosition.x, originalPosition.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

// Restore all the windows that we've hidden
void FakeMinimize::restoreRemainingWindows() {
    std::lock_guard<std::mutex> guard(originalWindowPositionsMutex);
    for (const auto& windowAndOriginalPosition : originalWindowPositions) {
        restoreWindow(windowAndOriginalPosition);
    }
}

void FakeMinimize::registerAtExit() {
    std::atexit(restoreRemainingWindows);
}

}  // namespace SonicVisualSplitBase