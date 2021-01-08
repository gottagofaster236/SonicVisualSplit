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
    using System.Diagnostics;
    using System.IO;
    using System.Reflection;
    using System.Runtime.CompilerServices;
    using System.Threading;

    class AutoSplitter
    {
        private LiveSplitState state;
        private SonicVisualSplitSettings settings;
        private static ISet<IFrameConsumer> frameConsumers = new HashSet<IFrameConsumer>();
        private static CancellationTokenSource frameAnalyzerTaskToken;
        private static readonly TimeSpan ANALYZE_FRAME_PERIOD = TimeSpan.FromMilliseconds(500);

        public AutoSplitter(LiveSplitState state, SonicVisualSplitSettings settings)
        {
            this.state = state;
            this.settings = settings;
        }

        public static void StartAnalyzingFrames()
        {
            BaseWrapper.StartSavingFrames();
            if (frameAnalyzerTaskToken != null)
                frameAnalyzerTaskToken.Cancel();
            frameAnalyzerTaskToken = new CancellationTokenSource();
            CancellationToken cancellationToken = frameAnalyzerTaskToken.Token;

            Task.Run(() =>
            {
                while (!cancellationToken.IsCancellationRequested)
                {
                    DateTime start = DateTime.Now;
                    DateTime nextIteration = start + ANALYZE_FRAME_PERIOD;
                    AnalyzeFrame();
                    TimeSpan waitTime = nextIteration - DateTime.Now;
                    if (waitTime > TimeSpan.Zero)
                        Thread.Sleep(waitTime);
                }
            }, cancellationToken);
        }

        public static void StopAnalyzingFrames()
        {
            throw new NotImplementedException();
        }

        [MethodImpl(MethodImplOptions.Synchronized)]
        private static void AnalyzeFrame()
        {
            List<long> frameTimes = BaseWrapper.GetSavedFramesTimes();
            if (frameTimes.Count == 0)
                return;
            long frameTime = frameTimes.Last();
            Debug.WriteLine("Frame time: " + frameTime);

            string templatesDirectory = Path.GetFullPath("C:\\Users\\lievl\\source\\repos\\gottagofaster236\\SonicVisualSplitWIP\\Templates\\Sonic 1@Composite");
            FrameAnalyzer analyzer = new FrameAnalyzer("Sonic 1", templatesDirectory, false);

            bool visualize = frameConsumers.Any();
            AnalysisResult result = analyzer.AnalyzeFrame(frameTime, false, visualize, true);
            BaseWrapper.DeleteSavedFramesBefore(frameTime);

            List<IFrameConsumer> toRemove = new List<IFrameConsumer>();
            foreach (var frameConsumer in frameConsumers)
            {
                if (!frameConsumer.OnFrameAnalyzed(result))
                    toRemove.Add(frameConsumer);
            }

            foreach (var frameConsumerToRemove in toRemove)
            {
                frameConsumers.Remove(frameConsumerToRemove);
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
    }


    interface IFrameConsumer
    {
        // Callback to do something with a frame. Return value: true if the consumer wants to continue to receive frames.
        bool OnFrameAnalyzed(AnalysisResult result);
    }
}
