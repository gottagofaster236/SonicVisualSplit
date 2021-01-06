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
            if (Parent != null)
            {
                Parent.VisibleChanged += OnVisibilityChanged;
                if (Parent.Visible)
                    OnVisibilityChanged();
            }
        }

        private void OnVisibilityChanged(object sender = null, EventArgs e = null)
        {
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

        public bool OnFrameAnalyzed(AnalysisResult result)
        {
            Invoke((MethodInvoker)delegate
            {
                gameCapturePreview.Image = result.VisualizedFrame;

                if (result.ErrorReason == ErrorReasonEnum.VIDEO_DISCONNECTED)
                {
                    recognitionResultsLabel.Text = "You have to open OBS with the game. Read more at this link";
                    recognitionResultsLabel.LinkArea = new LinkArea(49, 9);
                }
                else
                {
                    recognitionResultsLabel.LinkArea = new LinkArea(0, 0);
                }

                if (result.ErrorReason == ErrorReasonEnum.NO_TIME_ON_SCREEN)
                {
                    recognitionResultsLabel.Text = "Can't recognize the time.";
                }
                else if (result.ErrorReason == ErrorReasonEnum.NO_ERROR)
                {
                    if (result.IsBlackScreen)
                    {
                        recognitionResultsLabel.Text = "Black transition screen.";
                    }
                    else
                    {
                        recognitionResultsLabel.Text = $"Recognized time digits: {result.TimeDigits}.";
                        if (result.IsScoreScreen)
                            recognitionResultsLabel.Text += " Score screen (level completed).";
                    }
                }
            });
            
            return Parent != null && Parent.Visible;
        }

        private void OnRecognitionResultsLabelLinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            var startInfo = new ProcessStartInfo("http://www.google.com");
            Process.Start(startInfo);
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
    }
}
