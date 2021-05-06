﻿using LiveSplit.Model;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using SonicVisualSplitWrapper;
using System.IO;
using System.Reflection;
using System.Windows.Forms;
using System.IO.Compression;

namespace SonicVisualSplit
{
    public class FrameAnalyzer
    {
        private SonicVisualSplitWrapper.FrameAnalyzer nativeFrameAnalyzer;
        private object frameAnalyzationLock = new object();

        /* All times are in milliseconds.
         * A segment (not to be confused with LiveSplit's segment) is a continuous timespan of gameplay,
         * for example from respawn on a checkpoint to the end of the level. */

        // Game time, i.e. the time displayed in LiveSplit.
        private int gameTime = 0;

        // Game time in the beginning of the current segment.
        private int gameTimeOnSegmentStart = 0;

        // What the ingame timer was showing in the beginning of the current segment.
        private int ingameTimerOnSegmentStart = 0;

        // The time of capture of the first recognized frame of the current segment.
        private long firstFrameTimeOfSegment = 0;

        // The previous successfully analyzed frame.
        private AnalysisResult previousResult = null;

        // A state when we've split already, but the next stage hasn't started yet.
        private bool isAfterSplit = false;

        // If isAfterSplit is true, this is the ingame timer we've split on.
        private int ingameTimerOnSplit = 0;

        // The time of capture of the last frame that was checked for score screen.
        private long lastScoreScreenCheckFrameTime = 0;

        /* The time that was reported on the last frame with successful score screen check,
         * or -1 if the last frame was not such. */
        private int ingameTimerOnLastScoreCheck = -1;

        // The last frame of a transition that we've successfully analyzed.
        private AnalysisResult frameBeforeTransition = null;

        // How many times in a row the recognition failed.
        private int unsuccessfulStreak = 0;

        List<long> savedFrameTimes;

        private ISet<IResultConsumer> resultConsumers = new HashSet<IResultConsumer>();
        private CancellableLoopTask frameAnalyzationTask;
        private static readonly TimeSpan ANALYZE_FRAME_PERIOD = TimeSpan.FromMilliseconds(200);
    
        private LiveSplitState state;
        private ITimerModel model;
        private SonicVisualSplitSettings settings;

        public FrameAnalyzer(LiveSplitState state, SonicVisualSplitSettings settings)
        {
            this.state = state;
            model = new TimerModel() { CurrentState = state };
            
            this.settings = settings;
            this.settings.FrameAnalyzer = this;
            this.settings.SettingsChanged += OnSettingsChanged;

            UnpackTemplatesArrayIfNeeded();

            frameAnalyzationTask = new CancellableLoopTask(AnalyzeFrame, ANALYZE_FRAME_PERIOD);
        }

        private void AnalyzeFrame()
        {
            lock (frameAnalyzationLock)
            {
                savedFrameTimes = FrameStorage.GetSavedFramesTimes();
                if (savedFrameTimes.Count == 0)
                    return;
                long lastFrameTime = savedFrameTimes.Last();

                if (savedFrameTimes.Count == FrameStorage.GetMaxCapacity())
                {
                    /* The frames are not saved if there are too many saved already (to prevent an OOM).
                     * In this case we unfortunately have to delete the old frames, even if we need them. */
                    FrameStorage.DeleteSavedFramesBefore(lastFrameTime);
                }
                
                bool visualize;
                lock (resultConsumers)
                {
                    visualize = resultConsumers.Any(resultConsumer => resultConsumer.VisualizeAnalysisResult);
                }

                bool checkForScoreScreen = (lastFrameTime - lastScoreScreenCheckFrameTime >= 1000);
                if (checkForScoreScreen)
                    lastScoreScreenCheckFrameTime = lastFrameTime;

                if (unsuccessfulStreak >= 3 || (previousResult != null && previousResult.IsBlackScreen))
                {
                    SonicVisualSplitWrapper.FrameAnalyzer.ResetDigitsPlacement();
                }

                SonicVisualSplitWrapper.FrameAnalyzer.ReportCurrentSplitIndex(state.CurrentSplitIndex);

                AnalysisResult result = nativeFrameAnalyzer.AnalyzeFrame(lastFrameTime, checkForScoreScreen, visualize);
                SendResultToConsumers(result);

                if (!CheckAnalysisResult(result, isLatestFrame: true))
                {
                    return;
                }
                else
                {
                    unsuccessfulStreak = 0;
                }

                if (isAfterSplit)
                {
                    CheckIfNextStageStarted(result, checkForScoreScreen);
                }
                else if (result.RecognizedTime)
                {
                    CheckIfFrameIsAfterTransition(result);
                    if (!result.IsSuccessful())  // CheckIfFrameIsAfterTransition checks the analysis result.
                        return;

                    SplitIfNecessary(result, checkForScoreScreen);

                    if (!isAfterSplit)
                        UpdateGameTime(result);
                }
                else if (previousResult != null && previousResult.RecognizedTime &&
                    (result.IsBlackScreen || result.IsWhiteScreen))
                {
                    HandleFirstFrameOfTransition(result);
                }

                previousResult = result;
                FrameStorage.DeleteSavedFramesBefore(lastFrameTime);
            }
        }

