#include "FrameStorage.h"
#include "GameCapture.h"
#include <chrono>
#include <map>

namespace SonicVisualSplitBase {
namespace FrameStorage {

std::map<long long, cv::Mat> savedFrames;
std::mutex savedFramesMutex;


void startSavingFrames() {
    using namespace std::chrono;

    std::thread([]() {
        while (true) {
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
    }).detach();
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
    std::lock_guard<std::mutex> guard(savedFramesMutex);
    // std::map is sorted by key (i.e. frame time)
    while (savedFrames.size() > 0 && savedFrames.begin()->first < frameTime)
        savedFrames.erase(savedFrames.begin());
}

}  // namespace SonicVisualSplitBase
}  // namespace FrameStorage
