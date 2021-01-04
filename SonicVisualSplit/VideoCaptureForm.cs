using System;
using System.Diagnostics;
using System.Drawing;
using System.Threading.Tasks;
using System.Windows.Forms;
using SonicVisualSplitWrapper;
using System.IO;
using System.Collections.Generic;
using System.Linq;

namespace SonicVisualSplit
{
    public partial class VideoCaptureForm : Form
    {
        private FrameAnalyzer frameAnalyzer;

        public VideoCaptureForm()
        {
            InitializeComponent();
            BaseWrapper.StartSavingFrames();

            string templatesDirectory = Path.GetFullPath("../../../../Templates/Sonic 1@Composite");
            frameAnalyzer = new FrameAnalyzer("Sonic 1", templatesDirectory, false);

            Task.Run(() =>
            {
                while (true)
                {
                    Test();
                }
            });
        }

        private void Test()
        {
            Debug.WriteLine("Test!");
            Stopwatch stopWatch = Stopwatch.StartNew();

            List<long> frameTimes = BaseWrapper.GetSavedFramesTimes();
            if (frameTimes.Count == 0)
            {
                return;
            }
            long frameTime = frameTimes.Last();
            var result = frameAnalyzer.AnalyzeFrame(frameTime, false, true, true);
            BaseWrapper.DeleteSavedFramesBefore(frameTime);

            stopWatch.Stop();
            long runTime = stopWatch.ElapsedMilliseconds;

            Invoke((MethodInvoker)delegate
            {
                cameraPictureBox.Image = result.VisualizedFrame;
                Text = $"{runTime} ms; time is {result.TimeDigits}; success: {result.FoundAnyDigits}, " +
                $"reason: {result.ErrorReason} is score: {result.IsScoreScreen}";
                if (!result.FoundAnyDigits)
                    result.TimeDigits = "";
                string labelText = $"Time: {result.TimeDigits}";
                if (result.IsScoreScreen)
                    labelText += " Score!";
                if (result.IsBlackScreen)
                    labelText += " Black!";
                timeLabel.Text = labelText;
                Update();
            });
        }
    }
}
