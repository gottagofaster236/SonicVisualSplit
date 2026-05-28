#pragma once
#include "VideoCaptureManager.h"
#include <opencv2/core.hpp>
#include <vector>
#include <functional>
#include <map>
#include <mutex>

namespace SonicVisualSplitBase {
namespace IGT {

/* This class saves frames from the game video capture for further usage
 * (as we can't process all of them in real-time).
 * To distinguish between the frames, we use the time of capture
 * (in milliseconds since a certain point in time). */
class FrameStorage {
public:
	FrameStorage();

	~FrameStorage();

	// Creates a thread which saves a frame from the game video capture every 16 milliseconds.
	void startSavingFrames();

	// Stops that thread.
	void stopSavingFrames();

	// Returns the save times of the frames that are currently stored.
	std::vector<long long> getSavedFramesTimes() const;

	/* Returns a saved frame by the frame save time, or an empty cv::UMat if:
	 * (1) video was disconnected at the time of capture or
	 * (2) the frame storage doesn't contain this frame. */
	cv::UMat getSavedFrame(long long frameTime) const;

	/* Same as getSavedFrame().
	 * Used to avoid the race condition when you first get
	 * the last frame time and **then** call getSavedFrame(frameTime). */
	cv::UMat getLastSavedFrame() const;

	// Delete the frames whose save time is in the interval [beginFrameTime, endFrameTime)
	void deleteSavedFramesInRange(long long beginFrameTime, long long endFrameTime);

	/* The maximum amount of frames that can be saved in the storage.
	 * When this limit is reached, no new frames are saved until deleteSavedFramesInRange is called. */
	static constexpr int MAX_CAPACITY = 240;

private:
	/* Map: frame save time in milliseconds -> the frame itself.
	 * It is crucial to use cv::Mat instead of cv::UMat here,
	 * because saving frames in VRAM can cause the program to get out of VRAM and start corrupting itself. */
	std::map<long long, cv::Mat> savedFrames;
	mutable std::recursive_mutex savedFramesMutex;
	
	class OnSourceChangedListenerImpl : public VideoCaptureManager::OnSourceChangedListener {
	public:
		OnSourceChangedListenerImpl(FrameStorage& frameStorage);

		void onSourceChanged() override;
	private:
		FrameStorage& frameStorage;
	} onSourceChangedListener;

	class OnFrameCapturedListenerImpl : public VideoCaptureManager::OnFrameCapturedListener {
	public:
		OnFrameCapturedListenerImpl(FrameStorage& frameStorage);

		void onFrameCaptured(const VideoCaptureManager::CapturedFrame& capturedFrame) override;
	private:
		FrameStorage& frameStorage;
	} onFrameCapturedListener;
};

}  // namespace IGT
}  // namespace SonicVisualSplitBase
