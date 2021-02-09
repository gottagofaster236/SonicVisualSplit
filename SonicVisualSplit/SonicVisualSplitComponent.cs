using LiveSplit.Model;
using LiveSplit.UI.Components;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using LiveSplit.UI;
using System.Drawing;
using System.Windows.Forms;
using System.Xml;
using SonicVisualSplitWrapper;


namespace SonicVisualSplit
{
    class SonicVisualSplitComponent : IComponent, FrameAnalyzer.IResultConsumer
    {
        private InfoTextComponent internalComponent;
        private SonicVisualSplitSettings settings;
        private LiveSplitState state;
        private FrameAnalyzer frameAnalyzer;

        string IComponent.ComponentName => "SonicVisualSplit";

        IDictionary<string, Action> IComponent.ContextMenuControls => null;
        
        public SonicVisualSplitComponent(LiveSplitState state)
        {
            this.state = state;
            internalComponent = new InfoTextComponent("Time on screen (SVS)", "Wait..");
            settings = new SonicVisualSplitSettings();

            frameAnalyzer = new FrameAnalyzer(state, settings);
            frameAnalyzer.AddResultConsumer(this);
            frameAnalyzer.StartAnalyzingFrames();
        }

        void IComponent.Update(IInvalidator invalidator, LiveSplitState state, float width, float height, LayoutMode mode)
        {
            internalComponent.Update(invalidator, state, width, height, mode);
        }

        void IDisposable.Dispose()
        {
            frameAnalyzer.StopAnalyzingFrames();
            state.IsGameTimePaused = false;
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

        public bool OnFrameAnalyzed(AnalysisResult result)
        {
            if (result.RecognizedTime)
            {
                internalComponent.InformationValue = result.TimeString;
            }
            else if (!result.IsSuccessful())
            {
                if (result.ErrorReason == ErrorReasonEnum.NO_TIME_ON_SCREEN)
                {
                    internalComponent.InformationValue = "-";
                }
                else if (result.ErrorReason == ErrorReasonEnum.VIDEO_DISCONNECTED)
                {
                    internalComponent.InformationValue = "OBS Disconnected";
                }
            }
            return true;
        }

        public bool VisualizeAnalysisResult => false;
    }
}