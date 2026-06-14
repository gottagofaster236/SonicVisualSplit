#pragma once
#pragma managed(push, off)
#include "../SonicVisualSplitBase/AnalysisSettings.h"
#include <opencv2/core.hpp>
#pragma managed(pop)

namespace SonicVisualSplitWrapper {

public enum class Game {
    Sonic1,
    Sonic2,
    SonicCD,
};


public ref class AnalysisSettings {
public:
    AnalysisSettings(Game game, System::String^ templatesDirectory,
        System::Boolean isStretchedTo16By9, System::Boolean isComposite, System::Boolean autoResetEnabled);

    System::Boolean Equals(Object^ other) override;

    operator SonicVisualSplitBase::AnalysisSettings();

    property Game Game;
    property System::String^ TemplatesDirectory;
    property System::Boolean IsStretchedTo16By9;
    property System::Boolean IsComposite;
    property System::Boolean AutoResetEnabled;
};


public ref class VideoCaptureManager {
public:
    static void SetVideoCapture(int sourceIndex);

    static System::Int64 GetCurrentTimeInMilliseconds();

    literal int NO_VIDEO_CAPTURE = -1;
};


public ref class VirtualCamCapture {
public:
    static System::Collections::Generic::List<System::String^>^ GetVideoDevicesList();
};

System::Drawing::Bitmap^ toBitmap(const cv::Mat& mat);

}  // namespace SonicVisualSplitWrapper
