#include "GameCapture.h"
#include "WindowCapture.h"
#include "DigitsRecognizer.h"
#include <Windows.h>
#include <Tlhelp32.h>
#include <iostream>
#include <functional>


namespace SonicVisualSplitBase {

WindowCapture* obsCapture = nullptr;
HWND obsHwnd = nullptr;

bool updateOBSHwnd();
DWORD getOBSProcessId();
BOOL CALLBACK checkIfWindowIsOBS(HWND hwnd, LPARAM lparam);
int lastGameFrameWidth, lastGameFrameHeight;

// Gets the stream preview from the opened OBS window
// Warning: bad code ;)
cv::UMat getGameFrame() {
    cv::UMat streamPreview;
    if (!updateOBSHwnd()) {
        return streamPreview;  // return an empty image in case of error
    }
    obsCapture->getScreenshot();
    if (obsCapture->image.rows == 0 || obsCapture->image.cols == 0)
        return streamPreview;
    cv::Mat screenshot;
    cv::cvtColor(obsCapture->image, screenshot, cv::COLOR_BGRA2BGR);

    // in OBS, the stream preview has a light-gray border around it.
    // It is adjacent to the sides of the screen, so we start from the right side. (Left side may not work if we took the screenshot during OBS redraw).

    // Find the light-gray border rectangle
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
        DigitsRecognizer::resetDigitsPlacement();
        lastGameFrameWidth = streamPreview.cols;
        lastGameFrameHeight = streamPreview.rows;
    }
    return streamPreview;
}

bool updateOBSHwnd() {
    if (IsWindow(obsHwnd) && !IsIconic(obsHwnd)) {
        // checking that the size of the window hasn't changed
        RECT windowSize;
        GetClientRect(obsHwnd, &windowSize);

        int width = windowSize.right;
        int height = windowSize.bottom;
        if (width == obsCapture->width && height == obsCapture->height)
            return true;
    }
    // reset the scale for the DigitRecognizer, as the video stream changed
    DigitsRecognizer::resetDigitsPlacement();

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

    if (Process32First(snapshot, &entry) == TRUE) {
        while (Process32Next(snapshot, &entry) == TRUE) {
            if (lstrcmp(entry.szExeFile, TEXT("obs64.exe")) == 0 || lstrcmp(entry.szExeFile, TEXT("obs32.exe")) == 0) {
                return entry.th32ProcessID;
            }
        }
    }

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

} // namespace SonicVisualSplitBase

/*int main() {
    for (int i = 0; i < 3; i++) {
        using milli = std::chrono::milliseconds;
        auto start = std::chrono::high_resolution_clock::now();

        cv::UMat mat = SonicVisualSplitBase::getGameFrame();

        auto finish = std::chrono::high_resolution_clock::now();
        std::cout << "getFrameFromOBS() took "
            << std::chrono::duration_cast<milli>(finish - start).count()
            << " milliseconds\n";

        if (mat.cols == 0) {
            std::cout << "Cannot capture" << std::endl;
            Sleep(1000);
        }
        else {
            std::cout << "Writing to disk!" << std::endl;
            cv::imwrite("C:/tmp/obs.png", mat);
        }
    }

    system("pause");
}*/