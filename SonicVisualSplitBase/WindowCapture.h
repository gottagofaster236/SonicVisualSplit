#pragma once
#include <Windows.h>
#include <opencv2/opencv.hpp>
#include <mutex>

namespace SonicVisualSplitBase {

class FakeMinimize;

class WindowCapture {
public:
    WindowCapture(HWND hwindow);

    ~WindowCapture();

    void getScreenshot();

    cv::Mat image;
    int width, height;

private:
    void ensureWindowReadyForCapture();

    HWND hwnd;
    HDC hwindowDC, hwindowCompatibleDC;
    HBITMAP hbwindow;
    BITMAPINFOHEADER bmpInfo;
    FakeMinimize* fakeMinimize = nullptr;
};

class FakeMinimize {
public:
    FakeMinimize(HWND hwnd);

    ~FakeMinimize();

private:
    void addRestoreHook();

    static DWORD WINAPI addRestoreHookProc(LPVOID lpParameter);

    static void CALLBACK onForegroundWindowChanged(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG idObject, LONG idChild,
                                                   DWORD dwEventThread, DWORD dwmsEventTime);

    static void restoreWindow(std::pair<HWND, POINT> windowAndOriginalPosition);

    static void restoreRemainingWindows();

    static void registerAtExit();

    HWND hwnd;
    HANDLE messageLoopThread = nullptr;
    inline static std::map<HWND, POINT> originalWindowPositions;
    inline static std::mutex originalWindowPositionsMutex;
    inline static std::mutex threadTerminationMutex;
};

}  // namespace SonicVisualSplitBase