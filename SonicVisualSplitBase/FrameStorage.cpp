#include "FrameStorage.h"
#include "GameVideoCapture.h"
#include "ObsWindowCapture.h"
#include <chrono>
#include <thread>
#include <atomic>
#include <map>
#include <set>

namespace SonicVisualSplitBase {
namespace FrameStorage {

static GameVideoCapture* gameVideoCapture = new ObsWindowCapture();

static std::map<long long, cv::Mat> savedRawFrames;
static std::mutex savedRawFramesMutex;

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
            cv::Mat rawFrame = gameVideoCapture->captureRawFrame();
            {
                std::lock_guard<std::mutex> guard(savedRawFramesMutex);
                savedRawFrames[currentMilliseconds] = rawFrame;
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
    // make sure the frame thread stops writing to savedRawFrames
    std::lock_guard<std::mutex> guard(savedRawFramesMutex);
}


std::vector<long long> getSavedFramesTimes() {
    std::vector<long long> savedFramesTimes;
    {
        std::lock_guard<std::mutex> guard(savedRawFramesMutex);
        for (const auto& [frameTime, frame] : savedRawFrames)
            savedFramesTimes.push_back(frameTime);
    }
    return savedFramesTimes;
}


cv::UMat getSavedFrame(long long frameTime) {
    cv::Mat rawFrame;
    {
        std::lock_guard<std::mutex> guard(savedRawFramesMutex);
        rawFrame = savedRawFrames[frameTime];
    }
    return gameVideoCapture->processFrame(rawFrame);
}


void deleteSavedFramesInRange(long long beginFrameTime, long long endFrameTime) {
    std::lock_guard<std::mutex> guard(savedRawFramesMutex);
    /* Delete the frames whose save time is in the interval [beginFrameTime, endFrameTime).
     * std::map is sorted by key (i.e. frame time). */
    savedRawFrames.erase(savedRawFrames.lower_bound(beginFrameTime), savedRawFrames.lower_bound(endFrameTime));
}

}  // namespace SonicVisualSplitBase
}  // namespace FrameStorage
