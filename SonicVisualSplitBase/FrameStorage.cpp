#include "FrameStorage.h"
#include "GameVideoCapture.h"
#include "ObsWindowCapture.h"
#include "VirtualCamCapture.h"
#include <chrono>
#include <thread>
#include <mutex>
#include <memory>
#include <map>
#include <set>
using namespace std::chrono;


namespace SonicVisualSplitBase {
namespace FrameStorage {

/* Map: frame save time in milliseconds -> the frame itself.
 * It is crucial to use cv::Mat instead of cv::UMat here,
 * because saving frames in VRAM can cause the app to get out of VRAM and start corrupting itself. */
static std::map<long long, cv::Mat> savedRawFrames;
static std::mutex savedRawFramesMutex;

static std::unique_ptr<GameVideoCapture> gameVideoCapture;
static std::mutex gameVideoCaptureMutex;
static int currentVideoSourceIndex;

static std::jthread framesThread;

static void saveOneFrame();

void startSavingFrames() {
    stopSavingFrames();

    framesThread = std::jthread([](std::stop_token stopToken) {
        while (!stopToken.stop_requested()) {
            auto startTime = system_clock::now();
            saveOneFrame();
            auto nextIteration = startTime + milliseconds(16);  // 60 fps
            std::this_thread::sleep_until(nextIteration);
        }
    });
}


void saveOneFrame() {
    if (savedRawFrames.size() == MAX_CAPACITY)
        return;
    long long currentMilliseconds = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    cv::Mat rawFrame;
    {
        std::lock_guard<std::mutex> guard(gameVideoCaptureMutex);
        if (gameVideoCapture)
            gameVideoCapture->captureRawFrame();
    }
    {
        std::lock_guard<std::mutex> guard(savedRawFramesMutex);
        savedRawFrames[currentMilliseconds] = rawFrame;
    }
}


void stopSavingFrames() {
    framesThread.request_stop();
    framesThread.join();  // Make sure the frame thread stops.
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
    {
        std::lock_guard<std::mutex> guard(gameVideoCaptureMutex);
        if (gameVideoCapture)
            return gameVideoCapture->processFrame(rawFrame);
        else
            return cv::UMat();  // Return an empty mat in case of error.
    }
}


void deleteSavedFramesInRange(long long beginFrameTime, long long endFrameTime) {
    std::lock_guard<std::mutex> guard(savedRawFramesMutex);
    /* Delete the frames whose save time is in the interval [beginFrameTime, endFrameTime).
     * std::map is sorted by key (i.e. frame time). */
    savedRawFrames.erase(savedRawFrames.lower_bound(beginFrameTime), savedRawFrames.lower_bound(endFrameTime));
}


void setVideoCapture(int sourceIndex) {
    std::lock_guard<std::mutex> guard(gameVideoCaptureMutex);

    if (currentVideoSourceIndex == sourceIndex) {
        // We may want to recreate the VirtualCamCapture if it fails, so we check for that.
        if (sourceIndex < 0 || gameVideoCapture->getUnsuccessfulFramesStreak() < 5)
            return;
    }

    if (sourceIndex >= 0)
        gameVideoCapture = std::make_unique<VirtualCamCapture>(sourceIndex);
    else if (sourceIndex == OBS_WINDOW_CAPTURE)
        gameVideoCapture = std::make_unique<ObsWindowCapture>();
    else  // NO_VIDEO_CAPTURE
        gameVideoCapture = nullptr;
}

}  // namespace SonicVisualSplitBase
}  // namespace FrameStorage
