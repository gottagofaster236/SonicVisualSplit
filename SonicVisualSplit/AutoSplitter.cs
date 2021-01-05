using LiveSplit.Model;
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace SonicVisualSplit
{
    using SonicVisualSplitWrapper;
    using System.IO;

    class AutoSplitter
    {
        private LiveSplitState state;
        private SonicVisualSplitSettings settings;
        private static List<IFrameConsumer> frameConsumers = new List<IFrameConsumer>();
        private static System.Timers.Timer frameAnalyzeTimer;

        public AutoSplitter(LiveSplitState state, SonicVisualSplitSettings settings)
        {
            this.state = state;
            this.settings = settings;
        }

        public static void StartAnalyzingFrames()
        {
            #if true
            AllocConsole();
            #endif
            Console.WriteLine("test print!");

            BaseWrapper.StartSavingFrames();
            frameAnalyzeTimer = new System.Timers.Timer();
            frameAnalyzeTimer.Interval = 500;
            frameAnalyzeTimer.Elapsed += (sender, eventArgs) =>
            {
                try
                {
                    AnalyzeFrame();
                }
                catch (Exception e)
                {
                    Console.WriteLine("Exception: " + e);
                }
            };
            frameAnalyzeTimer.Start();
        }

        public static void StopAnalyzingFrames()
        {
            throw new NotImplementedException();
        }

        private static void AnalyzeFrame()
        {
            List<long> frameTimes = BaseWrapper.GetSavedFramesTimes();
            if (frameTimes.Count == 0)
                return;
            long frameTime = frameTimes.Last();
            Console.WriteLine("Frame time: " + frameTime);

            string templatesDirectory = Path.GetFullPath("../../../../Templates/Sonic 1@Composite");
            FrameAnalyzer analyzer = new FrameAnalyzer("Sonic 1", templatesDirectory, false);

            bool visualize = frameConsumers.Any();
            AnalysisResult result = analyzer.AnalyzeFrame(frameTime, false, visualize, true);
            BaseWrapper.DeleteSavedFramesBefore(frameTime);

            foreach (var frameConsumer in frameConsumers)
            {
                frameConsumer.OnFrameAnalyzed(result);
            }
        }

        public static void AddFrameConsumer(IFrameConsumer frameConsumer)
        {
            frameConsumers.Add(frameConsumer);
        }

        public static void RemoveFrameConsumer(IFrameConsumer frameConsumer)
        {
            frameConsumers.Remove(frameConsumer);
        }


        // Open up console for debugging
        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        static extern bool AllocConsole();
    }


    interface IFrameConsumer
    {
        void OnFrameAnalyzed(AnalysisResult result);
    }
}
