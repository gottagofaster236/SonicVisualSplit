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

        public VideoSourcesManager(SonicVisualSplitSettings settings)
        {
            this.settings = settings;
            settings.VideoSourcesManager = this;
            VideoSources = new List<string>();
            sourcesScanTask = new CancellableLoopTask(ScanSources, SOURCES_SCAN_PERIOD);
        }

        public void StartScanningSources()
        {
            sourcesScanTask.Start();
        }

        public void StopScanningSources()
        {
            sourcesScanTask.Stop();
            VideoCaptureManager.SetVideoCapture(VideoCaptureManager.NO_VIDEO_CAPTURE);
        }

        private void ScanSources()
        {
            List<string> scannedVideoSources = VirtualCamCapture.GetVideoDevicesList();
            string videoSource = settings.VideoSource;

            // Update the video capture in SonicVisualSplitBase.
            int index = scannedVideoSources.IndexOf(videoSource);
            if (index != -1)
            {
                VideoCaptureManager.SetVideoCapture(index);
            }
            else
            {
                VideoCaptureManager.SetVideoCapture(VideoCaptureManager.NO_VIDEO_CAPTURE);
            }

            // Update the video source selection list in Settings.
            // The possible video capture methods also include OBS Capture.
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
