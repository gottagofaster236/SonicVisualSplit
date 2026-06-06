using LiveSplit.Model;
using SonicVisualSplitWrapper;
using System;
using static SonicVisualSplit.SonicVisualSplitComponent;

namespace SonicVisualSplit.RTA
{
    public class FrameAnalyzer : IDisposable, SonicVisualSplitWrapper.RTA.FrameAnalyzer.TimerCallback
    {
        private ITimerModel model;
        private SonicVisualSplitSettings settings;

        private SonicVisualSplitWrapper.RTA.FrameAnalyzer nativeFrameAnalyzer;
        private object nativeFrameAnalyzerLock = new object();

        public FrameAnalyzer(LiveSplitState state, SonicVisualSplitSettings settings)
        {
            model = new TimerModel() { CurrentState = state };
            this.settings = settings;
            this.settings.SettingsChanged += OnSettingsChanged;
        }

        private void OnSettingsChanged(object sender, EventArgs e)
        {
            AnalysisSettings analysisSettings = settings.GetAnalysisSettings();
            lock (nativeFrameAnalyzerLock)
            {
                if (!settings.IsPracticeMode && settings.TimingMethod != TimingMethod.IGT)
                {
                    if (analysisSettings != nativeFrameAnalyzer?.Settings)
                    {
                        nativeFrameAnalyzer?.Dispose();
                        nativeFrameAnalyzer = new SonicVisualSplitWrapper.RTA.FrameAnalyzer(analysisSettings, this);
                    }
                }
                else
                {
                    nativeFrameAnalyzer?.Dispose();
                    nativeFrameAnalyzer = null;
                }
            }
        }

        public void Dispose()
        {
            nativeFrameAnalyzer?.Dispose();
        }

        public void Split()
        {
            RunOnUiThreadAsync(() =>
            {
                model.Split();
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
                model.Pause();
            });
        }

        public void UnpauseTimer()
        {
            RunOnUiThreadAsync(() =>
            {
                model.UndoAllPauses();
            });
        }
    }
}
