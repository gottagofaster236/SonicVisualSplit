#pragma once
#include <Windows.h>
#include <opencv2/core.hpp>
#include <mutex>
#include <map>

namespace SonicVisualSplitBase {

class FakeMinimize;

// A class for window capturing (OBS in our case)
class WindowCapture {
public:
    WindowCapture(HWND hwnd);

    ~WindowCapture();

    // Capture the window and write the result to the <code>image</code> field.
    void getScreenshot();

    cv::Mat image;
    int width, height;

private:
    bool ensureWindowReadyForCapture();

    bool isWindowMaximized();

    bool isWindowBeingMoved();

    HWND hwnd;
    DWORD processId;
    HDC hwindowDC, hwindowCompatibleDC;
    HBITMAP hbwindow;
    BITMAPINFOHEADER bmpInfo;
    FakeMinimize* fakeMinimize = nullptr;
};


// A helper class for FakeMinimize, to restore the window to the same position.
class OriginalWindowPositions {
public:
    void saveWindowPosition(HWND hwnd);

    // Returns false if hwnd is not an OBS window.
    bool restoreOBSWindow(HWND hwnd);

    ~OriginalWindowPositions();

private:
    // Return the window to the original position and remove transparency (as if the user restored the window).
    void restoreWindow(HWND hwnd, POINT originalPosition);

    std::map<HWND, POINT> originalWindowPositions;
    std::mutex originalWindowPositionsMutex;
};


/* FakeMinimize:
 * It's impossible to make a screenshot of a minimized window (because of WinAPI limitations).
 * So we are actually restoring a window, but making it transparent and click-through.
 * The user doesn't see the window, so it's the same as if it's still minimized.
 * When the user tries to "restore" the window (it's restored already), we intercept it with a hook
 * and return the window to its original state. */

class FakeMinimize {
public:
    FakeMinimize(HWND hwnd);

    ~FakeMinimize();

private:
    bool setMinimizeMaximizeAnimation(bool enabled);

    void addRestoreHook();

    static DWORD WINAPI addRestoreHookProc(LPVOID lpParameter);

    static void CALLBACK onWindowFakeRestore(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG idObject, LONG idChild,
                                             DWORD dwEventThread, DWORD dwmsEventTime);
    HWND hwnd;
    HANDLE messageLoopThread = nullptr;
    inline static OriginalWindowPositions originalWindowPositions;
    inline static std::mutex threadTerminationMutex;
};

}  // namespace SonicVisualSplitBase