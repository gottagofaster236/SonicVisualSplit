#include "ObsWindowCapture.h"
#include "WindowCapture.h"
#include "DigitsRecognizer.h"
#include <opencv2/imgproc.hpp>
#define NOMINMAX  // fighting defines from Windows.h
#include <Windows.h>
#include <Tlhelp32.h>
#include <functional>
#include <opencv2/imgcodecs.hpp>


namespace SonicVisualSplitBase {

static const int MINIMUM_STREAM_PREVIEW_HEIGHT = 535;


cv::Mat ObsWindowCapture::captureRawFrameImpl() {
    cv::Mat screenshot;
    if (!updateOBSHwnd())
        return screenshot;  // return an empty image in case of error
    {
        std::lock_guard<std::mutex> guard(obsCaptureMutex);
        obsCapture->getScreenshot();
        if (obsCapture->image.empty())
            return screenshot;
        cv::cvtColor(obsCapture->image, screenshot, cv::COLOR_BGRA2BGR);
    }
    return screenshot;
}


cv::UMat ObsWindowCapture::processFrame(cv::Mat screenshot) {
    /* In OBS, the stream preview has a light-gray border around it.
     * It is adjacent to the sides of the screen. We start from the right side. */
    cv::UMat streamPreview;
    if (screenshot.empty())
        return streamPreview;  // Return an empty image in case of error
    // Find the border rectangle.
    int borderRight = screenshot.cols - 1;
    cv::Vec3b borderColor = screenshot.at<cv::Vec3b>(screenshot.rows / 2, borderRight);
    int borderTop = screenshot.rows / 2;
    while (borderTop - 1 >= 0 && screenshot.at<cv::Vec3b>(borderTop - 1, borderRight) == borderColor)
        borderTop--;
    int borderBottom = screenshot.rows / 2;
    while (borderBottom + 1 < screenshot.rows && screenshot.at<cv::Vec3b>(borderBottom + 1, borderRight) == borderColor)
        borderBottom++;
    int borderLeft = borderRight;
    while (borderLeft - 1 >= 0 && screenshot.at<cv::Vec3b>(borderTop, borderLeft - 1) == borderColor)
        borderLeft--;

    // Find the stream preview rectangle
    int streamLeft = borderLeft, streamTop = borderTop, streamRight = borderRight, streamBottom = borderBottom;
    while (streamLeft < screenshot.cols && screenshot.at<cv::Vec3b>((borderTop + borderBottom) / 2, streamLeft) == borderColor)
        streamLeft++;
    while (streamTop < screenshot.rows && screenshot.at<cv::Vec3b>(streamTop, (borderLeft + borderRight) / 2) == borderColor)
        streamTop++;
    while (streamRight > 0 && screenshot.at<cv::Vec3b>((borderTop + borderBottom) / 2, streamRight - 1) == borderColor)
        streamRight--;
    while (streamBottom > 0 && screenshot.at<cv::Vec3b>(streamBottom - 1, (borderLeft + borderRight) / 2) == borderColor)
        streamBottom--;

    cv::Rect streamRectangle(streamLeft, streamTop, streamRight - streamLeft, streamBottom - streamTop);
    if (streamRectangle.empty()) {
        return cv::UMat();
    }
    screenshot(streamRectangle).copyTo(streamPreview);
    if (lastGameFrameWidth != streamPreview.cols || lastGameFrameHeight != streamPreview.rows) {
        DigitsRecognizer::resetDigitsPlacementAsync();
        lastGameFrameWidth = streamPreview.cols;
        lastGameFrameHeight = streamPreview.rows;
        int minimumObsHeight = screenshot.rows + obsVerticalMargin + (MINIMUM_STREAM_PREVIEW_HEIGHT - lastGameFrameHeight);
        {
            std::lock_guard<std::mutex> guard(obsCaptureMutex);
            if (obsCapture)
                obsCapture->setMinimumWindowHeight(minimumObsHeight);
        }
    }
    return streamPreview;
}


bool ObsWindowCapture::updateOBSHwnd() {
    if (IsWindow(obsHwnd) && !IsIconic(obsHwnd)) {
        // Checking that the size of the window hasn't changed.
        RECT windowSize;
        GetClientRect(obsHwnd, &windowSize);

        int width = windowSize.right;
        int height = windowSize.bottom;
        
        RECT windowRect;
        GetWindowRect(obsHwnd, &windowRect);
        int windowHeight = windowRect.bottom - windowRect.top;
        obsVerticalMargin = windowHeight - height;

        {
            std::lock_guard<std::mutex> guard(obsCaptureMutex);
            if (obsCapture && width == obsCapture->width && height == obsCapture->height)
                return true;
        }
    }
    // reset the scale for the DigitRecognizer, as the video stream changed
    DigitsRecognizer::resetDigitsPlacementAsync();

    obsHwnd = nullptr;
    DWORD processId = getOBSProcessId();
    if (processId == 0)
        return false;
    std::tuple<DWORD&, HWND&> pidAndObsHwnd = std::tie(processId, obsHwnd);
    EnumWindows(checkIfWindowIsOBS, reinterpret_cast<LPARAM>(&pidAndObsHwnd));
    if (!obsHwnd)
        return false;
    {
        std::lock_guard<std::mutex> guard(obsCaptureMutex);
        obsCapture = std::make_unique<WindowCapture>(obsHwnd);
        obsCapture->setMinimumWindowHeight(720);  // Default value.
    }
    return true;
}


DWORD ObsWindowCapture::getOBSProcessId() {
    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
    Process32First(snapshot, &entry);
    do {
        if (lstrcmp(entry.szExeFile, TEXT("obs64.exe")) == 0 || lstrcmp(entry.szExeFile, TEXT("obs32.exe")) == 0) {
            CloseHandle(snapshot);
            return entry.th32ProcessID;
        }
    } while (Process32Next(snapshot, &entry));

    CloseHandle(snapshot);
    return 0;
}


BOOL CALLBACK ObsWindowCapture::checkIfWindowIsOBS(HWND hwnd, LPARAM pidAndObsHwndPtr) {
    auto& [processId, obsHwnd] = *reinterpret_cast<std::tuple<DWORD&, HWND&>*>(pidAndObsHwndPtr);

    int textLength = GetWindowTextLength(hwnd);
    if (textLength == 0) {
        return TRUE;
    }

    DWORD windowProcessId;
    GetWindowThreadProcessId(hwnd, &windowProcessId);
    if (windowProcessId == processId) {
        // Checking the window title. The main window's title starts with "OBS".
        TCHAR* titleBuffer = new TCHAR[textLength + 1];
        GetWindowText(hwnd, titleBuffer, textLength + 1);
        if (wcsncmp(titleBuffer, TEXT("OBS"), 3) == 0) {
            obsHwnd = hwnd;
            return FALSE;
        }
    }
    return TRUE;
}

}  // namespace SonicVisualSplitBase