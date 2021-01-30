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
        private FrameAnalyzer frameAnalyzer;
        private static ISet<IFrameConsumer> frameConsumers = new HashSet<IFrameConsumer>();
        private static CancellationTokenSource frameAnalyzerTaskToken;
        private static readonly TimeSpan ANALYZE_FRAME_PERIOD = TimeSpan.FromMilliseconds(500);

        public AutoSplitter(LiveSplitState state, SonicVisualSplitSettings settings)
        {
            this.state = state;
            this.settings = settings;
            this.settings.SettingsChanged += OnSettingsChanged;
            OnSettingsChanged();
        }

        private void OnSettingsChanged(object sender = null, EventArgs e = null)
        {
            
        }

        public static void StartAnalyzingFrames()
        {
            BaseWrapper.StartSavingFrames();
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
            if (frameAnalyzerTaskToken != null)
                frameAnalyzerTaskToken.Cancel();
            BaseWrapper.StopSavingFrames();
            BaseWrapper.DeleteAllSavedFrames();
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

            bool visualize;
            lock (frameConsumers)
            {
                visualize = frameConsumers.Any();
            }
            AnalysisResult result = analyzer.AnalyzeFrame(frameTime, false, visualize, true);
            BaseWrapper.DeleteSavedFramesBefore(frameTime);
            SendFrameToConsumers(result);
        }

        private static void SendFrameToConsumers(AnalysisResult result)
        {
            lock (frameConsumers)
            {
                var frameConsumersToRemove = new List<IFrameConsumer>();
                foreach (var frameConsumer in frameConsumers)
                {
                    if (!frameConsumer.OnFrameAnalyzed(result))
                        frameConsumersToRemove.Add(frameConsumer);
                }

                foreach (var frameConsumerToRemove in frameConsumersToRemove)
                {
                    frameConsumers.Remove(frameConsumerToRemove);
                }
            }
        }

        public static void AddFrameConsumer(IFrameConsumer frameConsumer)
        {
            lock (frameConsumers)
            {
                frameConsumers.Add(frameConsumer);
            }
        }

        public static void RemoveFrameConsumer(IFrameConsumer frameConsumer)
        {
            lock (frameConsumers)
            {
                frameConsumers.Remove(frameConsumer);
            }
        }
    }


    interface IFrameConsumer
    {
        // Callback to do something with a frame. Return value: true if the consumer wants to continue to receive frames.
        bool OnFrameAnalyzed(AnalysisResult result);
    }
}
