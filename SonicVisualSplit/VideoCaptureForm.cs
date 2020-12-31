using System;
using System.Diagnostics;
using System.Drawing;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace SonicVisualSplit
{
    public partial class VideoCaptureForm : Form
    {
        public VideoCaptureForm()
        {
            InitializeComponent();
            /*for (int i = 0; i < 100; i++)
                DigitsRecognizer.Test();*/
            Task.Run(() =>
            {
                while (true)
                {
                    try
                    {
                        var (result, runTime) = DigitsRecognizer.Test();
                        Invoke((MethodInvoker)delegate
                        {
                            cameraPictureBox.Image = result.VisualizedFrame;
                            Text = $"{runTime} ms; time is {result.TimeDigits}; success: {result.FoundAnyDigits}, " +
                            $"reason: {result.ErrorReason} is score: {result.IsScoreScreen}";
                            if (!result.FoundAnyDigits)
                                result.TimeDigits = "";
                            string labelText = $"Time: {result.TimeDigits}";
                            if (result.IsScoreScreen)
                                labelText += " Score!";
                            if (result.IsBlackScreen)
                                labelText += " Black!";
                            timeLabel.Text = labelText;
                            Update();
                        });
                    }
                    catch (Exception e)
                    {
                        Debug.WriteLine(e.ToString());
                    }
                }
            });
        }
    }
}
