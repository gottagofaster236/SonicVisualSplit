#include "FrameStorage.h"
#include "GameVideoCapture.h"
#include "ObsWindowCapture.h"
#include "VirtualCamCapture.h"
#include "DigitsRecognizer.h"
#include "FairMutex.h"
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>
#include <map>
#include <set>
using namespace std::chrono;


namespace SonicVisualSplitBase {
namespace FrameStorage {

/* Map: frame save time in milliseconds -> the frame itself.
 * It is crucial to use cv::Mat instead of cv::UMat here,
 * because saving frames in VRAM can cause the program to get out of VRAM and start corrupting itself. */
static std::map<long long, cv::Mat> savedRawFrames;
static yamc::fair::mutex savedRawFramesMutex;

static std::unique_ptr<GameVideoCapture> gameVideoCapture;
static yamc::fair::mutex gameVideoCaptureMutex;
static int currentVideoSourceIndex;

static std::jthread framesThread;
// The time when the next frame is scheduled to capture, measured in system_clock::duration::count().
static std::atomic<long long> nextFrameCaptureTimeCount;
static const int captureFPS = 61;  // More than 60 for safety.

static void saveOneFrame();
static void resetNextFrameCaptureTime();

void startSavingFrames() {
    stopSavingFrames();

    resetNextFrameCaptureTime();

    framesThread = std::jthread([](std::stop_token stopToken) {
        while (!stopToken.stop_requested()) {
            auto nextFrameCaptureTime = system_clock::time_point(system_clock::duration(nextFrameCaptureTimeCount));
            std::this_thread::sleep_until(nextFrameCaptureTime);
            saveOneFrame();
            system_clock::duration frameLength(seconds(1) / captureFPS);
            nextFrameCaptureTimeCount += frameLength.count();
        }
    });

    /* The above loop is the reason a fair mutex is used.
     * With std::mutex it's possible that framesThread will acquire the mutexes all the time,
     * and other functions will just infinitely wait. */
}


void saveOneFrame() {
    std::lock_guard<yamc::fair::mutex> guard1(gameVideoCaptureMutex);
    std::lock_guard<yamc::fair::mutex> guard2(savedRawFramesMutex);

    if (savedRawFrames.size() == MAX_CAPACITY)
        return;

    cv::Mat rawFrame;
    if (gameVideoCapture)
        rawFrame = gameVideoCapture->captureRawFrame();

    long long currentMilliseconds = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    savedRawFrames[currentMilliseconds] = rawFrame;
}


void resetNextFrameCaptureTime() {
    nextFrameCaptureTimeCount = system_clock::now().time_since_epoch().count();
}


void stopSavingFrames() {
    if (framesThread.joinable()) {
        framesThread.request_stop();
        framesThread.join();  // Make sure the frame thread stops.
    }
}


std::vector<long long> getSavedFramesTimes() {
    std::vector<long long> savedFramesTimes;
    {
        std::lock_guard<yamc::fair::mutex> guard(savedRawFramesMutex);
        for (const auto& [frameTime, frame] : savedRawFrames)
            savedFramesTimes.push_back(frameTime);
    }
    return savedFramesTimes;
}


cv::UMat getSavedFrame(long long frameTime) {
    std::lock_guard<yamc::fair::mutex> guard1(gameVideoCaptureMutex);
    std::lock_guard<yamc::fair::mutex> guard2(savedRawFramesMutex);
    
    // Return an empty UMat if no capture is specified.
    if (!gameVideoCapture)
        return {};
    // Or if the storage doesn't contain the frame.
    if (!savedRawFrames.contains(frameTime))
        return {};

    cv::Mat rawFrame = savedRawFrames[frameTime];
    return gameVideoCapture->processFrame(rawFrame);
}


void deleteSavedFramesInRange(long long beginFrameTime, long long endFrameTime) {
    std::lock_guard<yamc::fair::mutex> guard(savedRawFramesMutex);
    /* Delete the frames whose save time is in the interval [beginFrameTime, endFrameTime).
     * std::map is sorted by key (i.e. frame time). */
    savedRawFrames.erase(savedRawFrames.lower_bound(beginFrameTime), savedRawFrames.lower_bound(endFrameTime));
}


void setVideoCapture(int sourceIndex) {
    std::lock_guard<yamc::fair::mutex> guard(gameVideoCaptureMutex);

    if (currentVideoSourceIndex == sourceIndex) {
        // We may want to recreate the VirtualCamCapture if it fails, so we check for that.
        if (sourceIndex < 0 || gameVideoCapture->getUnsuccessfulFramesStreak() < 5)
            return;
    }

    /* We've changed the capture, now the previous frames are now useless
     * (since the processFrame function is different). */
    std::lock_guard<yamc::fair::mutex> guard2(savedRawFramesMutex);
    savedRawFrames.clear();
    DigitsRecognizer::resetDigitsPlacementAsync();

    currentVideoSourceIndex = sourceIndex;

    if (sourceIndex >= 0)
        gameVideoCapture = std::make_unique<VirtualCamCapture>(sourceIndex);
    else if (sourceIndex == OBS_WINDOW_CAPTURE)
        gameVideoCapture = std::make_unique<ObsWindowCapture>();
    else  // NO_VIDEO_CAPTURE
        gameVideoCapture = nullptr;

    resetNextFrameCaptureTime();
}


}  // namespace SonicVisualSplitBase
}  // namespace FrameStorage
