#include "FrameStorage.h"
#include "GameCapture.h"
#include <chrono>
#include <map>
#include <set>

namespace SonicVisualSplitBase {
namespace FrameStorage {

std::map<long long, cv::Mat> savedFrames;
std::mutex savedFramesMutex;
std::atomic_bool* framesThreadCancelledFlag = nullptr;

std::set<long long> incorrectlyRecognizedFrames;

void startSavingFrames() {
    using namespace std::chrono;

    stopSavingFrames();
    framesThreadCancelledFlag = new std::atomic_bool(true);
    std::atomic_bool* framesThreadCancelledCopy = framesThreadCancelledFlag;  // can't capture a global variable

    std::thread([framesThreadCancelledCopy]() {
        while (*framesThreadCancelledCopy) {
            auto startTime = system_clock::now();
            long long currentMilliseconds = duration_cast<milliseconds>(startTime.time_since_epoch()).count();
            cv::Mat screenshot = GameCapture::getObsScreenshot();
            {
                std::lock_guard<std::mutex> guard(savedFramesMutex);
                savedFrames[currentMilliseconds] = screenshot;
            }
            auto nextIteration = startTime + milliseconds(16);  // 60 fps
            std::this_thread::sleep_until(nextIteration);
        }
        delete framesThreadCancelledCopy;
    }).detach();
}


void stopSavingFrames() {
    if (framesThreadCancelledFlag) {
        *framesThreadCancelledFlag = false;
        framesThreadCancelledFlag = nullptr;
    }
    // make sure the frame thread stops writing to savedFrames
    std::lock_guard<std::mutex> guard(savedFramesMutex);
}


std::vector<long long> getSavedFramesTimes() {
    std::vector<long long> savedFramesTimes;
    {
        std::lock_guard<std::mutex> guard(savedFramesMutex);
        for (auto& kv : savedFrames)
            savedFramesTimes.push_back(kv.first);
    }
    return savedFramesTimes;
}


cv::UMat getSavedFrame(long long frameTime) {
    cv::Mat obsScreenshot;
    {
        std::lock_guard<std::mutex> guard(savedFramesMutex);
        obsScreenshot = savedFrames[frameTime];
    }
    return GameCapture::getGameFrameFromObsScreenshot(obsScreenshot);
}


void deleteSavedFramesBefore(long long frameTime) {
    // std::map and std::set are sorted by key (i.e. frame time)
    {
        std::lock_guard<std::mutex> guard(savedFramesMutex);
        // delete the frames whose save time is less than frameTime
        savedFrames.erase(savedFrames.begin(), savedFrames.lower_bound(frameTime));
    }
    // delete the frames whose save time is less than frameTime
    incorrectlyRecognizedFrames.erase(incorrectlyRecognizedFrames.begin(), incorrectlyRecognizedFrames.lower_bound(frameTime));
}


void deleteAllSavedFrames() {
    std::lock_guard<std::mutex> guard(savedFramesMutex);
    savedFrames.clear();
}


void markFrameAsRecognizedIncorrectly(long long frameTime) {
    incorrectlyRecognizedFrames.insert(frameTime);
}

bool isFrameRecognizedIncorrectly(long long frameTime) {
    return incorrectlyRecognizedFrames.count(frameTime);
}

}  // namespace SonicVisualSplitBase
}  // namespace FrameStorage
