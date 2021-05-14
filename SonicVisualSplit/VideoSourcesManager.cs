using System;
using System.Collections.Generic;
using System.Linq;
using SonicVisualSplitWrapper;

namespace SonicVisualSplit
{
    public class VideoSourcesManager
    {
        public List<string> VideoSources 
        {
            get { return _videoSources; }
            private set { _videoSources = value; }
        }
        private volatile List<string> _videoSources = new List<string>();

        private SonicVisualSplitSettings settings;
        private CancellableLoopTask sourcesScanTask;
        private static readonly TimeSpan SOURCES_SCAN_PERIOD = TimeSpan.FromSeconds(1);
        private const string OBS_CAPTURE_STRING = "OBS Window Capture";

        public VideoSourcesManager(SonicVisualSplitSettings settings)
        {
            this.settings = settings;
            settings.VideoSourcesManager = this;
            VideoSources = new List<string>();
            VideoSources.Add(OBS_CAPTURE_STRING);
            sourcesScanTask = new CancellableLoopTask(ScanSources, SOURCES_SCAN_PERIOD);
        }

        public void StartScanningSources()
        {
            sourcesScanTask.Start();
        }

        public void StopScanningSources()
        {
            sourcesScanTask.Stop();
            FrameStorage.SetVideoCapture(FrameStorage.NO_VIDEO_CAPTURE);
        }

        private void ScanSources()
        {
            List<string> scannedVideoSources = VirtualCamCapture.GetVideoDevicesList();
            string videoSource = settings.VideoSource;

            // Update the video capture in SonicVisualSplitBase.
            if (videoSource == OBS_CAPTURE_STRING)
            {
                FrameStorage.SetVideoCapture(FrameStorage.OBS_WINDOW_CAPTURE);
            }
            else
            {
                int index = scannedVideoSources.IndexOf(videoSource);
                if (index != -1)
                    FrameStorage.SetVideoCapture(index);
                else
                    FrameStorage.SetVideoCapture(FrameStorage.NO_VIDEO_CAPTURE);
            }

            // Update the video source selection list in Settings.
            // The possible video capture methods also include OBS Capture.
            scannedVideoSources.Insert(0, OBS_CAPTURE_STRING);
            scannedVideoSources = RemoveDuplicates(scannedVideoSources);

            if (!VideoSources.SequenceEqual(scannedVideoSources))
            {
                VideoSources = scannedVideoSources;
                settings.OnVideoSourcesListUpdated();
            }
        }

        private static List<string> RemoveDuplicates(List<string> videoSources)
        {
            var withoutDuplicates = new List<string>();

            foreach (string name in videoSources)
            {
                string addName = name;
                int repetition = 1;
                while (withoutDuplicates.Contains(addName))
                {
                    repetition++;
                    addName = name + " (" + repetition + ")";
                }
                withoutDuplicates.Add(addName);
            }

            return withoutDuplicates;
        }
    }
}
