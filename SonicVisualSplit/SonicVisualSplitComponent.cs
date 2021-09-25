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
        private InfoTextComponent internalComponent;
        private LiveSplitState state;
        private SonicVisualSplitSettings settings;
        private FrameAnalyzer frameAnalyzer;
        private VideoSourcesManager videoSourcesManager;

        string IComponent.ComponentName => "SonicVisualSplit";

        public IDictionary<string, Action> ContextMenuControls { get; private set; }

        public bool Disposed { get; private set; } = false;

        private static Thread mainThread;
        
        public SonicVisualSplitComponent(LiveSplitState state)
        {
            this.state = state;
            mainThread = Thread.CurrentThread;
            internalComponent = new InfoTextComponent("Time on screen (SVS)", "Wait..");

            ContextMenuControls = new Dictionary<string, Action>();
            ContextMenuControls.Add("Toggle practice mode",
                () => { settings.TogglePracticeMode(); });

            settings = new SonicVisualSplitSettings();
            frameAnalyzer = new FrameAnalyzer(state, settings);
            frameAnalyzer.AddResultConsumer(this);
            settings.SettingsChanged += OnSettingsChanged;

            videoSourcesManager = new VideoSourcesManager(settings);
            videoSourcesManager.StartScanningSources();
        }

        void IComponent.Update(IInvalidator invalidator, LiveSplitState state, float width, float height, LayoutMode mode)
        {
            internalComponent.Update(invalidator, state, width, height, mode);
        }

        void IDisposable.Dispose()
        {
            if (Disposed)
            {
                return;
            }
            Disposed = true;
            frameAnalyzer.Dispose();
            state.IsGameTimePaused = false;
            videoSourcesManager.StopScanningSources();
        }

        void IComponent.DrawHorizontal(Graphics g, LiveSplitState state, float height, Region clipRegion)
        {
            PrepareDraw(state, LayoutMode.Horizontal);
            internalComponent.DrawHorizontal(g, state, height, clipRegion);
        }

        void IComponent.DrawVertical(Graphics g, LiveSplitState state, float width, Region clipRegion)
        {
            internalComponent.PrepareDraw(state, LayoutMode.Vertical);
            PrepareDraw(state, LayoutMode.Vertical);
            internalComponent.DrawVertical(g, state, width, clipRegion);
        }

        void PrepareDraw(LiveSplitState state, LayoutMode mode)
        {
            internalComponent.NameLabel.ForeColor = state.LayoutSettings.TextColor;
            internalComponent.ValueLabel.ForeColor = state.LayoutSettings.TextColor;
            internalComponent.PrepareDraw(state, mode);
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
                    internalComponent.InformationValue = result.TimeString;
                }
                else
                {
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
                var mainWindow = GetMainWindow();
                mainWindow?.BeginInvoke(action);
            }
        }

        private static Control GetMainWindow()
        {
            var forms = Application.OpenForms;
            if (forms.Count == 0)
            {
                return null;
            }
            return forms[0];
        }
    }
}