#pragma once
#include "GameVideoCapture.h"
#include "WindowCapture.h"
#include <opencv2/core.hpp>
#define NOMINMAX  // fighting defines from Windows.h
#include <Windows.h>
#include <memory>


namespace SonicVisualSplitBase {

/* A class that captures the stream preview from the currently opened OBS window.
 * See README.md for more information. */
class ObsWindowCapture : public GameVideoCapture {
public:
    // Returns the stream preview.
    cv::UMat processFrame(cv::Mat screenshot) override;

    std::chrono::milliseconds getDelayAfterLastFrame() override;

    static bool isSupported();

private:
    // Returns a screenshot of the OBS window client area.
    cv::Mat captureRawFrameImpl() override;

    bool updateOBSHwnd();

    DWORD getOBSProcessId();

    static BOOL CALLBACK checkIfWindowIsOBS(HWND hwnd, LPARAM pidAndObsHwndPtr);

    std::unique_ptr<WindowCapture> obsCapture;
    std::mutex obsCaptureMutex;
    HWND obsHwnd = nullptr;
    int lastScreenshotHeight = 0, lastGameFrameHeight = 0;
    
    int obsVerticalMargin = 0;  // Difference between the client rect and window rect.
};

}  // namespace SonicVisualSplitBase
