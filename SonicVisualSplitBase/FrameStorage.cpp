#include "FrameStorage.h"
#include "GameCapture.h"
#include <chrono>
#include <thread>
#include <atomic>
#include <map>
#include <set>

namespace SonicVisualSplitBase {
namespace FrameStorage {

static std::map<long long, cv::Mat> savedFrames;
static std::mutex savedFramesMutex;
static std::map<long long, AnalysisResult> cachedResults;
static std::mutex cachedResultsMutex;

static std::atomic_bool* framesThreadCancelledFlag = nullptr;


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


void deleteSavedFramesInRange(long long beginFrameTime, long long endFrameTime) {
    // std::map is sorted by key (i.e. frame time)
    {
        std::lock_guard<std::mutex> guard(savedFramesMutex);
        // delete the frames whose save time is in the interval [beginFrameTime, endFrameTime)
        savedFrames.erase(savedFrames.lower_bound(beginFrameTime), savedFrames.lower_bound(endFrameTime));
    }
    {
        std::lock_guard<std::mutex> guard(cachedResultsMutex);
        // delete the frames whose save time is in the interval [beginFrameTime, endFrameTime)
        cachedResults.erase(cachedResults.lower_bound(beginFrameTime), cachedResults.lower_bound(endFrameTime));
    }
}

void addResultToCache(const AnalysisResult& result) {
    std::lock_guard<std::mutex> guard(cachedResultsMutex);
    AnalysisResult resultCopy = result;
    resultCopy.visualizedFrame = cv::Mat();  // make sure that we don't store the image
    cachedResults[resultCopy.frameTime] = resultCopy;
}

bool getResultFromCache(long long frameTime, AnalysisResult& resultOutput) {
    std::lock_guard<std::mutex> guard(cachedResultsMutex);
    auto iterator = cachedResults.find(frameTime);
    if (iterator == cachedResults.end()) {
        // we don't have the result cached
        return false;
    }
    else {
        resultOutput = iterator->second;
        return true;
    }
}

}  // namespace SonicVisualSplitBase
}  // namespace FrameStorage
