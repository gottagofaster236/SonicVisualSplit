using LiveSplit.Model;
using SonicVisualSplitWrapper;
using System;
using System.Drawing;
using static SonicVisualSplit.SonicVisualSplitComponent;

namespace SonicVisualSplit.RTA
{
    public class FrameAnalyzer : IDisposable, SonicVisualSplitWrapper.RTA.FrameAnalyzer.Callback
    {
        private LiveSplitState state;
        private ITimerModel model;
        private SonicVisualSplitSettings settings;

        private SonicVisualSplitWrapper.RTA.FrameAnalyzer nativeFrameAnalyzer;

        public FrameAnalyzer(LiveSplitState state, SonicVisualSplitSettings settings)
        {
            model = new TimerModel() { CurrentState = state };
            this.settings = settings;
            settings.SettingsChanged += OnSettingsChanged;
            this.state = state;
            StartObservingCurrentSplitIndex();
        }

        private void OnSettingsChanged(object sender, EventArgs e)
        {
            AnalysisSettings analysisSettings = settings.GetAnalysisSettings();
            if (!settings.IsPracticeMode && settings.TimingMethod != TimingMethod.IGT)
            {
                if (analysisSettings != nativeFrameAnalyzer?.Settings)
                {
                    nativeFrameAnalyzer?.Dispose();
                    nativeFrameAnalyzer = new SonicVisualSplitWrapper.RTA.FrameAnalyzer(analysisSettings, this);
                }
                state.CurrentTimingMethod = LiveSplit.Model.TimingMethod.RealTime;
            }
            else
            {
                nativeFrameAnalyzer?.Dispose();
                nativeFrameAnalyzer = null;
            }
        }

        public void Dispose()
        {
            StopObservingCurrentSplitIndex();
            nativeFrameAnalyzer?.Dispose();
        }

        public Rectangle? GameRect
        {
            get
            {
                if (settings.TimingMethod == TimingMethod.IGT) return null;
                var gameRect = nativeFrameAnalyzer?.GameRect;
                if (gameRect != null) return gameRect;
                return new Rectangle();  // Empty rectangle instead of null to signify it should be used, but not ready yet
            }
        }

        private void StartObservingCurrentSplitIndex()
        {
            state.OnSplit += UpdateCurrentSplitIndex;
            state.OnUndoSplit += UpdateCurrentSplitIndex;
            state.OnSkipSplit += UpdateCurrentSplitIndex;
            state.OnStart += UpdateCurrentSplitIndex;
            state.OnReset += UpdateCurrentSplitIndex;
            UpdateCurrentSplitIndex();
        }

        // See StartObservingCurrentSplitIndex().
        private void StopObservingCurrentSplitIndex()
        {
            state.OnSplit -= UpdateCurrentSplitIndex;
            state.OnUndoSplit -= UpdateCurrentSplitIndex;
            state.OnSkipSplit -= UpdateCurrentSplitIndex;
            state.OnStart -= UpdateCurrentSplitIndex;
            state.OnReset -= UpdateCurrentSplitIndex;
        }

        private void UpdateCurrentSplitIndex(object sender = null, EventArgs e = null)
        {
            nativeFrameAnalyzer?.ReportSplitIndex(state.CurrentSplitIndex);
        }

        private void UpdateCurrentSplitIndex(object sender, TimerPhase timerPhase)
        {
            UpdateCurrentSplitIndex();
        }

        public void StartTimer()
        {
            RunOnUiThreadAsync(() =>
            {
                model.Start();
            });
        }

        public void Split()
        {
            RunOnUiThreadAsync(() =>
            {
                model.Split();
            });
        }

        public void UndoSplit()
        {
            RunOnUiThreadAsync(() =>
            {
                model.UndoSplit();
            });
        }

        public void Reset()
        {
            RunOnUiThreadAsync(() =>
            {
                model.Reset();
            });
        }

        public void PauseTimer()
        {
            RunOnUiThreadAsync(() =>
            {
                if (model.CurrentState.CurrentPhase == TimerPhase.Running)
                {
                    model.Pause();
                }
            });
        }

        public void UnpauseTimer()
        {
            RunOnUiThreadAsync(() =>
            {
                if (model.CurrentState.CurrentPhase == TimerPhase.Paused)
                {
                    model.Pause();
                }
            });
        }
    }
}
