using LiveSplit.Model;
using LiveSplit.UI.Components;
using System;

[assembly: ComponentFactory(typeof(SonicVisualSplit.SonicVisualSplitFactory))]

namespace SonicVisualSplit
{
    class SonicVisualSplitFactory : IComponentFactory
    {
        static readonly WeakReference<SonicVisualSplitComponent> openedComponentRef = new WeakReference<SonicVisualSplitComponent>(null);

        public string ComponentName => "SonicVisualSplit";

        public string Description => "An auto-splitter for people who play classic Sonic on console.";

        public ComponentCategory Category => ComponentCategory.Control;

        public IComponent Create(LiveSplitState state)
        {
            /* Workaround a LiveSplit bug/feature when it creates a new component before deleting the previous instance,
             * if you load another LiveSplit layout. */
            SonicVisualSplitComponent openedComponent;
            if (openedComponentRef.TryGetTarget(out openedComponent))
            {
                if (!openedComponent.Disposed)
                    ((IDisposable) openedComponent).Dispose();
            }

            var newComponent = new SonicVisualSplitComponent(state);
            openedComponentRef.SetTarget(newComponent);
            return newComponent;
        }

        public string UpdateName => ComponentName;

        public string XMLURL => "http://livesplit.org/update/Components/update.SonicVisualSplit.xml";

        public string UpdateURL => "http://livesplit.org/update/";

        public Version Version => Version.Parse("1.0.0");

    }
}
