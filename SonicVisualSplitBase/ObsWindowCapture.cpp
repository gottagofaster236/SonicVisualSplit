#include "ObsWindowCapture.h"
#include "WindowCapture.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#define NOMINMAX  // fighting defines from Windows.h
#include <Windows.h>
#include <Tlhelp32.h>
#include <functional>


namespace SonicVisualSplitBase {

static const int MINIMUM_STREAM_PREVIEW_HEIGHT = 480;
static const int MINIMUM_STREAM_PREVIEW_WIDTH = MINIMUM_STREAM_PREVIEW_HEIGHT * 18 / 9;
static const int OBS_MINIMUM_HEIGHT_DEFAULT = 680;


ObsWindowCapture::ObsWindowCapture() : isSupported(WindowCapture::isSupported()) {}


cv::UMat ObsWindowCapture::captureFrameImpl() {
    if (!isSupported)
        return {};
    if (!updateObsHwnd())
        return {};
    cv::Mat screenshotRgba;
    {
        std::lock_guard<std::mutex> guard(obsCaptureMutex);
        auto timeout = std::chrono::milliseconds(100);
        screenshotRgba = obsCapture->getFrame(timeout);
    }
    if (screenshotRgba.empty())
        return {};
    cv::Mat screenshot;
    cv::cvtColor(screenshotRgba, screenshot, cv::COLOR_BGRA2BGR);

    /* In OBS, the stream preview has a light-gray border around it.
     * It is adjacent to the sides of the screen. */
    int verticalCheckPosition = screenshot.rows / 5;
    int borderLeft = 0;
    cv::Vec3b borderColor = screenshot.at<cv::Vec3b>(verticalCheckPosition, borderLeft);
    cv::Vec3b blackMenuColor = screenshot.at<cv::Vec3b>(0, 0);
    if (borderColor == blackMenuColor) {
        // On newer OBS versions, the gray border of the stream preview has a black margin to the sides of the window.
        while (borderLeft + 1 < screenshot.cols && screenshot.at<cv::Vec3b>(verticalCheckPosition, borderLeft) == borderColor)
            borderLeft++;
        if (borderLeft + 1 == screenshot.cols)
            return {};
        borderColor = screenshot.at<cv::Vec3b>(verticalCheckPosition, borderLeft);
    }

    int borderTop = verticalCheckPosition;
    while (borderTop - 1 >= 0 && screenshot.at<cv::Vec3b>(borderTop - 1, borderLeft) == borderColor)
        borderTop--;
    int borderBottom = verticalCheckPosition;
    while (borderBottom + 1 < screenshot.rows && screenshot.at<cv::Vec3b>(borderBottom + 1, borderLeft) == borderColor)
        borderBottom++;
    int borderRight = borderLeft;
    while (borderRight + 1 < screenshot.cols && screenshot.at<cv::Vec3b>(borderTop, borderRight + 1) == borderColor)
        borderRight++;

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

    if (abs((streamLeft + streamRight) / 2 - screenshot.cols / 2) > screenshot.cols / 20) {
        // The stream preview isn't centered. That's probably because OBS hasn't finished drawing.
        return {};
    }

    cv::Rect streamRectangle(streamLeft, streamTop, streamRight - streamLeft, streamBottom - streamTop);
    if (streamRectangle.empty()) {
        return {};
    }
    cv::UMat streamPreview = screenshot(streamRectangle).getUMat(cv::ACCESS_READ);
    if (lastScreenshotHeight != screenshot.rows || lastGameFrameHeight != streamPreview.rows) {
        lastScreenshotHeight = screenshot.rows;
        lastGameFrameHeight = streamPreview.rows;
        int minimumObsHeight;
        int streamPreviewHeightDelta = MINIMUM_STREAM_PREVIEW_HEIGHT - lastGameFrameHeight;
        if (streamPreviewHeightDelta > 0)
            minimumObsHeight = lastScreenshotHeight + obsVerticalMargin + streamPreviewHeightDelta;
        else
            minimumObsHeight = OBS_MINIMUM_HEIGHT_DEFAULT;
        {
            std::lock_guard<std::mutex> guard(obsCaptureMutex);
            if (obsCapture)
                obsCapture->setMinimumWindowHeight(minimumObsHeight);
        }
    }
    return streamPreview;
}


std::chrono::milliseconds ObsWindowCapture::getDelayAfterLastFrame() {
    // Get frames at a rate WindowCapture provides them.
    return std::chrono::milliseconds(0);
}


std::string ObsWindowCapture::getVideoDisconnectedReason() {
    if (!isSupported)
        return "\nUpdate to a newer Windows version to use OBS Window Capture.";

    bool obsCapturePresent;
    {
        std::lock_guard<std::mutex> guard(obsCaptureMutex);
        obsCapturePresent = obsCapture != nullptr;
    }

    if (obsCapturePresent)
        return "1) Remove selection from any OBS sources (it obstructs the preview);\n"
            "2) resize the OBS window to a bigger size.";
    else
        return "OBS window not found.";
}


bool ObsWindowCapture::updateObsHwnd() {
    if (IsWindow(obsHwnd)) {
        if (!IsIconic(obsHwnd)) {
            // Checking that the size of the window hasn't changed.
            RECT windowSize;
            GetClientRect(obsHwnd, &windowSize);

            int width = windowSize.right;
            int height = windowSize.bottom;

            RECT windowRect;
            GetWindowRect(obsHwnd, &windowRect);
            int windowHeight = windowRect.bottom - windowRect.top;
            obsVerticalMargin = windowHeight - height;
        }

        std::lock_guard<std::mutex> guard(obsCaptureMutex);
        if (obsCapture)
            return true;
    }

    obsHwnd = nullptr;
    DWORD processId = getObsProcessId();
    if (processId == 0)
        return false;
    std::tuple<DWORD&, HWND&> pidAndObsHwnd = std::tie(processId, obsHwnd);
    EnumWindows(checkIfWindowIsObs, reinterpret_cast<LPARAM>(&pidAndObsHwnd));
    std::lock_guard<std::mutex> guard(obsCaptureMutex);
    if (!obsHwnd) {
        obsCapture = nullptr;
        return false;
    }
    obsCapture = std::make_unique<WindowCapture>(obsHwnd);
    obsCapture->setMinimumWindowWidth(MINIMUM_STREAM_PREVIEW_WIDTH);
    obsCapture->setMinimumWindowHeight(OBS_MINIMUM_HEIGHT_DEFAULT);
    return true;
}


DWORD ObsWindowCapture::getObsProcessId() {
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

BOOL CALLBACK ObsWindowCapture::checkIfWindowIsObs(HWND hwnd, LPARAM pidAndObsHwndPtr) {
    auto& [obsProcessId, obsHwnd] =
        *reinterpret_cast<std::tuple<DWORD&, HWND&>*>(pidAndObsHwndPtr);

    DWORD windowProcessId;
    GetWindowThreadProcessId(hwnd, &windowProcessId);
    if (windowProcessId != obsProcessId) {
        return TRUE;
    }

    // Checking the window title. The main window's title starts with "OBS".
    const int BUFFER_SIZE = 4;
    std::vector<TCHAR> titleBuffer(BUFFER_SIZE);
    GetWindowText(hwnd, titleBuffer.data(), BUFFER_SIZE);
    if (wcsncmp(titleBuffer.data(), TEXT("OBS"), 3) == 0) {
        obsHwnd = hwnd;
        return FALSE;
    }
    return TRUE;
}

}  // namespace SonicVisualSplitBase
