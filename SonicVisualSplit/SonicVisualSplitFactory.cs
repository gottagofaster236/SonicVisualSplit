using LiveSplit.Model;
using LiveSplit.UI.Components;
using System;

[assembly: ComponentFactory(typeof(SonicVisualSplit.SonicVisualSplitFactory))]

namespace SonicVisualSplit
{
    class SonicVisualSplitFactory : IComponentFactory
    {
        public string ComponentName => "SonicVisualSplits";

        public string Description => "Auto-Splitter for people who play Sonic on console.";

        public ComponentCategory Category => ComponentCategory.Information;

        public IComponent Create(LiveSplitState state) => new PBChanceComponent(state);

        public string UpdateName => ComponentName;

        public string UpdateURL => "http://livesplit.org/update/";

        public Version Version => Version.Parse("0.1.0");

    }
}
