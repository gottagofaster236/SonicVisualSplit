#include "WindowCapture.h"
#include "capture.interop.h"
#include "direct3d11.interop.h"
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.h>
#include <dwmapi.h>
#include <DispatcherQueue.h>
#include <cassert>

namespace SonicVisualSplitBase {

// Based on https://github.com/obsproject/obs-studio/blob/master/libobs-winrt/winrt-capture.cpp
// and https://github.com/obsproject/obs-studio/blame/master/libobs-winrt/winrt-dispatch.cpp
// OBS Project is licensed under GPLv2 or later version.

static const winrt::Windows::Graphics::DirectX::DirectXPixelFormat PIXEL_FORMAT = winrt::DirectXPixelFormat::B8G8R8A8UIntNormalized;


WindowCapture::WindowCapture(HWND hwnd) : hwnd(hwnd) {
    DispatcherQueueOptions options{sizeof(DispatcherQueueOptions), DQTYPE_THREAD_DEDICATED, DQTAT_COM_STA};
    CreateDispatcherQueueController(options, reinterpret_cast<
        ABI::Windows::System::IDispatcherQueueController**>(
            winrt::put_abi(dispatcherQueueController)));
    dispatcherQueueController.CreateOnDedicatedThread();

    bool enqueueResult = dispatcherQueueController.DispatcherQueue().TryEnqueue([=, this]() {
        /* DPI scaling causes GetWindowSize, GetClientRect and other functions return scaled results (i.e. incorrect ones).
        * This function call requires Windows 10 Anniversary Update (2016). */
        SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

        initWinRT();
        d3dDevice = CreateD3DDevice();
        d3dDevice->GetImmediateContext(d3dContext.put());
        winrt::com_ptr<IDXGIDevice> dxgiDevice = d3dDevice.as<IDXGIDevice>();
        direct3DDevice = CreateDirect3DDevice(dxgiDevice.get());

        winrt::GraphicsCaptureItem item = CreateCaptureItemForWindow(hwnd);
        lastSize = item.Size();
        framePool = winrt::Direct3D11CaptureFramePool::Create(direct3DDevice, PIXEL_FORMAT, 2, lastSize);
        session = framePool.CreateCaptureSession(item);
        session.IsBorderRequired(false);
        session.IsCursorCaptureEnabled(false);
        frameArrivedRevoker = framePool.FrameArrived(winrt::auto_revoke, {this, &WindowCapture::onFrameArrived});
        session.StartCapture();
    });
    assert(enqueueResult);
};


WindowCapture::~WindowCapture() {
    bool enqueueResult = dispatcherQueueController.DispatcherQueue().TryEnqueue(winrt::DispatcherQueuePriority::High, [=, this]() {
        frameArrivedRevoker.revoke();
        framePool.Close();
        session.Close();
    });
    assert(enqueueResult);
    std::thread([=, this]() {
        /* Call get() to wait until the completion.
         * Since there's an assert in WinRT that disallows calling get() on the main thread,
         * we are forced to do it on another background thread.
         */
        dispatcherQueueController.ShutdownQueueAsync().get();
    }).join();
}


cv::Mat WindowCapture::getFrame(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(lastFrameMutex);
    lastFrameArrived.wait_for(lock, timeout, [=, this]() {
        return !lastFrame.empty();
    });
    cv::Mat result = lastFrame;
    lastFrame = {};
    return result;
}


void WindowCapture::setMinimumWindowWidth(int value) {
    minimumWindowWidth = value;
}


void WindowCapture::setMinimumWindowHeight(int value) {
    minimumWindowHeight = value;
}


bool WindowCapture::isSupported() {
    return winrt::GraphicsCaptureSession::IsSupported();
}


static bool getClientBox(HWND window, uint32_t width, uint32_t height, D3D11_BOX* clientBox) {
    RECT clientRect{}, windowRect{};
    POINT upperLeft{};

    /* check iconic (minimized) twice, ABA is very unlikely */
    bool clientBoxAvailable =
        !IsIconic(window) && GetClientRect(window, &clientRect) &&
        !IsIconic(window) && (clientRect.right > 0) &&
        (clientRect.bottom > 0) &&
        (DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS,
            &windowRect,
            sizeof(windowRect)) == S_OK) &&
        ClientToScreen(window, &upperLeft);
    if (clientBoxAvailable) {
        const uint32_t left =
            (upperLeft.x > windowRect.left)
            ? (upperLeft.x - windowRect.left)
            : 0;
        clientBox->left = left;

        const uint32_t top = (upperLeft.y > windowRect.top)
            ? (upperLeft.y - windowRect.top)
            : 0;
        clientBox->top = top;

        uint32_t textureWidth = 1;
        if (width > left) {
            textureWidth =
                std::min(width - left, (uint32_t)clientRect.right);
        }

        uint32_t textureHeight = 1;
        if (height > top) {
            textureHeight =
                std::min(height - top, (uint32_t)clientRect.bottom);
        }

        clientBox->right = left + textureWidth;
        clientBox->bottom = top + textureHeight;

        clientBox->front = 0;
        clientBox->back = 1;

        clientBoxAvailable = (clientBox->right <= width) &&
            (clientBox->bottom <= height);
    }

