#pragma once
#define NOMINMAX  // fighting defines from Windows.h
#include <Windows.h>
#include <opencv2/core.hpp>
#include <Unknwn.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <d3d11_4.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <map>
#include <atomic>
#include <functional>
#include <chrono>

namespace winrt {
    using namespace Windows::Foundation;
    using namespace Windows::Graphics;
    using namespace Windows::Graphics::Capture;
    using namespace Windows::Graphics::DirectX;
    using namespace Windows::System;
}

namespace SonicVisualSplitBase {

class FakeMinimize;

// A class for window capturing (OBS in our case)
class WindowCapture {
public:
    WindowCapture(HWND hwnd);

    ~WindowCapture();

    cv::Mat getFrame(std::chrono::milliseconds timeout);

    /* Sets the minimum acceptable width of the window that's being captured.
     * The window will get resized if its height is lower than that value. */
    void setMinimumWindowWidth(int minimumWindowWidth);

    /* Sets the minimum acceptable height of the window that's being captured.
     * The window will get resized if its height is lower than that value. */
    void setMinimumWindowHeight(int minimumWindowHeight);

    static bool isSupported();

private:
    void onFrameArrived(winrt::Direct3D11CaptureFramePool const& sender, winrt::IInspectable const&);

    bool ensureWindowReadyForCapture();

    bool isWindowMaximized();

    bool isWindowBeingMoved();

    void initWinRT();

    cv::Mat lastFrame;
    std::mutex lastFrameMutex;
    std::condition_variable lastFrameArrived;

    HWND hwnd;
    int minimumWindowWidth = 0, minimumWindowHeight = 0;

    winrt::Windows::System::DispatcherQueueController dispatcherQueueController{nullptr};
    winrt::Windows::Graphics::SizeInt32 lastSize{};
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool framePool{nullptr};
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession session{nullptr};
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice direct3DDevice{nullptr};
    winrt::com_ptr<ID3D11DeviceContext> d3dContext{nullptr};
    winrt::com_ptr<ID3D11Device> d3dDevice{nullptr};
    winrt::Direct3D11CaptureFramePool::FrameArrived_revoker frameArrivedRevoker;
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

private:
    static FakeMinimize& getInstance();

    FakeMinimize();

    static void windowRestoreHookProc();

    static void CALLBACK onWindowFakeRestore(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG idObject, LONG idChild,
        DWORD dwEventThread, DWORD dwmsEventTime);

    ~FakeMinimize();

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
