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
        private SonicVisualSplitWrapper.FrameAnalyzer nativeFrameAnalyzer;
        private object frameAnalyzationLock = new object();

        // All times are in milliseconds.
        // A segment (not to be confused with LiveSplit's segment) is a continuous timespan of gameplay,
        // for example from respawn on a checkpoint to the end of the level.

        // Game time, i.e. the time displayed in LiveSplit.
        private int gameTime = 0;

        // Game time in the beginning of the current segment.
        private int gameTimeOnSegmentStart = 0;

        // What the ingame timer was showing in the beginning of the current segment.
        private int ingameTimerOnSegmentStart = 0;

        private AnalysisResult previousResult = null;

        // How many times in a row the recognition failed.
        private int unsuccessfulStreak = 0;

        private long lastFailedFrameTime = 0;

        List<long> frameTimes;

        private ISet<IResultConsumer> resultConsumers = new HashSet<IResultConsumer>();
        private CancellationTokenSource frameAnalyzerTaskToken;
        private object frameAnalyzerThreadRunningLock = new object();
        private static readonly TimeSpan ANALYZE_FRAME_PERIOD = TimeSpan.FromMilliseconds(500);

        private LiveSplitState state;
        private ITimerModel model;
        private SonicVisualSplitSettings settings;

        public FrameAnalyzer(LiveSplitState state, SonicVisualSplitSettings settings)
        {
            this.state = state;
            model = new TimerModel() { CurrentState = state };

            state.OnReset += OnReset;
            state.CurrentTimingMethod = TimingMethod.GameTime;
            state.IsGameTimePaused = true;  // stop the timer from automatically counting up

            this.settings = settings;
            this.settings.FrameAnalyzer = this;
            this.settings.SettingsChanged += OnSettingsChanged;
            OnSettingsChanged();
        }

        [MethodImpl(MethodImplOptions.Synchronized)]
        private void AnalyzeFrame()
        {
            lock (frameAnalyzationLock)
            {
                frameTimes = BaseWrapper.GetSavedFramesTimes();
                if (frameTimes.Count == 0)
                    return;
                long lastFrameTime = frameTimes.Last();

                bool visualize;
                lock (resultConsumers)
                {
                    visualize = resultConsumers.Any(resultConsumer => resultConsumer.VisualizeAnalysisResult);
                }

                AnalysisResult result;
                result = nativeFrameAnalyzer.AnalyzeFrame(lastFrameTime, checkForScoreScreen: false,
                    recalculateOnError: unsuccessfulStreak >= 5, visualize);
                SendResultToConsumers(result);

                if (!result.IsSuccessful())
                {
                    unsuccessfulStreak++;
                    HandleUnrecognizedFrame(result.FrameTime);
                    return;
                }
                else
                {
                    unsuccessfulStreak = 0;
                }

                if (result.RecognizedTime)
                {
                    if (previousResult != null && previousResult.IsBlackScreen)
                    {
                        // This is the first recognized frame after a black transition screen.
                        // We may have skipped the first frame after the transition, so we go and find it.
                        AnalysisResult frameAfterTransition = FindFirstRecognizedFrameAfter(previousResult.FrameTime);
                        gameTimeOnSegmentStart = gameTime;
                        ingameTimerOnSegmentStart = frameAfterTransition.TimeInMilliseconds;
                        previousResult = frameAfterTransition;
                    }

                    if (previousResult != null && previousResult.RecognizedTime)
                    {
                        // Checking that the recognized time is correct (at least to some degree).
                        // If the time decreased, or if it increased by too much, we just ignore that.
                        if (result.TimeInMilliseconds < previousResult.TimeInMilliseconds
                            || result.TimeInMilliseconds - previousResult.TimeInMilliseconds
                                > result.FrameTime - previousResult.FrameTime + 1500)
                        {
                            HandleUnrecognizedFrame(result.FrameTime);
                            return;
                        }
                    }

                    gameTime = (result.TimeInMilliseconds - ingameTimerOnSegmentStart) + gameTimeOnSegmentStart;
                    UpdateGameTime();
                }
                else if (result.IsBlackScreen && previousResult != null && !previousResult.IsBlackScreen)
                {
                    // This is the first black frame. That means we've entered the transition.
                    // We try to find the last frame before the transition to find the time.
                    AnalysisResult frameBeforeTransition = FindFirstRecognizedFrameBefore(result.FrameTime);
                    gameTime = (frameBeforeTransition.TimeInMilliseconds - ingameTimerOnSegmentStart) + gameTimeOnSegmentStart;
                    gameTimeOnSegmentStart = gameTime;
                    UpdateGameTime();

                    AnalysisResult scoreScreenCheck;
                    lock (frameAnalyzationLock)
                    {
                        scoreScreenCheck = nativeFrameAnalyzer.AnalyzeFrame(frameBeforeTransition.FrameTime, 
                            checkForScoreScreen: true, recalculateOnError: false, visualize: false);
                    }
                    if (scoreScreenCheck.IsScoreScreen)
                        model.Split();
                }

                previousResult = result;
                BaseWrapper.DeleteSavedFramesBefore(lastFrameTime);
            }
        }

        private AnalysisResult FindFirstRecognizedFrameAfter(long startFrameTime)
        {
            return FindFirstRecognizedFrame(true, startFrameTime);
        }

        private AnalysisResult FindFirstRecognizedFrameBefore(long startFrameTime)
        {
            return FindFirstRecognizedFrame(false, startFrameTime);
        }

        private AnalysisResult FindFirstRecognizedFrame(bool after, long startFrameTime)
        {
            int increment = after ? 1 : -1;
            int startingIndex = frameTimes.IndexOf(startFrameTime) + increment;
            for (int frameIndex = startingIndex; ; frameIndex += increment)
            {
                long frameTime = frameTimes[frameIndex];
                AnalysisResult frameBeforeTransition = nativeFrameAnalyzer.AnalyzeFrame(frameTime,
                    checkForScoreScreen: false, recalculateOnError: false, visualize: false);
                if (frameBeforeTransition.RecognizedTime)
                    return frameBeforeTransition;
            }
        }

        private void HandleUnrecognizedFrame(long frameTime)
        {
            // Delete the frame so we don't try to analyze it again.
            BaseWrapper.DeleteSavedFrame(frameTime);

            if (previousResult == null || previousResult.FrameTime < lastFailedFrameTime)
            {
                // If the previous analyzed frame was also a fail, we delete the frames since that moment,
                // to save up on memory usage.
                BaseWrapper.DeleteSavedFramesInRange(lastFailedFrameTime, frameTime);
            }

            lastFailedFrameTime = frameTime;
        }

        private void UpdateGameTime()
        {
            // make sure the run started
            if (state.CurrentSplitIndex == -1)
                model.Start();
            state.SetGameTime(TimeSpan.FromMilliseconds(gameTime));
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
                    lock (frameAnalyzerThreadRunningLock)
                    {
                        DateTime start = DateTime.Now;
                        DateTime nextIteration = start + ANALYZE_FRAME_PERIOD;
                        AnalyzeFrame();
                        TimeSpan waitTime = nextIteration - DateTime.Now;
                        if (waitTime > TimeSpan.Zero)
                            Thread.Sleep(waitTime);
                    }
                }
            }, cancellationToken);
        }

        public void StopAnalyzingFrames()
        {
            if (frameAnalyzerTaskToken != null)
                frameAnalyzerTaskToken.Cancel();
            BaseWrapper.StopSavingFrames();
            BaseWrapper.DeleteAllSavedFrames();
            // Making sure that the frame analyzer thread stops.
            lock (frameAnalyzerThreadRunningLock) { }

            // Restoring the LiveSplit state to the original.
            state.IsGameTimePaused = false;
        }

        private void OnSettingsChanged(object sender = null, EventArgs e = null)
        {
            lock (frameAnalyzationLock)
            {
                // finding the path with template images for the game
                string livesplitComponents = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
                string directoryName = settings.Game + "@" + (settings.RGB ? "RGB" : "Composite");
                string templatesDirectory = Path.Combine(livesplitComponents, "SVS Templates", directoryName);

                nativeFrameAnalyzer = new SonicVisualSplitWrapper.FrameAnalyzer(settings.Game, templatesDirectory, settings.Stretched);
            }
        }

        private void OnReset(object sender, TimerPhase value)
        {
            throw new NotImplementedException();
            /*
            gameTime = gameTimeOnSegmentStart = ingameTimerOnSegmentStart = 0;
            previousResult = null;
            */
        }

        private void SendResultToConsumers(AnalysisResult result)
        {
            lock (resultConsumers)
            {
                var resultConsumersToRemove = new List<IResultConsumer>();
                foreach (var resultConsumer in resultConsumers)
                {
                    if (!resultConsumer.OnFrameAnalyzed(result))
                        resultConsumersToRemove.Add(resultConsumer);
                }

                foreach (var resultConsumerToRemove in resultConsumersToRemove)
                {
                    resultConsumers.Remove(resultConsumerToRemove);
                }
            }
        }

        public void AddResultConsumer(IResultConsumer resultConsumer)
        {
            lock (resultConsumers)
            {
                resultConsumers.Add(resultConsumer);
            }
        }

        public void RemoveResultConsumer(IResultConsumer resultConsumer)
        {
            lock (resultConsumers)
            {
                resultConsumers.Remove(resultConsumer);
            }
        }

        public interface IResultConsumer
        {
            // Callback to do something with the analysis result. Return value: true if the consumer wants to continue to receive frames.
            bool OnFrameAnalyzed(AnalysisResult result);

            bool VisualizeAnalysisResult { get; }
        }
    }
}