        private void CheckIfNextStageStarted(AnalysisResult result, bool checkedForScoreScreen)
        {
            /* Check if the next stage has started.
             * Timer starts at zero, but we may miss that exact frame,
             * so we just check that the timer is less than 10 seconds. */
            if (result.RecognizedTime && result.TimeInMilliseconds < Math.Min(ingameTimerOnSplit, 10000))
            {
                bool isScoreScreen;
                if (checkedForScoreScreen)
                {
                    isScoreScreen = result.IsScoreScreen;
                }
                else
                {
                    var scoreScreenCheck = nativeFrameAnalyzer.AnalyzeFrame(result.FrameTime,
                        checkForScoreScreen: true, visualize: false);
                    isScoreScreen = scoreScreenCheck.IsScoreScreen;
                }

                if (!isScoreScreen)
                {
                    isAfterSplit = false;
                    UpdateGameTime(result);
                }
            }
        }

        private void CheckIfFrameIsAfterTransition(AnalysisResult result)
        {
            if (frameBeforeTransition == null || previousResult == null || state.CurrentSplitIndex == -1)
                return;

            if (previousResult.IsWhiteScreen)
            {
                /* This was a transition to a special stage, or time travel in SCD.
                 * Checking that time too. */
                
                if (settings.Game == "Sonic CD")
                {
                    // In Sonic CD, the timer decreases by ~0.3 seconds after time travel.
                    const int marginOfError = 330;
                    frameBeforeTransition.TimeInMilliseconds -= marginOfError;
                    frameBeforeTransition.FrameTime -= marginOfError;
                }

                if (!CheckAnalysisResult(result, isLatestFrame: true, frameBeforeTransition))
                {
                    return;
                }
            }
            else if (previousResult.IsBlackScreen)
            {
                /* This is the first recognized frame after a black transition screen.
                 * We may have skipped the first frame after the transition, so we go and find it. */
                AnalysisResult frameAfterTransition =
                    FindFirstRecognizedFrame(after: true, previousResult.FrameTime, fallback: result);
                SetFirstFrameOfSegment(frameAfterTransition);
                CorrectFirstFrameOfSegment(result);
            }
        }

        private void SetFirstFrameOfSegment(AnalysisResult result)
        {
            gameTimeOnSegmentStart = gameTime;
            firstFrameTimeOfSegment = result.FrameTime;
            previousResult = result;

            if (result.TimeInMilliseconds > 60)
            {
                ingameTimerOnSegmentStart = result.TimeInMilliseconds;
            }
            else
            {
                /* Sonic CD starts the timer from 0'00"03 (or 0'00"01) when you die.
                 * We check for 6 centiseconds (in case of a 30 fps capture).
                 * This check may be deleted as Sonic CD mods will clarify the rules. */
                ingameTimerOnSegmentStart = 0;
            }
        }

