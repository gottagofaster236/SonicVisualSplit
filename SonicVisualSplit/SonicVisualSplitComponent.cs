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
using System.Runtime.InteropServices;

namespace SonicVisualSplit
{
    class SonicVisualSplitComponent : IComponent
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
            internalComponent = new InfoTextComponent("Hello", "World");

            settings = new SonicVisualSplitSettings();
            settings.SettingsChanged += OnSettingChanged;

            frameAnalyzer = new FrameAnalyzer(state, settings);
            frameAnalyzer.StartAnalyzingFrames();

            state.OnReset += OnReset;
            state.OnStart += OnStart;
        }

        private void OnSettingChanged(object sender, EventArgs e)
        {
            Recalculate();
        }

        private void OnStart(object sender, EventArgs e)
        {
            Recalculate();
        }

        protected void OnReset(object sender, TimerPhase value)
        {
            Recalculate();
        }

        protected void Recalculate()
        {
            string text = "Hello youtube";
            internalComponent.InformationValue = text;
        }

        void IComponent.Update(IInvalidator invalidator, LiveSplitState state, float width, float height, LayoutMode mode)
        {
            internalComponent.Update(invalidator, state, width, height, mode);
        }

        void IDisposable.Dispose()
        {

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
    }
}