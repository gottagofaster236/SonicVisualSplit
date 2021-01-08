
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
            this.livePreviewLabel = new System.Windows.Forms.Label();
            this.linkLabel1 = new System.Windows.Forms.LinkLabel();
            this.copyrightLabel = new System.Windows.Forms.Label();
            this.settingsLabel = new System.Windows.Forms.Label();
            this.gamesComboBox = new System.Windows.Forms.ComboBox();
            this.selectGameLabel = new System.Windows.Forms.Label();
            this.compositeButton = new System.Windows.Forms.RadioButton();
            this.rgbButton = new System.Windows.Forms.RadioButton();
            this.fourByThreeButton = new System.Windows.Forms.RadioButton();
            this.sixteenByNineButton = new System.Windows.Forms.RadioButton();
            this.videoConnectorGroup = new System.Windows.Forms.GroupBox();
            this.aspectRatioBox = new System.Windows.Forms.GroupBox();
            this.liveGamePreviewLabel = new System.Windows.Forms.Label();
            this.recognitionResultsLabel = new System.Windows.Forms.LinkLabel();
            ((System.ComponentModel.ISupportInitialize)(this.gameCapturePreview)).BeginInit();
            this.videoConnectorGroup.SuspendLayout();
            this.aspectRatioBox.SuspendLayout();
            this.SuspendLayout();
            // 
            // gameCapturePreview
            // 
            this.gameCapturePreview.BackColor = System.Drawing.Color.Black;
            this.gameCapturePreview.Location = new System.Drawing.Point(0, 85);
            this.gameCapturePreview.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.gameCapturePreview.Name = "gameCapturePreview";
            this.gameCapturePreview.Size = new System.Drawing.Size(632, 356);
            this.gameCapturePreview.SizeMode = System.Windows.Forms.PictureBoxSizeMode.Zoom;
            this.gameCapturePreview.TabIndex = 1;
            this.gameCapturePreview.TabStop = false;
            // 
            // livePreviewLabel
            // 
            this.livePreviewLabel.AutoSize = true;
            this.livePreviewLabel.Location = new System.Drawing.Point(13, 62);
            this.livePreviewLabel.Name = "livePreviewLabel";
            this.livePreviewLabel.Size = new System.Drawing.Size(0, 17);
            this.livePreviewLabel.TabIndex = 2;
            // 
            // linkLabel1
            // 
            this.linkLabel1.AutoSize = true;
            this.linkLabel1.Location = new System.Drawing.Point(16, 43);
            this.linkLabel1.Name = "linkLabel1";
            this.linkLabel1.Size = new System.Drawing.Size(0, 17);
            this.linkLabel1.TabIndex = 3;
            // 
            // copyrightLabel
            // 
            this.copyrightLabel.AutoSize = true;
            this.copyrightLabel.Location = new System.Drawing.Point(16, 27);
            this.copyrightLabel.Name = "copyrightLabel";
            this.copyrightLabel.Size = new System.Drawing.Size(259, 17);
            this.copyrightLabel.TabIndex = 4;
            this.copyrightLabel.Text = "SonicVisualSplit by gottagofaster, 2021.";
            // 
            // settingsLabel
            // 
            this.settingsLabel.AutoSize = true;
            this.settingsLabel.Location = new System.Drawing.Point(16, 564);
            this.settingsLabel.Name = "settingsLabel";
            this.settingsLabel.Size = new System.Drawing.Size(136, 17);
            this.settingsLabel.TabIndex = 6;
            this.settingsLabel.Text = "Video input settings:";
            // 
            // gamesComboBox
            // 
            this.gamesComboBox.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.gamesComboBox.FormattingEnabled = true;
            this.gamesComboBox.Items.AddRange(new object[] {
            "Sonic 1",
            "Sonic 2",
            "Sonic 3 & Knuckles",
            "Sonic CD",
            "Knuckles Chaotix"});
            this.gamesComboBox.Location = new System.Drawing.Point(112, 506);
            this.gamesComboBox.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.gamesComboBox.Name = "gamesComboBox";
            this.gamesComboBox.Size = new System.Drawing.Size(121, 24);
            this.gamesComboBox.TabIndex = 9;
            // 
            // selectGameLabel
            // 
            this.selectGameLabel.AutoSize = true;
            this.selectGameLabel.Location = new System.Drawing.Point(16, 510);
            this.selectGameLabel.Name = "selectGameLabel";
            this.selectGameLabel.Size = new System.Drawing.Size(90, 17);
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
            this.compositeButton.Size = new System.Drawing.Size(117, 21);
            this.compositeButton.TabIndex = 0;
            this.compositeButton.TabStop = true;
            this.compositeButton.Text = "Composite/RF";
            this.compositeButton.UseVisualStyleBackColor = true;
            this.compositeButton.CheckedChanged += new System.EventHandler(this.OnVideoConnectorChanged);
            // 
            // rgbButton
            // 
            this.rgbButton.AutoSize = true;
            this.rgbButton.Location = new System.Drawing.Point(5, 47);
            this.rgbButton.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.rgbButton.Name = "rgbButton";
            this.rgbButton.Size = new System.Drawing.Size(59, 21);
            this.rgbButton.TabIndex = 1;
            this.rgbButton.Text = "RGB";
            this.rgbButton.UseVisualStyleBackColor = true;
            this.rgbButton.CheckedChanged += new System.EventHandler(this.OnVideoConnectorChanged);
            // 
            // fourByThreeButton
            // 
            this.fourByThreeButton.AutoSize = true;
            this.fourByThreeButton.Checked = true;
            this.fourByThreeButton.Location = new System.Drawing.Point(7, 21);
            this.fourByThreeButton.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.fourByThreeButton.Name = "fourByThreeButton";
            this.fourByThreeButton.Size = new System.Drawing.Size(106, 21);
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
            this.sixteenByNineButton.Size = new System.Drawing.Size(130, 21);
            this.sixteenByNineButton.TabIndex = 1;
            this.sixteenByNineButton.Text = "16:9 (stretched)";
            this.sixteenByNineButton.UseVisualStyleBackColor = true;
            this.sixteenByNineButton.CheckedChanged += new System.EventHandler(this.OnAspectRatioChanged);
            // 
            // videoConnectorGroup
            // 
            this.videoConnectorGroup.Controls.Add(this.compositeButton);
            this.videoConnectorGroup.Controls.Add(this.rgbButton);
            this.videoConnectorGroup.Location = new System.Drawing.Point(157, 564);
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
            this.aspectRatioBox.Location = new System.Drawing.Point(340, 564);
            this.aspectRatioBox.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.aspectRatioBox.Name = "aspectRatioBox";
            this.aspectRatioBox.Padding = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.aspectRatioBox.Size = new System.Drawing.Size(172, 75);
            this.aspectRatioBox.TabIndex = 12;
            this.aspectRatioBox.TabStop = false;
            this.aspectRatioBox.Text = "Game aspect ratio";
            // 
            // liveGamePreviewLabel
            // 
            this.liveGamePreviewLabel.AutoSize = true;
            this.liveGamePreviewLabel.Location = new System.Drawing.Point(16, 53);
            this.liveGamePreviewLabel.Name = "liveGamePreviewLabel";
            this.liveGamePreviewLabel.Size = new System.Drawing.Size(129, 17);
            this.liveGamePreviewLabel.TabIndex = 13;
            this.liveGamePreviewLabel.Text = "Live game preview:";
            // 
            // recognitionResultsLabel
            // 
            this.recognitionResultsLabel.AutoSize = true;
            this.recognitionResultsLabel.Font = new System.Drawing.Font("Segoe UI", 10.2F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.recognitionResultsLabel.LinkArea = new System.Windows.Forms.LinkArea(0, 0);
            this.recognitionResultsLabel.Location = new System.Drawing.Point(16, 466);
            this.recognitionResultsLabel.Name = "recognitionResultsLabel";
            this.recognitionResultsLabel.Size = new System.Drawing.Size(248, 23);
            this.recognitionResultsLabel.TabIndex = 14;
            this.recognitionResultsLabel.Text = "Recognition results will go here";
            this.recognitionResultsLabel.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.OnRecognitionResultsLabelLinkClicked);
            // 
            // SonicVisualSplitSettings
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(8F, 16F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.Controls.Add(this.recognitionResultsLabel);
            this.Controls.Add(this.liveGamePreviewLabel);
            this.Controls.Add(this.aspectRatioBox);
            this.Controls.Add(this.videoConnectorGroup);
            this.Controls.Add(this.selectGameLabel);
            this.Controls.Add(this.gamesComboBox);
            this.Controls.Add(this.settingsLabel);
            this.Controls.Add(this.copyrightLabel);
            this.Controls.Add(this.linkLabel1);
            this.Controls.Add(this.livePreviewLabel);
            this.Controls.Add(this.gameCapturePreview);
            this.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.Name = "SonicVisualSplitSettings";
            this.Size = new System.Drawing.Size(632, 644);
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
        private System.Windows.Forms.Label livePreviewLabel;
        private System.Windows.Forms.LinkLabel linkLabel1;
        private System.Windows.Forms.Label copyrightLabel;
        private System.Windows.Forms.Label settingsLabel;
        private System.Windows.Forms.ComboBox gamesComboBox;
        private System.Windows.Forms.Label selectGameLabel;
        private System.Windows.Forms.RadioButton compositeButton;
        private System.Windows.Forms.RadioButton rgbButton;
        private System.Windows.Forms.RadioButton fourByThreeButton;
        private System.Windows.Forms.RadioButton sixteenByNineButton;
        private System.Windows.Forms.GroupBox videoConnectorGroup;
        private System.Windows.Forms.GroupBox aspectRatioBox;
        private System.Windows.Forms.Label liveGamePreviewLabel;
        private System.Windows.Forms.LinkLabel recognitionResultsLabel;
    }
}