        private void SplitIfNecessary(AnalysisResult result, bool checkedForScoreScreen)
        {
            // If we're splitting, we want to double-check that.
            if (result.IsScoreScreen)
            {
                if (ingameTimerOnLastScoreCheck == result.TimeInMilliseconds)
                {
                    Split();
                    isAfterSplit = true;
                    gameTimeOnSegmentStart = gameTime;
                    ingameTimerOnSegmentStart = 0;
                    ingameTimerOnSplit = result.TimeInMilliseconds;
                }
                else if (result.TimeInMilliseconds >= 1000)
                {
                    /* Checking that the time is at least a second,
                     * because in SCD zone title can be recognized as score screen. */
                    ingameTimerOnLastScoreCheck = result.TimeInMilliseconds;
                    // Recalculating to increase accuracy.
                    SonicVisualSplitWrapper.FrameAnalyzer.ResetDigitsPlacement();
                }
            }
            else if (checkedForScoreScreen)
            {
                ingameTimerOnLastScoreCheck = -1;
            }
        }

        private void HandleFirstFrameOfTransition(AnalysisResult result)
        {
            bool isLastSplit = state.CurrentSplitIndex == state.Run.Count - 1;

            // In Sonic CD the timer stops before the ending transition, no need to update the time.
            if (result.IsBlackScreen || (result.IsWhiteScreen && isLastSplit && settings.Game != "Sonic CD"))
            {
                /* This is the first frame of a transition.
                 * We find the last frame before the transition to find out the time. */
                frameBeforeTransition =
                    FindFirstRecognizedFrame(after: false, result.FrameTime, fallback: previousResult);
                UpdateGameTime(frameBeforeTransition);
            }
            else
                frameBeforeTransition = previousResult;

            if (isLastSplit)
            {
                /* If we were on the last split, that means the run has finished.
                 * (Or it was a death on the final stage. In this case we'll undo the split automatically.) */
                Split();
            }
            else if (result.IsBlackScreen)
            {
                if (settings.Game == "Sonic 1" && state.CurrentSplitIndex == 17)
                {
                    /* Hack: Sonic 1's Scrap Brain 3 doesn't have the proper transition.
                     * If it's actually a death, you have to manually undo the split. */
                    Split();
                }
                else if (settings.Game == "Sonic 2" && (state.CurrentSplitIndex == 17 || state.CurrentSplitIndex == 18))
                {
                    // Same for Sonic 2's Sky Chase and Wing Fortress.
                    Split();
                }
            }
        }

        private void UpdateGameTime(AnalysisResult result)
        {
            gameTime = (result.TimeInMilliseconds - ingameTimerOnSegmentStart) + gameTimeOnSegmentStart;
            UpdateGameTime();
        }

        private void UpdateGameTime()
        {
            int gameTimeCopy = gameTime;

            RunOnUiThreadAsync(() => {
                // Make sure the run started and hasn't finished
                if (state.CurrentSplitIndex == -1)
                    model.Start();
                else if (state.CurrentSplitIndex == state.Run.Count)
                    model.UndoSplit();
                state.SetGameTime(TimeSpan.FromMilliseconds(gameTimeCopy));
            });
        }

        /* Finds the first recognized frame before or after a certain frame.
         * If no recognized frames are found, returns the fallback frame. */
        private AnalysisResult FindFirstRecognizedFrame(bool after, long startFrameTime, AnalysisResult fallback)
        {
            int increment = after ? 1 : -1;
            int startingIndex = savedFrameTimes.IndexOf(startFrameTime) + increment;
            int fallbackIndex = savedFrameTimes.IndexOf(fallback.FrameTime);

            TimeSpan timeout = TimeSpan.FromSeconds(5);  // FindFirstRecognizedFrame is a very expensive operation.
            DateTime start = DateTime.Now;

            for (int frameIndex = startingIndex; frameIndex != fallbackIndex; frameIndex += increment)
            {
                long frameTime = savedFrameTimes[frameIndex];
                AnalysisResult result = nativeFrameAnalyzer.AnalyzeFrame(frameTime,
                    checkForScoreScreen: false, visualize: false);
                if (result.RecognizedTime)
                {
                    if (CheckAnalysisResult(result))
                    {
                        return result;
                    }
                    else if (after)
                    {
                        SonicVisualSplitWrapper.FrameAnalyzer.ResetDigitsPlacement();
                        /* When we're going into a transition, the screen fades out. So we **don't** reset 
                         * the frame analyzer, because it won't find the "TIME" label on the screen once it's faded out. */
                    }
                }

                TimeSpan elapsed = DateTime.Now - start;
                if (elapsed > timeout)
                {
                    return fallback;
                }
            }

            /* Haven't found anything.
             * This probably happened because we've reset the precalculated digits positions,
             * and nothing was found with the new ones. */
            return fallback;
        }

