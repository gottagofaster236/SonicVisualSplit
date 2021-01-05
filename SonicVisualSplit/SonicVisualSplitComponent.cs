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

namespace SonicVisualSplit
{
    class SonicVisualSplitComponent : IComponent
    {
        protected InfoTextComponent InternalComponent { get; set; }
        protected SonicVisualSplitSettings Settings { get; set; }
        protected LiveSplitState State;
        protected Random rand;
        protected string category;

        string IComponent.ComponentName => "SonicVisualSplit";

        IDictionary<string, Action> IComponent.ContextMenuControls => null;
        float IComponent.HorizontalWidth => InternalComponent.HorizontalWidth;
        float IComponent.MinimumHeight => InternalComponent.MinimumHeight;
        float IComponent.MinimumWidth => InternalComponent.MinimumWidth;
        float IComponent.PaddingBottom => InternalComponent.PaddingBottom;
        float IComponent.PaddingLeft => InternalComponent.PaddingLeft;
        float IComponent.PaddingRight => InternalComponent.PaddingRight;
        float IComponent.PaddingTop => InternalComponent.PaddingTop;
        float IComponent.VerticalHeight => InternalComponent.VerticalHeight;

        XmlNode IComponent.GetSettings(XmlDocument document)
        {
            return Settings.GetSettings(document);
        }

        Control IComponent.GetSettingsControl(LayoutMode mode)
        {
            Settings.Mode = mode;
            return Settings;
        }

        void IComponent.SetSettings(XmlNode settings)
        {
            Settings.SetSettings(settings);
        }

        public SonicVisualSplitComponent(LiveSplitState state)
        {
            State = state;
            InternalComponent = new InfoTextComponent("Hello", "World");
            Settings = new SonicVisualSplitSettings();
            Settings.SettingChanged += OnSettingChanged;
            rand = new Random();
            category = State.Run.GameName + State.Run.CategoryName;

            state.OnSplit += OnSplit;
            state.OnReset += OnReset;
            state.OnSkipSplit += OnSkipSplit;
            state.OnUndoSplit += OnUndoSplit;
            state.OnStart += OnStart;
            state.RunManuallyModified += OnRunManuallyModified;

            Recalculate();
        }

        private void OnRunManuallyModified(object sender, EventArgs e)
        {
            Recalculate();
        }

        private void OnSettingChanged(object sender, EventArgs e)
        {
            Recalculate();
        }

        private void OnStart(object sender, EventArgs e)
        {
            Recalculate();
        }

        protected void OnUndoSplit(object sender, EventArgs e)
        {
            Recalculate();
        }

        protected void OnSkipSplit(object sender, EventArgs e)
        {
            Recalculate();
        }

        protected void OnReset(object sender, TimerPhase value)
        {
            Recalculate();
        }

        protected void OnSplit(object sender, EventArgs e)
        {
            Recalculate();
        }

        protected void Recalculate()
        {
            // Get the current Personal Best, if it exists
            string text = "Hello youtube";
            InternalComponent.InformationValue = text;
        }

        void IComponent.DrawHorizontal(Graphics g, LiveSplitState state, float height, Region clipRegion)
        {
            PrepareDraw(state, LayoutMode.Horizontal);
            InternalComponent.DrawHorizontal(g, state, height, clipRegion);
        }

        void IComponent.DrawVertical(Graphics g, LiveSplitState state, float width, Region clipRegion)
        {
            InternalComponent.PrepareDraw(state, LayoutMode.Vertical);
            PrepareDraw(state, LayoutMode.Vertical);
            InternalComponent.DrawVertical(g, state, width, clipRegion);
        }

        void PrepareDraw(LiveSplitState state, LayoutMode mode)
        {
            InternalComponent.NameLabel.ForeColor = state.LayoutSettings.TextColor;
            InternalComponent.ValueLabel.ForeColor = state.LayoutSettings.TextColor;
            InternalComponent.PrepareDraw(state, mode);
        }

        void IComponent.Update(IInvalidator invalidator, LiveSplitState state, float width, float height, LayoutMode mode)
        {
            string newCategory = State.Run.GameName + State.Run.CategoryName;
            if (newCategory != category)
            {
                Recalculate();
                category = newCategory;
            }

            InternalComponent.Update(invalidator, state, width, height, mode);
        }

        void IDisposable.Dispose()
        {

        }
    }
}