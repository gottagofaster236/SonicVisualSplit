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

private:
    // Returns a full screenshot of the OBS window.
    cv::Mat captureRawFrameImpl() override;

    bool updateOBSHwnd();

    DWORD getOBSProcessId();

    static BOOL CALLBACK checkIfWindowIsOBS(HWND hwnd, LPARAM pidAndObsHwndPtr);

    std::unique_ptr<WindowCapture> obsCapture;
    std::mutex obsCaptureMutex;
    HWND obsHwnd = nullptr;
    int lastGameFrameWidth, lastGameFrameHeight;
    
    int obsVerticalMargin;  // Difference between the client rect and window rect.
};

}  // namespace SonicVisualSplitBase