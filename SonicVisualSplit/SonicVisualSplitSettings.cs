using System;
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

        public void OnFrameAnalyzed(AnalysisResult result)
        {
            Console.WriteLine("Frame analyzed!");
            gameCapturePreview.Image = result.VisualizedFrame;
        }
    }
}
