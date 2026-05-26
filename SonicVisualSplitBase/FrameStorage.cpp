#include "FrameStorage.h"
#include <mutex>
#include <map>

namespace SonicVisualSplitBase {
namespace IGT {

FrameStorage::FrameStorage() : onSourceChangedListener(*this), onFrameCapturedListener(*this) {}


FrameStorage::~FrameStorage() {
    stopSavingFrames();
}


void FrameStorage::startSavingFrames() {
    VideoCaptureManager::addOnFrameCapturedListener(onFrameCapturedListener);
    VideoCaptureManager::addOnSourceChangedListener(onSourceChangedListener);
}


void FrameStorage::stopSavingFrames() {
    VideoCaptureManager::removeOnFrameCapturedListener(onFrameCapturedListener);
    VideoCaptureManager::removeOnSourceChangedListener(onSourceChangedListener);
}


std::vector<long long> FrameStorage::getSavedFramesTimes() const {
    std::lock_guard guard(savedFramesMutex);
    std::vector<long long> savedFramesTimes;
    for (const auto& [frameTime, frame] : savedFrames) {
        savedFramesTimes.push_back(frameTime);
    }
    return savedFramesTimes;
}


cv::UMat FrameStorage::getSavedFrame(long long frameTime) const {
    std::lock_guard guard(savedFramesMutex);
    auto findIter = savedFrames.find(frameTime);
    if (findIter == savedFrames.end()) return {};
    return findIter->second.getUMat(cv::ACCESS_READ);
}

cv::UMat FrameStorage::getLastSavedFrame() const {
    std::lock_guard guard(savedFramesMutex);
    if (savedFrames.empty()) {
        return {};
    }
    long long lastSavedFrame = savedFrames.rbegin()->first;
    return getSavedFrame(lastSavedFrame);
}


void FrameStorage::deleteSavedFramesInRange(long long beginFrameTime, long long endFrameTime) {
    std::lock_guard guard(savedFramesMutex);
    /* Delete the frames whose save time is in the interval [beginFrameTime, endFrameTime).
     * std::map is sorted by key (i.e. frame time). */
    savedFrames.erase(savedFrames.lower_bound(beginFrameTime), savedFrames.lower_bound(endFrameTime));
}


FrameStorage::OnSourceChangedListenerImpl::OnSourceChangedListenerImpl(FrameStorage& frameStorage) : frameStorage(frameStorage) {}


void FrameStorage::OnSourceChangedListenerImpl::onSourceChanged() {
    std::lock_guard guard(frameStorage.savedFramesMutex);
    frameStorage.savedFrames.clear();
}


FrameStorage::OnFrameCapturedListenerImpl::OnFrameCapturedListenerImpl(FrameStorage& frameStorage) : frameStorage(frameStorage) {}


void FrameStorage::OnFrameCapturedListenerImpl::onFrameCaptured(const VideoCaptureManager::CapturedFrame& capturedFrame) {
    std::lock_guard guard(frameStorage.savedFramesMutex);

    if (frameStorage.savedFrames.size() == MAX_CAPACITY)
        return;

    cv::Mat mat;
    capturedFrame.frame.copyTo(mat);
    frameStorage.savedFrames[capturedFrame.timestamp] = mat;
}

}  // namespace IGT
}  // namespace SonicVisualSplitBase
