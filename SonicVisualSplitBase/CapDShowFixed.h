/*M///////////////////////////////////////////////////////////////////////////////////////
//
// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html.
//
// Copyright (C) 2014, Itseez, Inc., all rights reserved.
// Third party copyrights are property of their respective owners.
//
//M*/

#ifndef _CAP_DSHOW_FIXED_HPP_
#define _CAP_DSHOW_FIXED_HPP_
#include <opencv2/core/mat.hpp>

class videoInput;
namespace cvFixed {

    // This is a fixed version of OpenCV's VideoCapture
    // that was made to work with the OBS's VirtualCamera.
    // This is needed until https://github.com/opencv/opencv/pull/23460 is merged.
    class VideoCapture_DShow {
    public:
        VideoCapture_DShow(int index);
        virtual ~VideoCapture_DShow();

        double getProperty(int propIdx) const;
        bool setProperty(int propIdx, double propVal);

        bool grabFrame();
        bool retrieveFrame(int outputType, cv::OutputArray frame);
        int getCaptureDomain();
        bool isOpened() const;
    protected:
        void open(int index);
        void close();

        int m_index, m_width, m_height, m_fourcc;
        int m_widthSet, m_heightSet;
        bool m_convertRGBSet;
        static videoInput g_VI;
    };

}

#endif //_CAP_DSHOW_FIXED_HPP_
