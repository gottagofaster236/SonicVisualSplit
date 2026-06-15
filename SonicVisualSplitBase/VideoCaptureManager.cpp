#include "VideoCaptureManager.h"
#include "VideoCapture.h"
#include "FairMutex.h"
#include "VirtualCamCapture.h"
#include <chrono>
#include <opencv2/imgproc.hpp>
#define NOMINMAX
#include <Windows.h>

using namespace std::chrono;

namespace SonicVisualSplitBase {
namespace VideoCaptureManager {

static std::unique_ptr<VideoCapture> videoCapture = std::make_unique<NullCapture>();
static yamc::fair::recursive_mutex videoCaptureMutex;
static int currentVideoSourceIndex;

static std::jthread framesThread;
static std::mutex framesThreadMutex;

static std::vector<OnSourceChangedListener*> onSourceChangedListeners;
static std::mutex onSourceChangedListenersMutex;

static std::vector<OnFrameCapturedListener*> onFrameCapturedListeners;
static std::mutex onFrameCapturedListenersMutex;


void setVideoCapture(int sourceIndex) {
    {
        std::lock_guard guard(videoCaptureMutex);
        if (currentVideoSourceIndex == sourceIndex) {
            // We may want to recreate the VirtualCamCapture if it fails, so we check for that.
            if (sourceIndex < 0 || videoCapture->getUnsuccessfulFramesStreak() < 300)
                return;
        }
        currentVideoSourceIndex = sourceIndex;
        if (sourceIndex >= 0)
            videoCapture = std::make_unique<VirtualCamCapture>(sourceIndex);
        else  // NO_VIDEO_CAPTURE
            videoCapture = std::make_unique<NullCapture>();
    }
    {
        std::lock_guard guard(onSourceChangedListenersMutex);
        for (OnSourceChangedListener* listener : onSourceChangedListeners)
            listener->onSourceChanged();
    }
}


long long getCurrentTimeInMilliseconds() {
    long long currentTimeMs = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    currentTimeMs += (long long)1e9;  // Make that sure that the epoch of this clock is far in the past.
    return currentTimeMs;
}


void addOnSourceChangedListener(OnSourceChangedListener& listener) {
    std::lock_guard guard(onSourceChangedListenersMutex);
    auto findIter = std::ranges::find(onSourceChangedListeners, &listener);
    if (findIter == onSourceChangedListeners.end()) {
        onSourceChangedListeners.push_back(&listener);
    }
}


void removeOnSourceChangedListener(OnSourceChangedListener& listener) {
    std::lock_guard guard(onSourceChangedListenersMutex);
    auto findIter = std::ranges::find(onSourceChangedListeners, &listener);
    if (findIter != onSourceChangedListeners.end()) {
        onSourceChangedListeners.erase(findIter);
    }
}


void swap(CapturedFrame& first, CapturedFrame& second) {
    using std::swap;
    swap(first.frame, second.frame);
    swap(first.timestamp, second.timestamp);
}


static void startCapturingFrames();

void addOnFrameCapturedListener(OnFrameCapturedListener& listener) {
    std::lock_guard guard(framesThreadMutex);
    bool shouldStartCapturingFrames = false;
    {
        std::lock_guard guard(onFrameCapturedListenersMutex);
        auto findIter = std::ranges::find(onFrameCapturedListeners, &listener);
        if (findIter == onFrameCapturedListeners.end()) {
            onFrameCapturedListeners.push_back(&listener);
            if (onFrameCapturedListeners.size() == 1) {
                shouldStartCapturingFrames = true;
            }
        }
    }
    if (shouldStartCapturingFrames) {
        startCapturingFrames();
    }
}



static void stopCapturingFrames();

void removeOnFrameCapturedListener(OnFrameCapturedListener& listener) {
    std::lock_guard guard(framesThreadMutex);
    bool shouldStopCapturingFrames = false;
    {
        std::lock_guard guard(onFrameCapturedListenersMutex);
        auto findIter = std::ranges::find(onFrameCapturedListeners, &listener);
        if (findIter != onFrameCapturedListeners.end()) {
            onFrameCapturedListeners.erase(findIter);
            if (onFrameCapturedListeners.empty()) {
                shouldStopCapturingFrames = true;
            }
        }
    }
    if (shouldStopCapturingFrames) {
        stopCapturingFrames();
    }
}


static void captureOneFrame();

static void startCapturingFrames() {
    framesThread = std::jthread([](std::stop_token stopToken) {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
        /* This loop is the reason a fair mutex is used.
         * With std::mutex it's possible that framesThread will acquire the mutexes all the time,
         * and other functions will just infinitely wait. */
        while (!stopToken.stop_requested()) {
            steady_clock::time_point startTime = steady_clock::now(), nextIteration;
            {
                std::lock_guard guard(videoCaptureMutex);
                captureOneFrame();
                nextIteration = startTime + videoCapture->getDelayAfterLastFrame();
            }
            std::this_thread::sleep_until(nextIteration);
        }
    });
}


static void captureOneFrame() {
    std::lock_guard guard1(videoCaptureMutex);

    cv::UMat frame;
    if (videoCapture)
        frame = videoCapture->captureFrame();

    bool needsHighQualityResize;
    {
        std::lock_guard guard2(onFrameCapturedListenersMutex);
        needsHighQualityResize = std::ranges::any_of(onFrameCapturedListeners, [](OnFrameCapturedListener* listener) { 
            return listener->needsHighQualityResize(); 
        });
    }

    if (frame.rows > MAX_ACCEPTABLE_FRAME_HEIGHT) {
        double scaleFactor = ((double) MAX_ACCEPTABLE_FRAME_HEIGHT) / frame.rows;
        cv::resize(frame, frame, {}, scaleFactor, scaleFactor, needsHighQualityResize ? cv::INTER_AREA : cv::INTER_NEAREST);
    }
    
    std::lock_guard guard2(onFrameCapturedListenersMutex);
    CapturedFrame capturedFrame{frame, getCurrentTimeInMilliseconds()};
    for (OnFrameCapturedListener* listener : onFrameCapturedListeners) {
        listener->onFrameCaptured(capturedFrame);
    }
}


static void stopCapturingFrames() {
    if (framesThread.joinable()) {
        framesThread.request_stop();
        framesThread.join();  // Make sure the frame thread stops.
    }
}

}  // namespace VideoCaptureManager 
}  // namespace SonicVisualSplitBase
