using System;
using System.Diagnostics;
using System.Drawing;
using System.Windows.Forms;
using System.Xml;
using LiveSplit.UI;
using SonicVisualSplitWrapper;
using static SonicVisualSplit.SonicVisualSplitComponent;
using System.Linq;

namespace SonicVisualSplit
{
    public enum TimingMethod : int
    {
        RTA_TB,
        IGT,
        COMBINED,
    }

    public partial class SonicVisualSplitSettings : UserControl, IGT.FrameAnalyzer.IResultConsumer
    {
        public string VideoSource { get; private set; }
        public bool RGB { get; private set; }
        public bool Stretched { get; private set; }
        public Game Game { get; private set; }
        public string GameString
        {
            get { return ToSettingsString(Game); }
        }
        public TimingMethod TimingMethod { get; private set; }
        public bool AutoResetEnabled { get; private set; }
        public bool IsPracticeMode { get; private set; }

        public event EventHandler SettingsChanged;

        public IGT.FrameAnalyzer FrameAnalyzer { get; set; }
        public bool VisualizeAnalysisResult => true;

        public VideoSourcesManager VideoSourcesManager { get; set; }

        public SonicVisualSplitSettings()
        {
            InitializeComponent();
            Disposed += OnDisposed;
            VisibleChanged += OnVisibleChanged;

            // Default values of the settings.
            RGB = false;
            Stretched = false;
            Game = Game.Sonic1;
            AutoResetEnabled = true;
            TimingMethod = TimingMethod.RTA_TB;

            gamesComboBox.Items.AddRange(
                ((Game[])Enum.GetValues(typeof(Game)))
                .Select(Game => ToSettingsString(Game)).ToArray());
            gamesComboBox.SelectedIndex = 0;

            timingMethodComboBox.Items.AddRange(
                ((TimingMethod[])Enum.GetValues(typeof(TimingMethod)))
                .Select(TimingMethod => ToSettingsString(TimingMethod)).ToArray());
            timingMethodComboBox.SelectedIndex = 0;
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
                SettingsHelper.CreateSetting(document, parent, "Game", ToSettingsString(Game)) ^
                SettingsHelper.CreateSetting(document, parent, "AutoResetEnabled", AutoResetEnabled) ^
                SettingsHelper.CreateSetting(document, parent, "TimingMethod", ToSettingsString(TimingMethod));
        }

        private static string ToSettingsString(Game game)
        {
            switch (game)
            {
                case Game.Sonic1:
                    return "Sonic 1";
                case Game.Sonic2:
                    return "Sonic 2";
                case Game.SonicCD:
                    return "Sonic CD";
                default:
                    throw new ArgumentOutOfRangeException();
            }
        }

        private static string ToSettingsString(TimingMethod timingMethod)
        {
            switch (timingMethod)
            {
                case TimingMethod.IGT:
                    return "IGT";
                case TimingMethod.RTA_TB:
                    return "RTA-TB";
                case TimingMethod.COMBINED:
                    return "RTA-TB & IGT";
                default:
                    throw new ArgumentOutOfRangeException();
            }
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

            string gameString = SettingsHelper.ParseString(settings["Game"]);
            int gameIndex = gamesComboBox.FindStringExact(gameString);
            if (gameIndex != -1)
            {
                gamesComboBox.SelectedIndex = gameIndex;
                Game = (Game)gameIndex;
            }

            string timingMethodString = SettingsHelper.ParseString(settings["TimingMethod"]);
            int timingMethodIndex = timingMethodComboBox.FindStringExact(timingMethodString);
            if (timingMethodIndex != -1)
            {
                TimingMethod = (TimingMethod)timingMethodIndex;
            }
            else
            {
                TimingMethod = TimingMethod.IGT;  // Keep the same value from before a timing method setting was added
            }
            timingMethodComboBox.SelectedIndex = (int) TimingMethod;

            AutoResetEnabled = SettingsHelper.ParseBool(settings["AutoResetEnabled"], true);
            autoResetCheckbox.Checked = AutoResetEnabled;

            OnSettingsChanged();
        }

        public void OnVideoSourcesListUpdated()
        {
            RunOnUiThreadAsync(() =>
            {
                videoSourceComboBox.Items.Clear();
                videoSourceComboBox.Items.AddRange(VideoSourcesManager.VideoSources.ToArray());
            });
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
            Game = (Game)gamesComboBox.SelectedIndex;
            OnSettingsChanged();
        }

        private void OnTimingMethodChanged(object sender, EventArgs e)
        {
            TimingMethod = (TimingMethod)timingMethodComboBox.SelectedIndex;
            OnSettingsChanged();
        }

        private void OnAutoResetEnabledChanged(object sender, EventArgs e)
        {
            AutoResetEnabled = autoResetCheckbox.Checked;
            OnSettingsChanged();
        }

        // Shows the preview of digits recognition results.
        public void OnFrameAnalyzed(SonicVisualSplitWrapper.IGT.AnalysisResult result)
        {
            RunOnUiThreadAsync(() =>
            {
                if (!UpdateFrameAnalyzerSubscription())
                {
                    return;
                }
                if (IsPracticeMode)
                {
                    return;
                }
                gameCapturePreview.Image = result.VisualizedFrame;
                string resultText = null;
                LinkArea linkArea = new LinkArea(0, 0);

                if (result.ErrorReason == SonicVisualSplitWrapper.IGT.ErrorReasonEnum.VIDEO_DISCONNECTED)
                {
                    resultText = "Video disconnected. Read more at this link.";
                    linkArea.Start = resultText.Length - 10;
                    linkArea.Length = 9;
                }
                else if (result.ErrorReason == SonicVisualSplitWrapper.IGT.ErrorReasonEnum.NO_TIME_ON_SCREEN)
                {
                    resultText = "Can't recognize the time.";
                }
                else if (result.ErrorReason == SonicVisualSplitWrapper.IGT.ErrorReasonEnum.NO_ERROR)
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

                Color textColor;
                if (result.IsSuccessful())
                    textColor = Color.Green;
                else
                    textColor = Color.Black;

                float fontSize = resultText.Contains("\n") ? 8.25F : 11.25F;
                Font font = new Font("Segoe UI", fontSize, FontStyle.Regular, GraphicsUnit.Point, 0);

                if (recognitionResultsLabel.Text != resultText)
                {
                    // Frequent updates break the LinkLabel. So we check if we actually have to update.
                    recognitionResultsLabel.Text = resultText;
                    recognitionResultsLabel.LinkArea = linkArea;
                    recognitionResultsLabel.ForeColor = textColor;
                    recognitionResultsLabel.Font = font;
                }
            });
        }

        private void OnDisposed(object sender, EventArgs e)
        {
            FrameAnalyzer.RemoveResultConsumer(this);
        }

        private void OnVisibleChanged(object sender, EventArgs e)
        {
            UpdateFrameAnalyzerSubscription();
        }

        private bool UpdateFrameAnalyzerSubscription()
        {
            if (Visible)
            {
                FrameAnalyzer.AddResultConsumer(this);
                return true;
            }
            else
            {
                FrameAnalyzer.RemoveResultConsumer(this);
                return false;
            }
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
            OpenWebsite("https://github.com/gottagofaster236/SonicVisualSplit#setting-up-video-capture");
        }

        private void OpenGithub(object sender, LinkLabelLinkClickedEventArgs e)
        {
            OpenWebsite("https://github.com/gottagofaster236/SonicVisualSplit");
        }

        private void OpenWebsite(string url)
        {
            var startInfo = new ProcessStartInfo(url);
            Process.Start(startInfo);
        }
    }
}
