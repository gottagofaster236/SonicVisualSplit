#include "GameCapture.h"
#include "WindowCapture.h"
#include "DigitsRecognizer.h"
#include <opencv2/imgproc.hpp>
#include <Windows.h>
#include <Tlhelp32.h>
#include <functional>


namespace SonicVisualSplitBase {
namespace GameCapture {

WindowCapture* obsCapture = nullptr;
HWND obsHwnd = nullptr;
int lastGameFrameWidth, lastGameFrameHeight;

int obsVerticalMargin;  // difference between the client rect and window rect
const int MINIMUM_STREAM_PREVIEW_HEIGHT = 620;
int minimumObsHeight = 720;  // the minimum acceptable height of the OBS window (initialized with 720 when not calculated)

bool updateOBSHwnd();
DWORD getOBSProcessId();
BOOL CALLBACK checkIfWindowIsOBS(HWND hwnd, LPARAM lparam);


cv::Mat getObsScreenshot() {
    cv::Mat screenshot;
    if (!updateOBSHwnd())
        return screenshot;  // return an empty image in case of error
    obsCapture->getScreenshot();
    if (obsCapture->image.empty())
        return screenshot;
    cv::cvtColor(obsCapture->image, screenshot, cv::COLOR_BGRA2BGR);
    return screenshot;
}


cv::UMat getGameFrameFromObsScreenshot(cv::Mat screenshot) {
    /* In OBS, the stream preview has a light-gray border around it.
     * It is adjacent to the sides of the screen. We start from the right side. */
    cv::UMat streamPreview;
    if (screenshot.empty())
        return streamPreview;  // return an empty image in case of error
    
    // Find the border rectangle
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
        minimumObsHeight = screenshot.rows + obsVerticalMargin + (MINIMUM_STREAM_PREVIEW_HEIGHT - lastGameFrameHeight);
    }
    return streamPreview;
}


int getMinimumObsHeight() {
    return minimumObsHeight;
}


bool updateOBSHwnd() {
    if (IsWindow(obsHwnd) && !IsIconic(obsHwnd)) {
        // checking that the size of the window hasn't changed
        RECT windowSize;
        GetClientRect(obsHwnd, &windowSize);

        int width = windowSize.right;
        int height = windowSize.bottom;
        
        RECT windowRect;
        GetWindowRect(obsHwnd, &windowRect);
        int windowHeight = windowRect.bottom - windowRect.top;
        obsVerticalMargin = windowHeight - height;

        if (width == obsCapture->width && height == obsCapture->height)
            return true;
    }
    // reset the scale for the DigitRecognizer, as the video stream changed
    DigitsRecognizer::resetDigitsPlacementAsync();

    obsHwnd = nullptr;
    DWORD processId = getOBSProcessId();
    if (processId == 0)
        return false;
    EnumWindows(checkIfWindowIsOBS, processId);
    if (!obsHwnd)
        return false;
    delete obsCapture;
    obsCapture = new WindowCapture(obsHwnd);
    return true;
}


DWORD getOBSProcessId() {
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


BOOL CALLBACK checkIfWindowIsOBS(HWND hwnd, LPARAM lparam) {
    if (GetWindowTextLength(hwnd) == 0) {
        return TRUE;
    }

    DWORD processId;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId == lparam) {
        // Checking the window title. The main window's title starts with "OBS".
        int textLen = GetWindowTextLength(hwnd);
        TCHAR* titleBuffer = new TCHAR[textLen + 1];
        GetWindowText(hwnd, titleBuffer, textLen + 1);
        if (wcsncmp(titleBuffer, TEXT("OBS"), 3) == 0) {
            obsHwnd = hwnd;
            return FALSE;
        }
    }
    return TRUE;
}

}  // namespace GameCapture
}  // namespace SonicVisualSplitBase


/* Debug code for game capture.
#include "FrameStorage.h"

int main() {
    SonicVisualSplitBase::FrameStorage::startSavingFrames();
    system("pause");
    SonicVisualSplitBase::FrameStorage::stopSavingFrames();
}*/