using System;
using System.Windows.Forms;
using System.Xml;
using LiveSplit.UI;

namespace SonicVisualSplit
{
    public partial class SonicVisualSplitSettings : UserControl
    {
        public bool RGB { get; set; }
        public bool Stretched { get; set; }

        public event EventHandler SettingChanged;

        public SonicVisualSplitSettings()
        {
            InitializeComponent();

            RGB = false;
            Stretched = false;

            compositeButton.DataBindings.Add("Checked", this, "RGB", false, DataSourceUpdateMode.OnPropertyChanged).BindingComplete += OnSettingChanged;
            rgbButton.DataBindings.Add("Checked", this, "RGB", true, DataSourceUpdateMode.OnPropertyChanged).BindingComplete += OnSettingChanged;
            fourByThreeButton.DataBindings.Add("Checked", this, "Stretched", false, DataSourceUpdateMode.OnPropertyChanged).BindingComplete += OnSettingChanged;
            sixteenByNineButton.DataBindings.Add("Checked", this, "Stretched", true, DataSourceUpdateMode.OnPropertyChanged).BindingComplete += OnSettingChanged;
        }

        private void OnSettingChanged(object sender, BindingCompleteEventArgs e)
        {
            SettingChanged?.Invoke(this, e);
        }

        public LayoutMode Mode { get; internal set; }

        internal XmlNode GetSettings(XmlDocument document)
        {
            var parent = document.CreateElement("Settings");
            CreateSettingsNode(document, parent);
            return parent;
        }

        private int CreateSettingsNode(XmlDocument document, XmlElement parent)
        {
            return SettingsHelper.CreateSetting(document, parent, "Version", "0.1") ^
                SettingsHelper.CreateSetting(document, parent, "RGB", RGB) ^
                SettingsHelper.CreateSetting(document, parent, "Stretched", Stretched);
        }

        internal void SetSettings(XmlNode settings)
        {
            RGB = SettingsHelper.ParseBool(settings["RGB"]);
            Stretched = SettingsHelper.ParseBool(settings["Stretched"]);
        }
    }
}
