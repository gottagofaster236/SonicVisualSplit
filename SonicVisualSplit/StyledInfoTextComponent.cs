using LiveSplit.Model;
using LiveSplit.UI;
using LiveSplit.UI.Components;

namespace SonicVisualSplit
{
    class StyledInfoTextComponent : InfoTextComponent
    {
        public bool IsTime { get; set; } = false;

        public StyledInfoTextComponent(string informationName, string informationValue) :
            base(informationName, informationValue) {}

        public override void PrepareDraw(LiveSplitState state, LayoutMode mode)
        {
            base.PrepareDraw(state, mode);
            ValueLabel.IsMonospaced = IsTime;
            ValueLabel.Font = IsTime ? state.LayoutSettings.TimesFont : state.LayoutSettings.TextFont;
            NameMeasureLabel.Font = state.LayoutSettings.TextFont;
            NameLabel.Font = state.LayoutSettings.TextFont;
            NameLabel.ForeColor = state.LayoutSettings.TextColor;
            ValueLabel.ForeColor = state.LayoutSettings.TextColor;
            NameLabel.HasShadow = state.LayoutSettings.DropShadows;
            ValueLabel.HasShadow = state.LayoutSettings.DropShadows;
        }
    }
}
