using LiveSplit.Model;
using LiveSplit.UI.Components;
using System;
using System.Collections.Generic;
using LiveSplit.UI;
using System.Drawing;
using System.Windows.Forms;
using System.Xml;
using SonicVisualSplitWrapper;
using System.Threading;

namespace SonicVisualSplit
{
    class SonicVisualSplitComponent : IComponent, FrameAnalyzer.IResultConsumer
    {
        private StyledInfoTextComponent internalComponent;
        private LiveSplitState state;
        private SonicVisualSplitSettings settings;
        private FrameAnalyzer frameAnalyzer;
        private VideoSourcesManager videoSourcesManager;

        string IComponent.ComponentName => "SonicVisualSplit";

        public IDictionary<string, Action> ContextMenuControls { get; private set; }

        public bool Disposed { get; private set; } = false;

        private static Form mainForm;
        private static Thread mainThread;

        public SonicVisualSplitComponent(LiveSplitState state)
        {
            this.state = state;
            mainForm = state.Form;
            mainThread = Thread.CurrentThread;
            internalComponent = new StyledInfoTextComponent("Time on screen (SVS)", "Wait..");

            settings = new SonicVisualSplitSettings();
            frameAnalyzer = new FrameAnalyzer(state, settings);
            frameAnalyzer.AddResultConsumer(this);
            settings.SettingsChanged += OnSettingsChanged;

            videoSourcesManager = new VideoSourcesManager(settings);
            videoSourcesManager.StartScanningSources();

            ContextMenuControls = new Dictionary<string, Action>();
            ContextMenuControls.Add("Toggle practice mode (Ctrl+P)",
                () => { settings.TogglePracticeMode(); });
            mainForm.KeyDown += CheckPracticeModeKeyboardShortcut;
        }

        void IDisposable.Dispose()
        {
            mainForm.KeyDown -= CheckPracticeModeKeyboardShortcut;
            if (Disposed)
            {
                return;
            }
            Disposed = true;
            frameAnalyzer.Dispose();
            state.IsGameTimePaused = false;
            videoSourcesManager.StopScanningSources();
        }

        public bool VisualizeAnalysisResult => false;

        public void OnFrameAnalyzed(AnalysisResult result)
        {
            RunOnUiThreadAsync(() =>
            {
                if (settings.IsPracticeMode)
                {
                    return;
                }
                if (result.RecognizedTime)
                {
                    internalComponent.IsTime = true;
                    internalComponent.InformationValue = result.TimeString;
                }
                else
                {
                    internalComponent.IsTime = false;
                    if (result.ErrorReason == ErrorReasonEnum.VIDEO_DISCONNECTED)
                    {
                        internalComponent.InformationValue = "Video Disconnected";
                    }
                    else
                    {
                        internalComponent.InformationValue = "-";
                    }
                }
            });
        }

        private void OnSettingsChanged(object sender, EventArgs e)
        {
            internalComponent.IsTime = false;
            if (settings.IsPracticeMode)
            {
                internalComponent.InformationValue = "Practice mode";
            }
            else
            {
                internalComponent.InformationValue = "Wait..";
            }
        }

        /* We execute all actions that can cause the UI thread to update asynchronously
         * in order to avoid deadlocks. */
        public static void RunOnUiThreadAsync(Action action)
        {
            var currentThread = Thread.CurrentThread;
            if (currentThread.ManagedThreadId == mainThread.ManagedThreadId)
            {
                action();
            }
            else
            {
                if (CanRunOnUiThreadAsync())
                {
                    try
                    {
                        mainForm.BeginInvoke(action);
                    }
                    catch (InvalidOperationException)
                    {
                        // The form handle is already destroyed.
                    }
                }
            }
        }

        private void CheckPracticeModeKeyboardShortcut(object sender, KeyEventArgs e)
        {
            if (e.Control && e.KeyCode == Keys.P)
            {
                settings.TogglePracticeMode();
            }
        }

        public static bool CanRunOnUiThreadAsync()
        {
            return mainForm.IsHandleCreated;
        }

        // Boilerplate code below.

        void IComponent.Update(IInvalidator invalidator, LiveSplitState state, float width, float height, LayoutMode mode)
        {
            internalComponent.Update(invalidator, state, width, height, mode);
        }

        void IComponent.DrawHorizontal(Graphics g, LiveSplitState state, float height, Region clipRegion)
        {
            internalComponent.DrawHorizontal(g, state, height, clipRegion);
        }

        void IComponent.DrawVertical(Graphics g, LiveSplitState state, float width, Region clipRegion)
        {
            internalComponent.DrawVertical(g, state, width, clipRegion);
        }

        float IComponent.HorizontalWidth => internalComponent.HorizontalWidth;
        float IComponent.MinimumHeight => internalComponent.MinimumHeight;
        float IComponent.MinimumWidth => internalComponent.MinimumWidth;
        float IComponent.PaddingBottom => internalComponent.PaddingBottom;
        float IComponent.PaddingLeft => internalComponent.PaddingLeft;
        float IComponent.PaddingRight => internalComponent.PaddingRight;
        float IComponent.PaddingTop => internalComponent.PaddingTop;
        float IComponent.VerticalHeight => internalComponent.VerticalHeight;

        XmlNode IComponent.GetSettings(XmlDocument document)
        {
            return settings.GetSettings(document);
        }

        Control IComponent.GetSettingsControl(LayoutMode mode)
        {
            settings.Mode = mode;
            return settings;
        }

        void IComponent.SetSettings(XmlNode settings)
        {
            this.settings.SetSettings(settings);
        }
    }
}