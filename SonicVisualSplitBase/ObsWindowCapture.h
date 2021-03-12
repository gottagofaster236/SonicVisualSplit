#pragma once
#include "GameVideoCapture.h"
#include "WindowCapture.h"
#include <opencv2/core.hpp>
#define NOMINMAX  // fighting defines from Windows.h
#include <Windows.h>


namespace SonicVisualSplitBase {

class ObsWindowCapture : public GameVideoCapture {
public:
    // Returns a full screenshot of the OBS window.
    cv::Mat captureRawFrame() override;

    // Returns the stream preview.
    cv::UMat processFrame(cv::Mat screenshot) override;

    ~ObsWindowCapture();

private:
    bool updateOBSHwnd();

    DWORD getOBSProcessId();

    static BOOL CALLBACK checkIfWindowIsOBS(HWND hwnd, LPARAM pidAndObsHwndPtr);

    WindowCapture* obsCapture = nullptr;
    HWND obsHwnd = nullptr;
    int lastGameFrameWidth, lastGameFrameHeight;
    
    int obsVerticalMargin;  // Difference between the client rect and window rect.
};




}  // namespace SonicVisualSplitBase