        /* If the first frame of the segment is inconsistent with the second frame of the segment,
         * we assume the first frame of the segment is wrong. */
        private void CorrectFirstFrameOfSegment(AnalysisResult newResult)
        {
            /* Go over all frames to find the first frame which is consistent with the frame after it.
             * We use CheckAnalysisResult to check that. */
            int beginIndex = savedFrameTimes.IndexOf(firstFrameTimeOfSegment) + 1;
            int endIndex = savedFrameTimes.IndexOf(newResult.FrameTime);

            for (int frameIndex = beginIndex; frameIndex <= endIndex; frameIndex++)
            {
                long frameTime = savedFrameTimes[frameIndex];
                AnalysisResult secondFrameOfSegment;
                if (frameTime != newResult.FrameTime)
                    secondFrameOfSegment = nativeFrameAnalyzer.AnalyzeFrame(frameTime, checkForScoreScreen: false, visualize: false);
                else
                    secondFrameOfSegment = newResult;

                if (secondFrameOfSegment.TimeInMilliseconds == previousResult.TimeInMilliseconds &&
                    secondFrameOfSegment.FrameTime - previousResult.FrameTime < 30)
                {
                    /* This frame may be a duplicate of the first frame, as we are
                     * capturing a frame every 16 ms (which is less than 1/60th of a second). */
                    DeleteFrame(secondFrameOfSegment, incrementUnsuccessfulStreak: false);
                    continue;
                }

                if (CheckAnalysisResult(secondFrameOfSegment))
                {
                    // The first frame of the segment (which is equal to previousResult) is presumably correct.
                    return;
                }
                else
                {
                    /* The second frame of segment is inconsistent with the first frame of segment,
                     * so we assume the first frame was incorrect. */
                    SetFirstFrameOfSegment(secondFrameOfSegment);
                }
            }
        }

        /* Checks a frame with recognized digits, based on the previousResult.
         * Returns true if the result is (presumably) correct. */
        bool CheckAnalysisResult(AnalysisResult result, bool isLatestFrame = false, AnalysisResult previousResult = null)
        {
            if (previousResult == null)  // Default parameter.
                previousResult = this.previousResult;

            if (!result.IsSuccessful())
            {
                if (isLatestFrame)
                    DeleteFrame(result);
                return false;
            }

            if (!isAfterSplit && result.RecognizedTime && previousResult != null && previousResult.RecognizedTime)
            {
                /* Checking that the recognized time is correct (at least to some degree).
                 * If the time decreased, or if it increased by too much, we just ignore that.
                 * We want to recover from errors, so we also introduce a margin of error. */
                long timeElapsed = result.FrameTime - previousResult.FrameTime + 50;
                // Adding a margin to the elapsed time, as frames are not captured at precise moments.
                long timerAccuracy = (settings.Game == "Sonic CD" ? 10 : 1000);
                long marginOfError = (long)(timeElapsed * 0.35);

                if (result.TimeInMilliseconds < previousResult.TimeInMilliseconds - marginOfError
                    || result.TimeInMilliseconds - previousResult.TimeInMilliseconds
                        > timeElapsed + timerAccuracy + marginOfError)
                {
                    if (isLatestFrame && previousResult.FrameTime == firstFrameTimeOfSegment)
                    {
                        CorrectFirstFrameOfSegment(result);
                        return true;
                    }

                    if (isLatestFrame)
                        DeleteFrame(result);
                    result.MarkAsIncorrectlyRecognized();
                    return false;
                }
            }
            return true;
        }


        /* Deletes a frame from the FrameStorage.
         * With incrementUnsuccessfulStreak being true, this function should be
         * only called for the latest frame that's been analyzed. */
        private void DeleteFrame(AnalysisResult result, bool incrementUnsuccessfulStreak = true)
        {
            // Delete the frame so we don't try to analyze it again.
            long frameTime = result.FrameTime;
            FrameStorage.DeleteSavedFrame(frameTime);
            result.MarkAsIncorrectlyRecognized();

            if (incrementUnsuccessfulStreak)
            {
                if (previousResult != null && !previousResult.IsSuccessful())
                {
                    /* If the previous analyzed frame was also a fail, we delete the frames since that moment,
                     * to save up on memory usage. */
                    FrameStorage.DeleteSavedFramesInRange(previousResult.FrameTime, frameTime);
                }
                unsuccessfulStreak++;
            }
        }

