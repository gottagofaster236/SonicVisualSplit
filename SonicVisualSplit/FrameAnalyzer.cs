using LiveSplit.Model;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using SonicVisualSplitWrapper;
using System.Diagnostics;
using System.IO;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Reflection;

namespace SonicVisualSplit
{
    public class FrameAnalyzer
    {
        private LiveSplitState state;
        private SonicVisualSplitSettings settings;
        private SonicVisualSplitWrapper.FrameAnalyzer nativeFrameAnalyzer;
        private object nativeFrameAnalyzerLock = new object();

        private ISet<IFrameConsumer> frameConsumers = new HashSet<IFrameConsumer>();
        private CancellationTokenSource frameAnalyzerTaskToken;
        private static readonly TimeSpan ANALYZE_FRAME_PERIOD = TimeSpan.FromMilliseconds(500);

        public FrameAnalyzer(LiveSplitState state, SonicVisualSplitSettings settings)
        {
            this.state = state;
            this.settings = settings;
            this.settings.FrameAnalyzer = this;
            this.settings.SettingsChanged += OnSettingsChanged;
            OnSettingsChanged();
        }

        private void OnSettingsChanged(object sender = null, EventArgs e = null)
        {
            lock (nativeFrameAnalyzerLock)
            {
                // finding the path with template images for the game
                string livesplitComponents = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
                string directoryName = settings.Game + "@" + (settings.RGB ? "RGB" : "Composite");
                string templatesDirectory = Path.Combine(livesplitComponents, "SVS Templates", directoryName);

                nativeFrameAnalyzer = new SonicVisualSplitWrapper.FrameAnalyzer(settings.Game, templatesDirectory, settings.Stretched);
            }
        }

        public void StartAnalyzingFrames()
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

        public void StopAnalyzingFrames()
        {
            if (frameAnalyzerTaskToken != null)
                frameAnalyzerTaskToken.Cancel();
            BaseWrapper.StopSavingFrames();
            BaseWrapper.DeleteAllSavedFrames();
        }

        [MethodImpl(MethodImplOptions.Synchronized)]
        private void AnalyzeFrame()
        {
            List<long> frameTimes = BaseWrapper.GetSavedFramesTimes();
            if (frameTimes.Count == 0)
                return;
            long frameTime = frameTimes.Last();

            bool visualize;
            lock (frameConsumers)
            {
                visualize = frameConsumers.Any();
            }

            AnalysisResult result;
            lock (nativeFrameAnalyzerLock)
            {
                result = nativeFrameAnalyzer.AnalyzeFrame(frameTime, false, visualize, true);
            }
            BaseWrapper.DeleteSavedFramesBefore(frameTime);
            SendFrameToConsumers(result);
        }

        private void SendFrameToConsumers(AnalysisResult result)
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

        public void AddFrameConsumer(IFrameConsumer frameConsumer)
        {
            lock (frameConsumers)
            {
                frameConsumers.Add(frameConsumer);
            }
        }

        public void RemoveFrameConsumer(IFrameConsumer frameConsumer)
        {
            lock (frameConsumers)
            {
                frameConsumers.Remove(frameConsumer);
            }
        }
    }


    public interface IFrameConsumer
    {
        // Callback to do something with a frame. Return value: true if the consumer wants to continue to receive frames.
        bool OnFrameAnalyzed(AnalysisResult result);
    }
}
