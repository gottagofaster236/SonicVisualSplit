using System;
using System.Diagnostics;
using System.Drawing;
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

        public event EventHandler SettingsChanged;
        private bool disposed = false;

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
            if (Parent == null || !Parent.Visible)
                return false;
            try
            {
                // Calling Invoke to update the everything from UI thread.
                BeginInvoke((MethodInvoker)delegate
                {
                    gameCapturePreview.Image = result.VisualizedFrame;
                    string resultText = null;
                    LinkArea linkArea;
                    Color textColor;

                    if (result.ErrorReason == ErrorReasonEnum.VIDEO_DISCONNECTED)
                    {
                        resultText = "You have to open OBS with the game. Read more at this link";
                    }
                    else if (result.ErrorReason == ErrorReasonEnum.NO_TIME_ON_SCREEN)
                    {
                        resultText = "Can't recognize the time.";
                    }
                    else if (result.ErrorReason == ErrorReasonEnum.NO_ERROR)
                    {
                        if (result.IsBlackScreen)
                        {
                            resultText = "Black transition screen.";
                        }
                        else
                        {
                            resultText = $"Recognized time digits: {result.TimeDigits}.";
                            if (result.IsScoreScreen)
                                resultText += " Score screen (level completed).";
                        }
                    }

                    if (result.ErrorReason == ErrorReasonEnum.VIDEO_DISCONNECTED)
                        linkArea = new LinkArea(49, 9);
                    else
                        linkArea = new LinkArea(0, 0);

                    if (result.IsSuccessful())
                        textColor = Color.Green;
                    else
                        textColor = Color.Black;

                    if (recognitionResultsLabel.Text != resultText)
                    {
                        // Frequent updates break the LinkLabel. So we check if we actually have to update.
                        recognitionResultsLabel.Text = resultText;
                        recognitionResultsLabel.LinkArea = linkArea;
                        recognitionResultsLabel.ForeColor = textColor;
                    }
                });
            }
            catch (InvalidOperationException)
            {
                // We have a race condition (checking for parent visibility and THEN calling BeginInvoke).
                // Thus we may sometimes call BeginInvoke after the control was destroyed.
                return false;
            }
            return true;
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
            OnSettingsChanged();
        }

        private void OnAspectRatioChanged(object sender, EventArgs e)
        {
            Stretched = sixteenByNineButton.Checked;
            OnSettingsChanged();
        }

        private void OnSettingsChanged()
        {
            SettingsChanged?.Invoke(this, null);
        }
    }
}
