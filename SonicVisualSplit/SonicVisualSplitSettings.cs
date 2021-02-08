using System;
using System.Diagnostics;
using System.Drawing;
using System.Windows.Forms;
using System.Xml;
using LiveSplit.UI;
using SonicVisualSplitWrapper;

namespace SonicVisualSplit
{
    public partial class SonicVisualSplitSettings : UserControl, FrameAnalyzer.Callback
    {
        public bool RGB { get; set; }
        public bool Stretched { get; set; }
        public string Game { get; set; }

        public event EventHandler SettingsChanged;
        public FrameAnalyzer FrameAnalyzer { get; set; }
        public bool VisualizeAnalysisResult => true;

        public SonicVisualSplitSettings()
        {
            InitializeComponent();
            Disposed += OnDisposed;

            // default values of the settings
            RGB = false;
            Stretched = false;
            Game = "Sonic 1";

            gamesComboBox.Items.AddRange(new string[] {
                "Sonic 1",
                "Sonic 2",
                "Sonic 3 & Knuckles",
                "Sonic CD",
                "Knuckles' Chaotix"});
            gamesComboBox.SelectedIndex = 0;
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
                SettingsHelper.CreateSetting(document, parent, "Stretched", Stretched) ^
                SettingsHelper.CreateSetting(document, parent, "Game", Game);
        }

        public void SetSettings(XmlNode settings)
        {
            RGB = SettingsHelper.ParseBool(settings["RGB"]);
            compositeButton.Checked = !RGB;
            rgbButton.Checked = RGB;

            Stretched = SettingsHelper.ParseBool(settings["Stretched"]);
            fourByThreeButton.Checked = !Stretched;
            sixteenByNineButton.Checked = Stretched;

            Game = SettingsHelper.ParseString(settings["Game"]);
            int gameIndex = gamesComboBox.FindStringExact(Game);
            if (gameIndex == -1)
            {
                // No such game found, the settings are from previous version?
                gameIndex = 0;
                Game = "Sonic 1";
            }
            gamesComboBox.SelectedIndex = gameIndex;
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

        private void OnGameChanged(object sender, EventArgs e)
        {
            Game = (string) gamesComboBox.SelectedItem;
            OnSettingsChanged();
        }

        private void OnSettingsChanged()
        {
            SettingsChanged?.Invoke(this, null);
        }

        // Code that shows the preview of digits recognition results.

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
                FrameAnalyzer.AddFrameConsumer(this);
            }
            else
            {
                FrameAnalyzer.RemoveFrameConsumer(this);
            }
        }

        private void OnDisposed(object sender, EventArgs e)
        {
            FrameAnalyzer.RemoveFrameConsumer(this);
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
                        resultText = "You have to open OBS with the game capture. Read more at this link";
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
                            resultText = $"Recognized time digits: {result.TimeString}.";
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

        private void ShowHelp(object sender, LinkLabelLinkClickedEventArgs e)
        {
            var startInfo = new ProcessStartInfo("http://www.google.com");
            Process.Start(startInfo);
        }
    }
}