        public void StartAnalyzingFrames()
        {
            state.CurrentTimingMethod = TimingMethod.GameTime;
            state.IsGameTimePaused = true;  // stop the timer from automatically counting up
            state.OnReset += OnReset;

            FrameStorage.StartSavingFrames();
            frameAnalyzationTask.Start();
        }

        public void StopAnalyzingFrames()
        {
            frameAnalyzationTask.Stop();

            FrameStorage.StopSavingFrames();
            FrameStorage.DeleteAllSavedFrames();

            state.OnReset -= OnReset;
            OnReset();
        }

        private void OnSettingsChanged(object sender, EventArgs e)
        {
            bool haveToStopAnalyzingFrames = false;

            lock (frameAnalyzationLock)
            {
                if (!settings.IsPracticeMode)
                {
                    // finding the path with template images for the game
                    string livesplitComponents = GetLivesplitComponentsDirectory();
                    string directoryName = settings.Game + "@" + (settings.RGB ? "RGB" : "Composite");
                    string templatesDirectory = Path.Combine(livesplitComponents, "SVS Templates", directoryName);

                    bool wasAnalyzingFrames = (nativeFrameAnalyzer != null);
                    nativeFrameAnalyzer = new SonicVisualSplitWrapper.FrameAnalyzer(settings.Game, templatesDirectory,
                        settings.Stretched, isComposite: !settings.RGB);

                    if (!wasAnalyzingFrames)
                    {
                        StartAnalyzingFrames();
                    }
                }
                else if (nativeFrameAnalyzer != null)
                {
                    haveToStopAnalyzingFrames = true;
                    // Stopping the thread outside of the lock
                }
            }

            if (haveToStopAnalyzingFrames)
            {
                StopAnalyzingFrames();
                nativeFrameAnalyzer = null;
            }
        }

        private static void UnpackTemplatesArrayIfNeeded()
        {
            string livesplitComponents = GetLivesplitComponentsDirectory();
            string zipLocation = Path.Combine(livesplitComponents, "SVS Templates.zip");
            if (File.Exists(zipLocation))
            {
                string destinationDirectory = Path.Combine(livesplitComponents, "SVS Templates");
                try
                {
                    if (Directory.Exists(destinationDirectory))
                        Directory.Delete(destinationDirectory, recursive: true);
                    ZipFile.ExtractToDirectory(zipLocation, destinationDirectory);
                    File.Delete(zipLocation);
                }
                catch (UnauthorizedAccessException)
                {
                    MessageBox.Show("Please restart LiveSplit with administrator privileges.", "SVS Error",
                        MessageBoxButtons.OK, MessageBoxIcon.Error);
                    Environment.Exit(0);
                }
            }
        }

        private static string GetLivesplitComponentsDirectory()
        {
            return Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
        }

        private void OnReset(object sender = null, TimerPhase value = 0)
        {
            Task.Run(() =>
            {
                lock (frameAnalyzationLock)
                {
                    // IsGameTimePaused is reset too, bring it back to true.
                    state.IsGameTimePaused = true;
                    FrameStorage.DeleteAllSavedFrames();
                    SonicVisualSplitWrapper.FrameAnalyzer.FullReset();

                    gameTime = 0;
                    gameTimeOnSegmentStart = 0;
                    ingameTimerOnSegmentStart = 0;
                    firstFrameTimeOfSegment = 0;
                    previousResult = null;
                    isAfterSplit = false;
                    ingameTimerOnSplit = 0;
                    lastScoreScreenCheckFrameTime = 0;
                    ingameTimerOnLastScoreCheck = -1;
                    frameBeforeTransition = null;
                }
            });
        }

        private void Split()
        {
            RunOnUiThreadAsync(() => model.Split());
        }

        /* We execute all actions that can cause the UI thread to update asynchronously,
         * because if UI thread is waiting for frame analyzer thread to finish (StopAnalyzingFrames), this can cause a deadlock. */
        private void RunOnUiThreadAsync(Action action)
        {
            Control mainWindow = Application.OpenForms[0];
            mainWindow.BeginInvoke(action);
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
