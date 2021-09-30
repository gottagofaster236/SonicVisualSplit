#pragma once
#include <opencv2/core/types.hpp>

/* OpenCV's types aren't trivially copyable: https://github.com/opencv/opencv/issues/20718.
 * Thus a temporary workaround has to be applied. This will be fixed in OpenCV 4.5.4. */

struct RectTrivial {
    int x, y, width, height;

    RectTrivial() : x(0), y(0), width(0), height(0) {}

    RectTrivial(int x, int y, int width, int height) :
        x(x), y(y), width(width), height(height) {}

    RectTrivial(const cv::Rect& rect) : RectTrivial(rect.x, rect.y, rect.width, rect.height) {}

    operator cv::Rect() const {
        return { x, y, width, height };
    }

    bool empty() const {
        return static_cast<cv::Rect>(*this).empty();
    }
};

struct SizeTrivial {
    int width, height;

    SizeTrivial() : width(0), height(0) {}

    SizeTrivial(int width, int height) : width(width), height(height) {}

    SizeTrivial(const cv::Size& size) : SizeTrivial(size.width, size.height) {}

    operator cv::Size() const {
        return { width, height };
    }
};

inline bool operator!=(const cv::Size& size, const SizeTrivial& sizeTrivial) {
    return static_cast<cv::Size>(sizeTrivial) != size;
}