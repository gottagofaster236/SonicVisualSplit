using System;
using System.Diagnostics;
using System.Windows.Forms;
using System.Xml;
using LiveSplit.UI;
using SonicVisualSplitWrapper;

namespace SonicVisualSplit
{
    public partial class SonicVisualSplitSettings : UserControl, IFrameConsumer
    {
        public bool RGB { get; set; }
        public bool Stretched { get; set; }
        public bool SettingsLoaded { get; set; }

        public event EventHandler SettingChanged;

        public SonicVisualSplitSettings()
        {
            InitializeComponent();
            RGB = false;
            Stretched = false;
            SettingsLoaded = false;
        }

        protected override void OnParentChanged(EventArgs e)
        {
            Parent.VisibleChanged += OnVisibilityChanged;
            if (Parent.Visible)
                OnVisibilityChanged(null, null);
        }

        private void OnVisibilityChanged(object sender, EventArgs e)
        {
            Debug.WriteLine("Hello! parent visible: " + Parent.Visible);
            if (Parent.Visible)
            {
                AutoSplitter.AddFrameConsumer(this);
            }
            else
            {
                AutoSplitter.RemoveFrameConsumer(this);
            }
        }

        ~SonicVisualSplitSettings()
        {
            AutoSplitter.RemoveFrameConsumer(this);
        }

        public void OnFrameAnalyzed(AnalysisResult result)
        {
            Debug.WriteLine($"Frame arrived! parent visible: {Parent.Visible}");
            gameCapturePreview.Image = result.VisualizedFrame;
        }

        /*bool IsVisible()
        {
            bool visible = true;
            Control curNode = this;
            while (curNode != null)
            {
                if (!curNode.Visible)
                {
                    visible = false;
                    break;
                }
                curNode = curNode.Parent;
            }
            return visible;
        }*/


        public LayoutMode Mode { get; internal set; }

        public XmlNode GetSettings(XmlDocument document)
        {
            var parent = document.CreateElement("Settings");
            CreateSettingsNode(document, parent);
            return parent;
        }

        public int CreateSettingsNode(XmlDocument document, XmlElement parent)
        {
            return SettingsHelper.CreateSetting(document, parent, "Version", "0.1") ^
                SettingsHelper.CreateSetting(document, parent, "RGB", RGB) ^
                SettingsHelper.CreateSetting(document, parent, "Stretched", Stretched);
        }

        public void SetSettings(XmlNode settings)
        {
            RGB = SettingsHelper.ParseBool(settings["RGB"]);
            compositeButton.Checked = !RGB;
            rgbButton.Checked = RGB;

            Stretched = SettingsHelper.ParseBool(settings["Stretched"]);
            fourByThreeButton.Checked = !Stretched;
            sixteenByNineButton.Checked = Stretched;

            SettingsLoaded = true;
        }

        private void OnVideoConnectorChanged(object sender, EventArgs e)
        {
            RGB = rgbButton.Checked;
            OnSettingChanged();
        }

        private void OnAspectRatioChanged(object sender, EventArgs e)
        {
            Stretched = sixteenByNineButton.Checked;
            OnSettingChanged();
        }

        private void OnSettingChanged()
        {
            SettingChanged?.Invoke(this, null);
        }
    }
}
