
namespace SonicVisualSplit
{
    partial class SonicVisualSplitSettings
    {
        /// <summary> 
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary> 
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Component Designer generated code

        /// <summary> 
        /// Required method for Designer support - do not modify 
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.gameCapturePreview = new System.Windows.Forms.PictureBox();
            this.settingsLabel = new System.Windows.Forms.Label();
            this.gamesComboBox = new System.Windows.Forms.ComboBox();
            this.selectGameLabel = new System.Windows.Forms.Label();
            this.compositeButton = new System.Windows.Forms.RadioButton();
            this.rgbButton = new System.Windows.Forms.RadioButton();
            this.fourByThreeButton = new System.Windows.Forms.RadioButton();
            this.sixteenByNineButton = new System.Windows.Forms.RadioButton();
            this.videoConnectorGroup = new System.Windows.Forms.GroupBox();
            this.aspectRatioBox = new System.Windows.Forms.GroupBox();
            this.recognitionResultsLabel = new System.Windows.Forms.LinkLabel();
            this.togglePracticeModeButton = new System.Windows.Forms.Button();
            this.videoSourceLabel = new System.Windows.Forms.Label();
            this.videoSourceComboBox = new System.Windows.Forms.ComboBox();
            this.autoResetCheckbox = new System.Windows.Forms.CheckBox();
            this.copyrightLabel = new System.Windows.Forms.LinkLabel();
            this.timingMethodLabel = new System.Windows.Forms.Label();
            this.timingMethodComboBox = new System.Windows.Forms.ComboBox();
            ((System.ComponentModel.ISupportInitialize)(this.gameCapturePreview)).BeginInit();
            this.videoConnectorGroup.SuspendLayout();
            this.aspectRatioBox.SuspendLayout();
            this.SuspendLayout();
            // 
            // gameCapturePreview
            // 
            this.gameCapturePreview.BackColor = System.Drawing.Color.Black;
            this.gameCapturePreview.Location = new System.Drawing.Point(0, 98);
            this.gameCapturePreview.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.gameCapturePreview.Name = "gameCapturePreview";
            this.gameCapturePreview.Size = new System.Drawing.Size(632, 356);
            this.gameCapturePreview.SizeMode = System.Windows.Forms.PictureBoxSizeMode.Zoom;
            this.gameCapturePreview.TabIndex = 1;
            this.gameCapturePreview.TabStop = false;
            // 
            // settingsLabel
            // 
            this.settingsLabel.AutoSize = true;
            this.settingsLabel.Location = new System.Drawing.Point(16, 562);
            this.settingsLabel.Name = "settingsLabel";
            this.settingsLabel.Size = new System.Drawing.Size(126, 16);
            this.settingsLabel.TabIndex = 6;
            this.settingsLabel.Text = "Video input settings:";
            // 
            // gamesComboBox
            // 
            this.gamesComboBox.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.gamesComboBox.FormattingEnabled = true;
            this.gamesComboBox.Location = new System.Drawing.Point(113, 517);
            this.gamesComboBox.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.gamesComboBox.Name = "gamesComboBox";
            this.gamesComboBox.Size = new System.Drawing.Size(135, 24);
            this.gamesComboBox.TabIndex = 9;
            this.gamesComboBox.SelectedIndexChanged += new System.EventHandler(this.OnGameChanged);
            // 
            // selectGameLabel
            // 
            this.selectGameLabel.AutoSize = true;
            this.selectGameLabel.Location = new System.Drawing.Point(16, 521);
            this.selectGameLabel.Name = "selectGameLabel";
            this.selectGameLabel.Size = new System.Drawing.Size(86, 16);
            this.selectGameLabel.TabIndex = 10;
            this.selectGameLabel.Text = "Select game:";
            // 
            // compositeButton
            // 
            this.compositeButton.AutoSize = true;
            this.compositeButton.Checked = true;
            this.compositeButton.Location = new System.Drawing.Point(5, 21);
            this.compositeButton.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.compositeButton.Name = "compositeButton";
            this.compositeButton.Size = new System.Drawing.Size(115, 20);
            this.compositeButton.TabIndex = 0;
            this.compositeButton.TabStop = true;
            this.compositeButton.Text = "Composite/RF";
            this.compositeButton.UseVisualStyleBackColor = true;
            this.compositeButton.CheckedChanged += new System.EventHandler(this.OnVideoConnectorTypeChanged);
            // 
            // rgbButton
            // 
            this.rgbButton.AutoSize = true;
            this.rgbButton.Location = new System.Drawing.Point(5, 47);
            this.rgbButton.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.rgbButton.Name = "rgbButton";
            this.rgbButton.Size = new System.Drawing.Size(57, 20);
            this.rgbButton.TabIndex = 1;
            this.rgbButton.Text = "RGB";
            this.rgbButton.UseVisualStyleBackColor = true;
            this.rgbButton.CheckedChanged += new System.EventHandler(this.OnVideoConnectorTypeChanged);
            // 
            // fourByThreeButton
            // 
            this.fourByThreeButton.AutoSize = true;
            this.fourByThreeButton.Checked = true;
            this.fourByThreeButton.Location = new System.Drawing.Point(7, 21);
            this.fourByThreeButton.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.fourByThreeButton.Name = "fourByThreeButton";
            this.fourByThreeButton.Size = new System.Drawing.Size(97, 20);
            this.fourByThreeButton.TabIndex = 0;
            this.fourByThreeButton.TabStop = true;
            this.fourByThreeButton.Text = "4:3 (normal)";
            this.fourByThreeButton.UseVisualStyleBackColor = true;
            this.fourByThreeButton.CheckedChanged += new System.EventHandler(this.OnAspectRatioChanged);
            // 
            // sixteenByNineButton
            // 
            this.sixteenByNineButton.AutoSize = true;
            this.sixteenByNineButton.Location = new System.Drawing.Point(7, 47);
            this.sixteenByNineButton.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.sixteenByNineButton.Name = "sixteenByNineButton";
            this.sixteenByNineButton.Size = new System.Drawing.Size(118, 20);
            this.sixteenByNineButton.TabIndex = 1;
            this.sixteenByNineButton.Text = "16:9 (stretched)";
            this.sixteenByNineButton.UseVisualStyleBackColor = true;
            this.sixteenByNineButton.CheckedChanged += new System.EventHandler(this.OnAspectRatioChanged);
            // 
            // videoConnectorGroup
            // 
            this.videoConnectorGroup.Controls.Add(this.compositeButton);
            this.videoConnectorGroup.Controls.Add(this.rgbButton);
            this.videoConnectorGroup.Location = new System.Drawing.Point(157, 562);
            this.videoConnectorGroup.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.videoConnectorGroup.Name = "videoConnectorGroup";
            this.videoConnectorGroup.Padding = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.videoConnectorGroup.Size = new System.Drawing.Size(164, 75);
            this.videoConnectorGroup.TabIndex = 11;
            this.videoConnectorGroup.TabStop = false;
            this.videoConnectorGroup.Text = "Video connector";
            // 
            // aspectRatioBox
            // 
            this.aspectRatioBox.Controls.Add(this.fourByThreeButton);
            this.aspectRatioBox.Controls.Add(this.sixteenByNineButton);
            this.aspectRatioBox.Location = new System.Drawing.Point(340, 562);
            this.aspectRatioBox.Margin = new System.Windows.Forms.Padding(4);
            this.aspectRatioBox.Name = "aspectRatioBox";
            this.aspectRatioBox.Padding = new System.Windows.Forms.Padding(4);
            this.aspectRatioBox.Size = new System.Drawing.Size(172, 75);
            this.aspectRatioBox.TabIndex = 12;
            this.aspectRatioBox.TabStop = false;
            this.aspectRatioBox.Text = "Game aspect ratio";
            // 
            // recognitionResultsLabel
            // 
            this.recognitionResultsLabel.AutoSize = true;
            this.recognitionResultsLabel.Font = new System.Drawing.Font("Segoe UI", 11.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.recognitionResultsLabel.LinkArea = new System.Windows.Forms.LinkArea(0, 0);
            this.recognitionResultsLabel.Location = new System.Drawing.Point(16, 471);
            this.recognitionResultsLabel.Name = "recognitionResultsLabel";
            this.recognitionResultsLabel.Size = new System.Drawing.Size(280, 25);
            this.recognitionResultsLabel.TabIndex = 14;
            this.recognitionResultsLabel.Text = "Recognition results will go here.";
            this.recognitionResultsLabel.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.ShowHelp);
            // 
            // togglePracticeModeButton
            // 
            this.togglePracticeModeButton.Location = new System.Drawing.Point(203, 647);
            this.togglePracticeModeButton.Margin = new System.Windows.Forms.Padding(4);
            this.togglePracticeModeButton.Name = "togglePracticeModeButton";
            this.togglePracticeModeButton.Size = new System.Drawing.Size(157, 28);
            this.togglePracticeModeButton.TabIndex = 15;
            this.togglePracticeModeButton.Text = "Toggle practice mode";
            this.togglePracticeModeButton.UseVisualStyleBackColor = true;
            this.togglePracticeModeButton.Click += new System.EventHandler(this.TogglePracticeMode);
            // 
            // videoSourceLabel
            // 
            this.videoSourceLabel.AutoSize = true;
            this.videoSourceLabel.Location = new System.Drawing.Point(16, 60);
            this.videoSourceLabel.Name = "videoSourceLabel";
            this.videoSourceLabel.Size = new System.Drawing.Size(129, 16);
            this.videoSourceLabel.TabIndex = 17;
            this.videoSourceLabel.Text = "Select video source:";
            // 
            // videoSourceComboBox
            // 
            this.videoSourceComboBox.FormattingEnabled = true;
            this.videoSourceComboBox.Location = new System.Drawing.Point(160, 57);
            this.videoSourceComboBox.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.videoSourceComboBox.Name = "videoSourceComboBox";
            this.videoSourceComboBox.Size = new System.Drawing.Size(177, 24);
            this.videoSourceComboBox.TabIndex = 16;
            this.videoSourceComboBox.TextChanged += new System.EventHandler(this.OnVideoSourceChanged);
            // 
            // autoResetCheckbox
            // 
            this.autoResetCheckbox.AutoSize = true;
            this.autoResetCheckbox.Checked = true;
            this.autoResetCheckbox.CheckState = System.Windows.Forms.CheckState.Checked;
            this.autoResetCheckbox.Location = new System.Drawing.Point(16, 652);
            this.autoResetCheckbox.Margin = new System.Windows.Forms.Padding(4);
            this.autoResetCheckbox.Name = "autoResetCheckbox";
            this.autoResetCheckbox.Size = new System.Drawing.Size(166, 20);
            this.autoResetCheckbox.TabIndex = 18;
            this.autoResetCheckbox.Text = "Enable automatic reset";
            this.autoResetCheckbox.UseVisualStyleBackColor = true;
            this.autoResetCheckbox.CheckedChanged += new System.EventHandler(this.OnAutoResetEnabledChanged);
            // 
            // copyrightLabel
            // 
            this.copyrightLabel.AutoSize = true;
            this.copyrightLabel.LinkArea = new System.Windows.Forms.LinkArea(47, 11);
            this.copyrightLabel.Location = new System.Drawing.Point(16, 27);
            this.copyrightLabel.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.copyrightLabel.Name = "copyrightLabel";
            this.copyrightLabel.Size = new System.Drawing.Size(348, 20);
            this.copyrightLabel.TabIndex = 19;
            this.copyrightLabel.TabStop = true;
            this.copyrightLabel.Text = "SonicVisualSplit 1.3.0 by gottagofaster, 2026. GitHub link.";
            this.copyrightLabel.UseCompatibleTextRendering = true;
            this.copyrightLabel.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.OpenGithub);
            // 
            // timingMethodLabel
            // 
            this.timingMethodLabel.AutoSize = true;
            this.timingMethodLabel.Location = new System.Drawing.Point(265, 521);
            this.timingMethodLabel.Name = "timingMethodLabel";
            this.timingMethodLabel.Size = new System.Drawing.Size(99, 16);
            this.timingMethodLabel.TabIndex = 21;
            this.timingMethodLabel.Text = "Timing method:";
            // 
            // timingMethodComboBox
            // 
            this.timingMethodComboBox.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.timingMethodComboBox.FormattingEnabled = true;
            this.timingMethodComboBox.Location = new System.Drawing.Point(376, 517);
            this.timingMethodComboBox.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.timingMethodComboBox.Name = "timingMethodComboBox";
            this.timingMethodComboBox.Size = new System.Drawing.Size(135, 24);
            this.timingMethodComboBox.TabIndex = 20;
            this.timingMethodComboBox.SelectedIndexChanged += new System.EventHandler(this.OnTimingMethodChanged);
            // 
            // SonicVisualSplitSettings
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(8F, 16F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.Controls.Add(this.timingMethodLabel);
            this.Controls.Add(this.timingMethodComboBox);
            this.Controls.Add(this.copyrightLabel);
            this.Controls.Add(this.autoResetCheckbox);
            this.Controls.Add(this.videoSourceLabel);
            this.Controls.Add(this.videoSourceComboBox);
            this.Controls.Add(this.togglePracticeModeButton);
            this.Controls.Add(this.recognitionResultsLabel);
            this.Controls.Add(this.aspectRatioBox);
            this.Controls.Add(this.videoConnectorGroup);
            this.Controls.Add(this.selectGameLabel);
            this.Controls.Add(this.gamesComboBox);
            this.Controls.Add(this.settingsLabel);
            this.Controls.Add(this.gameCapturePreview);
            this.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.Name = "SonicVisualSplitSettings";
            this.Size = new System.Drawing.Size(632, 688);
            ((System.ComponentModel.ISupportInitialize)(this.gameCapturePreview)).EndInit();
            this.videoConnectorGroup.ResumeLayout(false);
            this.videoConnectorGroup.PerformLayout();
            this.aspectRatioBox.ResumeLayout(false);
            this.aspectRatioBox.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion
        private System.Windows.Forms.PictureBox gameCapturePreview;
        private System.Windows.Forms.Label settingsLabel;
        private System.Windows.Forms.ComboBox gamesComboBox;
        private System.Windows.Forms.Label selectGameLabel;
        private System.Windows.Forms.RadioButton compositeButton;
        private System.Windows.Forms.RadioButton rgbButton;
        private System.Windows.Forms.RadioButton fourByThreeButton;
        private System.Windows.Forms.RadioButton sixteenByNineButton;
        private System.Windows.Forms.GroupBox videoConnectorGroup;
        private System.Windows.Forms.GroupBox aspectRatioBox;
        private System.Windows.Forms.LinkLabel recognitionResultsLabel;
        private System.Windows.Forms.Button togglePracticeModeButton;
        private System.Windows.Forms.Label videoSourceLabel;
        private System.Windows.Forms.ComboBox videoSourceComboBox;
        private System.Windows.Forms.CheckBox autoResetCheckbox;
        private System.Windows.Forms.LinkLabel copyrightLabel;
        private System.Windows.Forms.Label timingMethodLabel;
        private System.Windows.Forms.ComboBox timingMethodComboBox;
    }
}