    return clientBoxAvailable;
}


void WindowCapture::onFrameArrived(winrt::Direct3D11CaptureFramePool const& sender, winrt::IInspectable const&) {
    // Based on https://github.com/obsproject/obs-studio/blob/master/libobs-winrt/winrt-capture.cpp
    // and https://stackoverflow.com/a/72568741/6120487
    bool newSize = false;
    winrt::Direct3D11CaptureFrame frame = sender.TryGetNextFrame();
    winrt::SizeInt32 frameContentSize = frame.ContentSize();
    winrt::com_ptr<ID3D11Texture2D> frameSurface = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());
    
    /* need GetDesc because ContentSize is not reliable */
    D3D11_TEXTURE2D_DESC desc;
    frameSurface->GetDesc(&desc);

    D3D11_BOX clientBox;
    if (ensureWindowReadyForCapture() && getClientBox(hwnd, desc.Width, desc.Height, &clientBox)) {
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc = {1, 0};
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags = 0;
        desc.Width = clientBox.right - clientBox.left;
        desc.Height = clientBox.bottom - clientBox.top;
        winrt::com_ptr<ID3D11Texture2D> stagingTexture;
        d3dDevice.get()->CreateTexture2D(&desc, nullptr, stagingTexture.put());
        d3dContext->CopySubresourceRegion(stagingTexture.get(),
            0, 0, 0, 0, frameSurface.get(), 0, &clientBox);
        D3D11_MAPPED_SUBRESOURCE mappedTex;
        d3dContext->Map(stagingTexture.get(), 0, D3D11_MAP_READ, 0, &mappedTex);
        {
            std::lock_guard<std::mutex> guard(lastFrameMutex);
            lastFrame = cv::Mat(desc.Height, desc.Width, CV_8UC4, mappedTex.pData, mappedTex.RowPitch).clone();
        }
        lastFrameArrived.notify_one();
        d3dContext->Unmap(stagingTexture.get(), 0);
    }

    if (frameContentSize.Width != lastSize.Width || frameContentSize.Height != lastSize.Height) {
        framePool.Recreate(direct3DDevice, PIXEL_FORMAT, 2, frameContentSize);
        lastSize = frameContentSize;
    }
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
    // Make sure the window is not off-screen.
    HMONITOR hMonitor = MonitorFromRect(&windowPos, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    GetMonitorInfo(hMonitor, &mi);
    RECT rc = mi.rcMonitor;
    // Check the size of the window (stream preview shouldn't bee too small).
    width = std::max(width, minimumWindowWidth);
    height = std::max(height, minimumWindowHeight);
    width = std::min<int>(width, rc.right - rc.left);
    height = std::min<int>(height, rc.bottom - rc.top);

    windowPos.left = std::max(rc.left, std::min(rc.right - width, windowPos.left));
    windowPos.top = std::max(rc.top, std::min(rc.bottom - height, windowPos.top));
    windowPos.right = std::min(rc.right, windowPos.left + width);
    windowPos.bottom = std::min(rc.bottom, windowPos.top + height);

    if (oldWindowPos.left != windowPos.left || oldWindowPos.top != windowPos.top
            || oldWindowPos.right != windowPos.right || oldWindowPos.bottom != windowPos.bottom) {
        if (isWindowBeingMoved()) {
            // We shouldn't move a window while the user is moving it, so we're out of luck.
            return false;
        }
        SetWindowPos(hwnd, nullptr, windowPos.left, windowPos.top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
        redrawWindow = true;
        return false;
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


class WinRtInitializer {
public:
    WinRtInitializer() {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
    }

    ~WinRtInitializer() {
        winrt::uninit_apartment();
    }
};


void WindowCapture::initWinRT() {
    thread_local WinRtInitializer initializer;
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
    // Start a thread that will get the event when a window needs to be restored.
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
    // Save the position of the window to show it later at the same position.
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
        if (findIter == originalWindowPositions.end())  // hwnd is not the OBS window.
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
