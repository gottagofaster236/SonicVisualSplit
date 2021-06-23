using LiveSplit.Model;
using LiveSplit.UI.Components;
using System;
using System.Reflection;

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
                {
                    ((IDisposable)openedComponent).Dispose();
                }
            }

            var newComponent = new SonicVisualSplitComponent(state);
            openedComponentRef.SetTarget(newComponent);
            return newComponent;
        }

        // Update code take from here https://github.com/fatalis/SourceSplit/

        public string UpdateName => ComponentName;

        public string UpdateURL => "https://raw.githubusercontent.com/gottagofaster236/SonicVisualSplit/autoupdate/";

        public string XMLURL => UpdateURL + "update.xml";

        public Version Version => Assembly.GetExecutingAssembly().GetName().Version;

    }
}
