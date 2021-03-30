#include "WindowCapture.h"
#include <cassert>


namespace SonicVisualSplitBase {

// WindowCapture is based on https://github.com/sturkmen72/opencv_samples/blob/master/Screen-Capturing.cpp

WindowCapture::WindowCapture(HWND hwnd) : hwnd(hwnd) {
    /* DPI scaling causes GetWindowSize, GetClientRect and other functions return scaled results (i.e. incorrect ones).
     * This function call raises the system requirements to Windows 10 Anniversary Update (2016), unfortunately. */
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    hwindowDC = GetDC(hwnd);
    hwindowCompatibleDC = CreateCompatibleDC(hwindowDC);
    SetStretchBltMode(hwindowCompatibleDC, COLORONCOLOR);

    ensureWindowReadyForCapture();
    RECT windowSize;
    GetClientRect(hwnd, &windowSize);
    width = windowSize.right;
    height = windowSize.bottom;

    // Create a bitmap.
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

    // Use the previously created device context with the bitmap.
    SelectObject(hwindowCompatibleDC, hbwindow);
};


WindowCapture::~WindowCapture() {
    DeleteObject(hbwindow);
    DeleteDC(hwindowCompatibleDC);
    ReleaseDC(hwnd, hwindowDC);
};


void WindowCapture::getScreenshot() {
    if (!ensureWindowReadyForCapture()) {
        image = cv::Mat();  // Set the result to an empty image in case of error.
        return;
    }

    // Ensure the correct size is set.
    image.create(height, width, CV_8UC4);
    // Copy from the window device context to the bitmap device context.
    BitBlt(hwindowCompatibleDC, 0, 0, width, height, hwindowDC, 0, 0, SRCCOPY);
    // Copy from hwindowCompatibleDC to hbwindow.
    GetDIBits(hwindowCompatibleDC, hbwindow, 0, height, image.data, (BITMAPINFO*) &bmpInfo, DIB_RGB_COLORS);
}


void WindowCapture::setMinimumWindowHeight(int value) {
    minimumWindowHeight = value;
}


// Make sure the window isn't minimized; check that the window is large enough and not off-screen.
bool WindowCapture::ensureWindowReadyForCapture() {
    if (isWindowMaximized())
        return true;

    bool redrawWindow = false;

    if (IsIconic(hwnd)) {
        FakeMinimize::prepareMinimizedWindowForCapture(hwnd);
        redrawWindow = true;
    }

    RECT oldWindowPos;
    GetWindowRect(hwnd, &oldWindowPos);
    int width = oldWindowPos.right - oldWindowPos.left;
    int height = oldWindowPos.bottom - oldWindowPos.top;
    RECT windowPos = oldWindowPos;
    // mare sure the window is not off-screen
    HMONITOR hMonitor = MonitorFromRect(&windowPos, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    GetMonitorInfo(hMonitor, &mi);
    RECT rc = mi.rcMonitor;
    // check the size of the window (i.e. stream preview isn't too small)
    if (height < minimumWindowHeight)
        height = minimumWindowHeight;
    height = std::min<int>(height, rc.bottom - rc.top);

    windowPos.left = std::max(rc.left, std::min(rc.right - width, windowPos.left));
    windowPos.top = std::max(rc.top, std::min(rc.bottom - height, windowPos.top));
    windowPos.right = std::min(rc.right, windowPos.left + width);
    windowPos.bottom = std::min(rc.bottom, windowPos.top + height);

    if (oldWindowPos.left != windowPos.left || oldWindowPos.top != windowPos.top
            || oldWindowPos.right != windowPos.right || oldWindowPos.bottom != windowPos.bottom) {
        if (isWindowBeingMoved()) {
            // we shouldn't move a window while the user is moving it, so we're out of luck.
            return false;
        }
        SetWindowPos(hwnd, nullptr, windowPos.left, windowPos.top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
        redrawWindow = true;
    }

    if (redrawWindow) {
        RedrawWindow(hwnd, 0, 0, RDW_INVALIDATE | RDW_UPDATENOW);
    }

    return true;
}


bool WindowCapture::isWindowMaximized() {
    WINDOWPLACEMENT placement;
    placement.length = sizeof(placement);
    GetWindowPlacement(hwnd, &placement);
    return placement.showCmd == SW_MAXIMIZE;
}


bool WindowCapture::isWindowBeingMoved() {
    DWORD threadId = GetWindowThreadProcessId(hwnd, nullptr);
    GUITHREADINFO gui;
    gui.cbSize = sizeof(gui);
    GetGUIThreadInfo(threadId, &gui);
    return gui.flags & GUI_INMOVESIZE;
}


// FakeMinimize (see comments in the header).

void FakeMinimize::prepareMinimizedWindowForCapture(HWND hwnd) {
    getInstance().prepareMinimizedWindowForCaptureImpl(hwnd);
}


FakeMinimize& FakeMinimize::getInstance() {
    static FakeMinimize fakeMinimize;
    return fakeMinimize;
}


FakeMinimize::FakeMinimize() {
    // Starting a thread that will get the event when a window needs to be restored
    windowRestoreHookThread = std::thread(windowRestoreHookProc);
}


void FakeMinimize::windowRestoreHookProc() {
    SetWinEventHook(EVENT_SYSTEM_MINIMIZESTART, EVENT_SYSTEM_MINIMIZEEND, nullptr,
        onWindowFakeRestore, 0, 0, WINEVENT_OUTOFCONTEXT);
    SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr,
        onWindowFakeRestore, 0, 0, WINEVENT_OUTOFCONTEXT);

    // Message loop is needed to receive events from SetWinEventHook.
    MSG msg;
    BOOL bRet;

    while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0) {
        if (bRet == -1) {  // error
            return;
        }
        else {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
}


void CALLBACK FakeMinimize::onWindowFakeRestore(HWINEVENTHOOK hook, DWORD event, HWND hwnd,
        LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime) {
    getInstance().restoreWindow(hwnd);
}


FakeMinimize::~FakeMinimize() {
    if (windowRestoreHookThread.joinable()) {
        HANDLE nativeHandle = windowRestoreHookThread.native_handle();
        DWORD threadId = GetThreadId(nativeHandle);
        PostThreadMessage(threadId, WM_QUIT, 0, 0);
        windowRestoreHookThread.join();
    }
    restoreAllWindows();
}


void FakeMinimize::prepareMinimizedWindowForCaptureImpl(HWND hwnd) {
    // Enable transparency and make the window click-through.
    LONG winLong = GetWindowLong(hwnd, GWL_EXSTYLE);
    SetWindowLong(hwnd, GWL_EXSTYLE, winLong | WS_EX_LAYERED | WS_EX_TRANSPARENT);
    SetLayeredWindowAttributes(hwnd, 0, 1, LWA_ALPHA);

    // Restore the window (it's transparent already).
    // Make sure the window restore animation isn't played.
    bool oldAnimStatus = setMinimizeMaximizeAnimation(hwnd, false);
    ShowWindow(hwnd, SW_RESTORE);
    // Saving the position of the window to show it later at the same position.
    saveWindowPosition(hwnd);
    // Place the window at the top-left corner to make sure it's not off screen.
    SetWindowPos(hwnd, GetDesktopWindow(), 0, 0, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
    setMinimizeMaximizeAnimation(hwnd, oldAnimStatus);
}


bool FakeMinimize::setMinimizeMaximizeAnimation(HWND hwnd, bool enabled) {
    ANIMATIONINFO animationInfo;
    animationInfo.cbSize = sizeof(animationInfo);
    animationInfo.iMinAnimate = enabled;
    SystemParametersInfo(SPI_GETANIMATION, animationInfo.cbSize, &animationInfo, 0);
    bool oldEnabled = animationInfo.iMinAnimate;

    if ((bool) animationInfo.iMinAnimate != enabled) {
        animationInfo.iMinAnimate = enabled;
        SystemParametersInfo(SPI_SETANIMATION, animationInfo.cbSize, &animationInfo, SPIF_SENDCHANGE);
    }
    return oldEnabled;
}


void FakeMinimize::saveWindowPosition(HWND hwnd) {
    RECT position;
    GetWindowRect(hwnd, &position);
    {
        std::lock_guard<std::mutex> guard(originalWindowPositionsMutex);
        originalWindowPositions[hwnd] = {position.left, position.top};
    }
}


void FakeMinimize::restoreWindow(HWND hwnd) {
    POINT originalPosition;
    {
        std::lock_guard<std::mutex> guard(originalWindowPositionsMutex);
        auto findIter = originalWindowPositions.find(hwnd);
        if (findIter == originalWindowPositions.end())  // hwnd is not the OBS window
            return;
        originalPosition = findIter->second;
        originalWindowPositions.erase(findIter);
    }
    restoreWindow(hwnd, originalPosition);
}


void FakeMinimize::restoreWindow(HWND hwnd, POINT originalPosition) {
    LONG winLong = GetWindowLong(hwnd, GWL_EXSTYLE);
    SetWindowLong(hwnd, GWL_EXSTYLE, winLong & (~WS_EX_LAYERED) & (~WS_EX_TRANSPARENT));
    SetWindowPos(hwnd, nullptr, originalPosition.x, originalPosition.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    ShowWindowAsync(hwnd, SW_RESTORE);
}


void FakeMinimize::restoreAllWindows() {
    std::lock_guard<std::mutex> guard(originalWindowPositionsMutex);
    for (const auto& [hwnd, originalPosition] : originalWindowPositions) {
        restoreWindow(hwnd, originalPosition);
    }
}

}  // namespace SonicVisualSplitBase