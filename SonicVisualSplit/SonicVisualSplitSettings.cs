using System;
using System.Diagnostics;
using System.Drawing;
using System.Windows.Forms;
using System.Xml;
using LiveSplit.UI;
using SonicVisualSplitWrapper;

namespace SonicVisualSplit
{
    public partial class SonicVisualSplitSettings : UserControl, FrameAnalyzer.IResultConsumer
    {
        public string VideoSource { get; private set; }
        public bool RGB { get; private set; }
        public bool Stretched { get; private set; }
        public string Game { get; private set; }
        public bool AutoResetEnabled { get; private set; }
        public bool IsPracticeMode { get; private set; }

        public event EventHandler SettingsChanged;

        public FrameAnalyzer FrameAnalyzer { get; set; }
        public bool VisualizeAnalysisResult => true;

        public VideoSourcesManager VideoSourcesManager { get; set; }

        public SonicVisualSplitSettings()
        {
            InitializeComponent();
            Disposed += OnDisposed;

            // Default values of the settings.
            RGB = false;
            Stretched = false;
            Game = "Sonic 1";
            AutoResetEnabled = true;

            gamesComboBox.Items.AddRange(new string[] {
                "Sonic 1",
                "Sonic 2",
                "Sonic CD"
            });
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
            return SettingsHelper.CreateSetting(document, parent, "Version", "1.0") ^
                SettingsHelper.CreateSetting(document, parent, "VideoSource", VideoSource) ^
                SettingsHelper.CreateSetting(document, parent, "RGB", RGB) ^
                SettingsHelper.CreateSetting(document, parent, "Stretched", Stretched) ^
                SettingsHelper.CreateSetting(document, parent, "Game", Game) ^
                SettingsHelper.CreateSetting(document, parent, "AutoResetEnabled", AutoResetEnabled);
        }

        public void SetSettings(XmlNode settings)
        {
            VideoSource = SettingsHelper.ParseString(settings["VideoSource"]);
            videoSourceComboBox.Text = VideoSource;

            RGB = SettingsHelper.ParseBool(settings["RGB"]);
            rgbButton.Checked = RGB;
            compositeButton.Checked = !RGB;

            Stretched = SettingsHelper.ParseBool(settings["Stretched"]);
            sixteenByNineButton.Checked = Stretched;
            fourByThreeButton.Checked = !Stretched;

            Game = SettingsHelper.ParseString(settings["Game"]);
            int gameIndex = gamesComboBox.FindStringExact(Game);
            if (gameIndex == -1)
            {
                // No such game found, the settings are from a previous version?
                gameIndex = 0;
                Game = "Sonic 1";
            }
            gamesComboBox.SelectedIndex = gameIndex;

            AutoResetEnabled = SettingsHelper.ParseBool(settings["AutoResetEnabled"], true);
            autoResetCheckbox.Checked = AutoResetEnabled;

            OnSettingsChanged();
        }

        public void OnVideoSourcesListUpdated()
        {
            if (!IsHandleCreated)
            {
                return;
            }
            try
            {
                // Calling BeginInvoke to update the everything from the UI thread.
                BeginInvoke((MethodInvoker)delegate
                {
                    videoSourceComboBox.Items.Clear();
                    videoSourceComboBox.Items.AddRange(VideoSourcesManager.VideoSources.ToArray());
                });
            }
            catch (InvalidOperationException) { }
        }

        protected override void OnHandleCreated(EventArgs e)
        {
            base.OnHandleCreated(e);
            OnVideoSourcesListUpdated();
        }

        private void OnVideoSourceChanged(object sender, EventArgs e)
        {
            VideoSource = videoSourceComboBox.Text;
            OnSettingsChanged();
        }

        private void OnVideoConnectorTypeChanged(object sender, EventArgs e)
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

        private void OnAutoResetEnabledChanged(object sender, EventArgs e)
        {
            AutoResetEnabled = autoResetCheckbox.Checked;
            OnSettingsChanged();
        }

        // Code that shows the preview of digits recognition results.

        protected override void OnParentChanged(EventArgs e)
        {
            if (Parent != null)
            {
                Parent.VisibleChanged += OnVisibilityChanged;
                if (Parent.Visible)
                {
                    OnVisibilityChanged();
                }
            }
        }

        private void OnVisibilityChanged(object sender = null, EventArgs e = null)
        {
            if (Parent.Visible)
            {
                FrameAnalyzer.AddResultConsumer(this);
            }
            else
            {
                FrameAnalyzer.RemoveResultConsumer(this);
            }
        }

        private void OnDisposed(object sender, EventArgs e)
        {
            FrameAnalyzer.RemoveResultConsumer(this);
        }

        public bool OnFrameAnalyzed(AnalysisResult result)
        {
            try {
                // Calling BeginInvoke to update the everything from the UI thread.
                BeginInvoke((MethodInvoker)delegate
                {
                    gameCapturePreview.Image = result.VisualizedFrame;
                    string resultText = null;
                    LinkArea linkArea;
                    Color textColor;

                    if (result.ErrorReason == ErrorReasonEnum.VIDEO_DISCONNECTED)
                    {
                        resultText = "Video disconnected. Read more at this link.";
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
                        else if (result.IsWhiteScreen)
                        {
                            resultText = "White transition screen.";
                        }
                        else
                        {
                            resultText = $"Recognized time digits: {result.TimeString}.";
                        }
                    }

                    if (result.ErrorReason == ErrorReasonEnum.VIDEO_DISCONNECTED)
                    {
                        linkArea = new LinkArea(33, 9);
                    }
                    else
                    {
                        linkArea = new LinkArea(0, 0);
                    }

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
                /* We have a race condition (checking for parent visibility and THEN calling BeginInvoke).
                 * Thus we may sometimes call BeginInvoke after the control was destroyed. */
                return false;
            }
            return true;
        }

        private void OnSettingsChanged()
        {
            SettingsChanged?.Invoke(this, null);
            CheckForPracticeMode();
        }

        private void CheckForPracticeMode()
        {
            if (IsPracticeMode)
            {
                recognitionResultsLabel.Text = "Practice mode. Turn it off to see preview";
                recognitionResultsLabel.LinkArea = new LinkArea(0, 0);
                recognitionResultsLabel.ForeColor = Color.Black;
            }
        }

        public void TogglePracticeMode(object sender = null, EventArgs e = null)
        {
            IsPracticeMode = !IsPracticeMode;
            OnSettingsChanged();
        }

        private void ShowHelp(object sender, LinkLabelLinkClickedEventArgs e)
        {
            var startInfo = new ProcessStartInfo("https://github.com/gottagofaster236/SonicVisualSplit#setting-up-video-capture");
            Process.Start(startInfo);
        }
    }
}
