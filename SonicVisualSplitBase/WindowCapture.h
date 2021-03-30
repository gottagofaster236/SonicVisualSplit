#pragma once
#define NOMINMAX  // fighting defines from Windows.h
#include <Windows.h>
#include <opencv2/core.hpp>
#include <thread>
#include <mutex>
#include <map>


namespace SonicVisualSplitBase {

class FakeMinimize;

// A class for window capturing (OBS in our case)
class WindowCapture {
public:
    WindowCapture(HWND hwnd);

    ~WindowCapture();

    // Capture the window and write the result to the image field.
    void getScreenshot();

    /* Sets the minimum acceptable height of the window that's being captured.
     * The window will get resized if its height is lower than that value. */
    void setMinimumWindowHeight(int minimumWindowHeight);

    cv::Mat image;
    int width, height;

private:
    bool ensureWindowReadyForCapture();

    bool isWindowMaximized();

    bool isWindowBeingMoved();

    HWND hwnd;
    int minimumWindowHeight = 0;
    DWORD processId;
    HDC hwindowDC, hwindowCompatibleDC;
    HBITMAP hbwindow;
    BITMAPINFOHEADER bmpInfo;
};


/* FakeMinimize:
 * It's impossible to make a screenshot of a minimized window (because of WinAPI limitations).
 * So we are actually restoring a window, but making it transparent and click-through.
 * The user doesn't see the window, so it's the same as if it's still minimized.
 * When the user tries to "restore" the window (it's restored already), we intercept it with a hook
 * and return the window to its original state. */

class FakeMinimize {
public:
    static void prepareMinimizedWindowForCapture(HWND hwnd);

    ~FakeMinimize();

private:
    static FakeMinimize& getInstance();

    FakeMinimize();

    static void windowRestoreHookProc();

    static void CALLBACK onWindowFakeRestore(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG idObject, LONG idChild,
        DWORD dwEventThread, DWORD dwmsEventTime);

    // Restore the window, but make it transparent and click-through.
    void prepareMinimizedWindowForCaptureImpl(HWND hwnd);

    // Turns the window minimize/maximize animation on or off. Returns the old animation status.
    bool setMinimizeMaximizeAnimation(HWND hwnd, bool enabled);

    void saveWindowPosition(HWND hwnd);

    /* Return the window to the original position and remove transparency (as if the user restored the window).
     * Does nothing if it's not the window we made transparent. */
    void restoreWindow(HWND hwnd);

    // Return the window to the original position and remove transparency.
    void restoreWindow(HWND hwnd, POINT originalPosition);

    // Restore all the windows that we've made transparent and click-through.
    void restoreAllWindows();

    std::thread windowRestoreHookThread;
    std::map<HWND, POINT> originalWindowPositions;
    std::mutex originalWindowPositionsMutex;
};

}  // namespace SonicVisualSplitBase