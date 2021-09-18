#include "FrameStorage.h"
#include "GameVideoCapture.h"
#include "ObsWindowCapture.h"
#include "VirtualCamCapture.h"
#include "FairMutex.h"
#include <chrono>
#include <thread>
#include <mutex>
#include <memory>
#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#define NOMINMAX
#include "Windows.h"
using namespace std::chrono;


namespace SonicVisualSplitBase {
namespace FrameStorage {

/* Map: frame save time in milliseconds -> the frame itself.
 * It is crucial to use cv::Mat instead of cv::UMat here,
 * because saving frames in VRAM can cause the program to get out of VRAM and start corrupting itself. */
static std::map<long long, cv::Mat> savedRawFrames;
static yamc::fair::recursive_mutex savedRawFramesMutex;

static std::unique_ptr<GameVideoCapture> gameVideoCapture = std::make_unique<NullCapture>();
static yamc::fair::recursive_mutex gameVideoCaptureMutex;
static int currentVideoSourceIndex;

static std::jthread framesThread;

static std::vector<const OnSourceChangedListener*> onSourceChangedListeners;
static std::mutex onSourceChangedListenersMutex;

// Forward declarations.
static void saveOneFrame();
static void elevateFramesThreadPriority();


void startSavingFrames() {
    stopSavingFrames();

    framesThread = std::jthread([](std::stop_token stopToken) {
        while (!stopToken.stop_requested()) {
            steady_clock::time_point startTime = steady_clock::now(), nextIteration;
            {
                std::lock_guard<yamc::fair::recursive_mutex> guard(gameVideoCaptureMutex);
                saveOneFrame();
                nextIteration = startTime + gameVideoCapture->getDelayAfterLastFrame();
            }
            std::this_thread::sleep_until(nextIteration);
        }
    });

    /* The above loop is the reason a fair mutex is used.
     * With std::mutex it's possible that framesThread will acquire the mutexes all the time,
     * and other functions will just infinitely wait. */

    elevateFramesThreadPriority();
}


void saveOneFrame() {
    std::lock_guard<yamc::fair::recursive_mutex> guard(savedRawFramesMutex);

    if (savedRawFrames.size() == MAX_CAPACITY)
        return;

    cv::Mat rawFrame;
    if (gameVideoCapture)
        rawFrame = gameVideoCapture->captureRawFrame();
    savedRawFrames[getCurrentTimeInMilliseconds()] = rawFrame;
}


void stopSavingFrames() {
    if (framesThread.joinable()) {
        framesThread.request_stop();
        framesThread.join();  // Make sure the frame thread stops.
    }
}


void elevateFramesThreadPriority() {
    HANDLE nativeHandle = framesThread.native_handle();
    SetThreadPriority(nativeHandle, HIGH_PRIORITY_CLASS);
}


std::vector<long long> getSavedFramesTimes() {
    std::vector<long long> savedFramesTimes;
    {
        std::lock_guard<yamc::fair::recursive_mutex> guard(savedRawFramesMutex);
        for (const auto& [frameTime, frame] : savedRawFrames)
            savedFramesTimes.push_back(frameTime);
    }
    return savedFramesTimes;
}


cv::UMat getSavedFrame(long long frameTime) {
    std::lock_guard<yamc::fair::recursive_mutex> guard1(gameVideoCaptureMutex);
    std::lock_guard<yamc::fair::recursive_mutex> guard2(savedRawFramesMutex);
    
    // Return an empty UMat if no capture is specified.
    if (!gameVideoCapture)
        return {};
    // Or if the storage doesn't contain the frame.
    if (!savedRawFrames.contains(frameTime))
        return {};

    cv::Mat rawFrame = savedRawFrames[frameTime];
    return gameVideoCapture->processFrame(rawFrame);
}


cv::UMat getLastSavedFrame() {
    // Locking beforehand to avoid the race condition.
    std::lock_guard<yamc::fair::recursive_mutex> guard1(gameVideoCaptureMutex);
    std::lock_guard<yamc::fair::recursive_mutex> guard2(savedRawFramesMutex);

    if (savedRawFrames.empty())
        return {};
    long long lastSavedFrame = savedRawFrames.rbegin()->first;
    return getSavedFrame(lastSavedFrame);
}


void deleteSavedFramesInRange(long long beginFrameTime, long long endFrameTime) {
    std::lock_guard<yamc::fair::recursive_mutex> guard(savedRawFramesMutex);
    /* Delete the frames whose save time is in the interval [beginFrameTime, endFrameTime).
     * std::map is sorted by key (i.e. frame time). */
    savedRawFrames.erase(savedRawFrames.lower_bound(beginFrameTime), savedRawFrames.lower_bound(endFrameTime));
}


long long getCurrentTimeInMilliseconds() {
    long long currentTimeMs = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    currentTimeMs += (long long) 1e9;  // Make that sure that the epoch of this clock is far in the past.
    return currentTimeMs;
}


void addOnSourceChangedListener(const OnSourceChangedListener& listener) {
    std::lock_guard<std::mutex> guard(onSourceChangedListenersMutex);
    auto findIter = std::ranges::find(onSourceChangedListeners, &listener);
    if (findIter == onSourceChangedListeners.end())
        onSourceChangedListeners.push_back(&listener);
}


void removeOnSourceChangedListener(const OnSourceChangedListener& listener) {
    std::lock_guard<std::mutex> guard(onSourceChangedListenersMutex);
    auto findIter = std::ranges::find(onSourceChangedListeners, &listener);
    onSourceChangedListeners.erase(findIter);
}


static void callOnSourceChangedCallbacks() {
    std::lock_guard<std::mutex> guard(onSourceChangedListenersMutex);
    for (const OnSourceChangedListener* listener : onSourceChangedListeners)
        listener->onSourceChanged();
}


void setVideoCapture(int sourceIndex) {
    std::lock_guard<yamc::fair::recursive_mutex> guard(gameVideoCaptureMutex);

    if (currentVideoSourceIndex == sourceIndex) {
        // We may want to recreate the VirtualCamCapture if it fails, so we check for that.
        if (sourceIndex < 0 || gameVideoCapture->getUnsuccessfulFramesStreak() < 300)
            return;
    }

    /* We've changed the capture, now the previous frames are now useless
     * (since the processFrame function is different). */
    std::lock_guard<yamc::fair::recursive_mutex> guard2(savedRawFramesMutex);
    savedRawFrames.clear();
    callOnSourceChangedCallbacks();

    currentVideoSourceIndex = sourceIndex;

    if (sourceIndex >= 0)
        gameVideoCapture = std::make_unique<VirtualCamCapture>(sourceIndex);
    else if (sourceIndex == OBS_WINDOW_CAPTURE)
        gameVideoCapture = std::make_unique<ObsWindowCapture>();
    else  // NO_VIDEO_CAPTURE
        gameVideoCapture = std::make_unique<NullCapture>();
}

}  // namespace SonicVisualSplitBase
}  // namespace FrameStorage
