
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
            this.linkLabel2 = new System.Windows.Forms.LinkLabel();
            this.settingsLabel = new System.Windows.Forms.Label();
            this.gamesComboBox = new System.Windows.Forms.ComboBox();
            this.selectGameLabel = new System.Windows.Forms.Label();
            this.compositeButton = new System.Windows.Forms.RadioButton();
            this.rgbButton = new System.Windows.Forms.RadioButton();
            this.compositeOrRgbPanel = new System.Windows.Forms.Panel();
            this.fourByThreeButton = new System.Windows.Forms.RadioButton();
            this.sixteenByNineButton = new System.Windows.Forms.RadioButton();
            this.aspectRatioPanel = new System.Windows.Forms.Panel();
            ((System.ComponentModel.ISupportInitialize)(this.gameCapturePreview)).BeginInit();
            this.compositeOrRgbPanel.SuspendLayout();
            this.aspectRatioPanel.SuspendLayout();
            this.SuspendLayout();
            // 
            // gameCapturePreview
            // 
            this.gameCapturePreview.Location = new System.Drawing.Point(12, 69);
            this.gameCapturePreview.Margin = new System.Windows.Forms.Padding(2, 2, 2, 2);
            this.gameCapturePreview.Name = "gameCapturePreview";
            this.gameCapturePreview.Size = new System.Drawing.Size(430, 289);
            this.gameCapturePreview.TabIndex = 1;
            this.gameCapturePreview.TabStop = false;
            // 
            // livePreviewLabel
            // 
            this.livePreviewLabel.AutoSize = true;
            this.livePreviewLabel.Location = new System.Drawing.Point(10, 50);
            this.livePreviewLabel.Margin = new System.Windows.Forms.Padding(2, 0, 2, 0);
            this.livePreviewLabel.Name = "livePreviewLabel";
            this.livePreviewLabel.Size = new System.Drawing.Size(0, 13);
            this.livePreviewLabel.TabIndex = 2;
            // 
            // linkLabel1
            // 
            this.linkLabel1.AutoSize = true;
            this.linkLabel1.Location = new System.Drawing.Point(12, 35);
            this.linkLabel1.Margin = new System.Windows.Forms.Padding(2, 0, 2, 0);
            this.linkLabel1.Name = "linkLabel1";
            this.linkLabel1.Size = new System.Drawing.Size(0, 13);
            this.linkLabel1.TabIndex = 3;
            // 
            // copyrightLabel
            // 
            this.copyrightLabel.AutoSize = true;
            this.copyrightLabel.Location = new System.Drawing.Point(12, 22);
            this.copyrightLabel.Margin = new System.Windows.Forms.Padding(2, 0, 2, 0);
            this.copyrightLabel.Name = "copyrightLabel";
            this.copyrightLabel.Size = new System.Drawing.Size(194, 13);
            this.copyrightLabel.TabIndex = 4;
            this.copyrightLabel.Text = "SonicVisualSplit by gottagofaster, 2021.";
            // 
            // linkLabel2
            // 
            this.linkLabel2.AutoSize = true;
            this.linkLabel2.LinkArea = new System.Windows.Forms.LinkArea(69, 11);
            this.linkLabel2.Location = new System.Drawing.Point(12, 46);
            this.linkLabel2.Margin = new System.Windows.Forms.Padding(2, 0, 2, 0);
            this.linkLabel2.Name = "linkLabel2";
            this.linkLabel2.Size = new System.Drawing.Size(424, 17);
            this.linkLabel2.TabIndex = 5;
            this.linkLabel2.TabStop = true;
            this.linkLabel2.Text = "Game live preview: (if it\'s not showing, launch OBS. More details on the webpage)" +
    ".\r\n";
            this.linkLabel2.UseCompatibleTextRendering = true;
            // 
            // settingsLabel
            // 
            this.settingsLabel.AutoSize = true;
            this.settingsLabel.Location = new System.Drawing.Point(12, 430);
            this.settingsLabel.Margin = new System.Windows.Forms.Padding(2, 0, 2, 0);
            this.settingsLabel.Name = "settingsLabel";
            this.settingsLabel.Size = new System.Drawing.Size(102, 13);
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
            this.gamesComboBox.Location = new System.Drawing.Point(86, 378);
            this.gamesComboBox.Margin = new System.Windows.Forms.Padding(2, 2, 2, 2);
            this.gamesComboBox.Name = "gamesComboBox";
            this.gamesComboBox.Size = new System.Drawing.Size(92, 21);
            this.gamesComboBox.TabIndex = 9;
            // 
            // selectGameLabel
            // 
            this.selectGameLabel.AutoSize = true;
            this.selectGameLabel.Location = new System.Drawing.Point(14, 380);
            this.selectGameLabel.Margin = new System.Windows.Forms.Padding(2, 0, 2, 0);
            this.selectGameLabel.Name = "selectGameLabel";
            this.selectGameLabel.Size = new System.Drawing.Size(69, 13);
            this.selectGameLabel.TabIndex = 10;
            this.selectGameLabel.Text = "Select game:";
            // 
            // compositeButton
            // 
            this.compositeButton.AutoCheck = false;
            this.compositeButton.AutoSize = true;
            this.compositeButton.Location = new System.Drawing.Point(2, 2);
            this.compositeButton.Margin = new System.Windows.Forms.Padding(2);
            this.compositeButton.Name = "compositeButton";
            this.compositeButton.Size = new System.Drawing.Size(93, 17);
            this.compositeButton.TabIndex = 0;
            this.compositeButton.Text = "Composite/RF";
            this.compositeButton.UseVisualStyleBackColor = true;
            // 
            // rgbButton
            // 
            this.rgbButton.Location = new System.Drawing.Point(3, 25);
            this.rgbButton.Margin = new System.Windows.Forms.Padding(2);
            this.rgbButton.Name = "rgbButton";
            this.rgbButton.Size = new System.Drawing.Size(48, 17);
            this.rgbButton.TabIndex = 1;
            this.rgbButton.Text = "RGB";
            this.rgbButton.UseVisualStyleBackColor = true;
            // 
            // compositeOrRgbPanel
            // 
            this.compositeOrRgbPanel.Controls.Add(this.rgbButton);
            this.compositeOrRgbPanel.Controls.Add(this.compositeButton);
            this.compositeOrRgbPanel.Location = new System.Drawing.Point(118, 426);
            this.compositeOrRgbPanel.Margin = new System.Windows.Forms.Padding(2);
            this.compositeOrRgbPanel.Name = "compositeOrRgbPanel";
            this.compositeOrRgbPanel.Size = new System.Drawing.Size(98, 46);
            this.compositeOrRgbPanel.TabIndex = 7;
            // 
            // fourByThreeButton
            // 
            this.fourByThreeButton.AutoSize = true;
            this.fourByThreeButton.Location = new System.Drawing.Point(2, 2);
            this.fourByThreeButton.Margin = new System.Windows.Forms.Padding(2);
            this.fourByThreeButton.Name = "fourByThreeButton";
            this.fourByThreeButton.Size = new System.Drawing.Size(80, 17);
            this.fourByThreeButton.TabIndex = 0;
            this.fourByThreeButton.Text = "4:3 (normal)";
            this.fourByThreeButton.UseVisualStyleBackColor = true;
            // 
            // sixteenByNineButton
            // 
            this.sixteenByNineButton.AutoCheck = false;
            this.sixteenByNineButton.AutoSize = true;
            this.sixteenByNineButton.Location = new System.Drawing.Point(3, 25);
            this.sixteenByNineButton.Margin = new System.Windows.Forms.Padding(2);
            this.sixteenByNineButton.Name = "sixteenByNineButton";
            this.sixteenByNineButton.Size = new System.Drawing.Size(107, 17);
            this.sixteenByNineButton.TabIndex = 1;
            this.sixteenByNineButton.Text = "Stretched to 16:9";
            this.sixteenByNineButton.UseVisualStyleBackColor = true;
            // 
            // aspectRatioPanel
            // 
            this.aspectRatioPanel.Controls.Add(this.sixteenByNineButton);
            this.aspectRatioPanel.Controls.Add(this.fourByThreeButton);
            this.aspectRatioPanel.Location = new System.Drawing.Point(235, 426);
            this.aspectRatioPanel.Margin = new System.Windows.Forms.Padding(2);
            this.aspectRatioPanel.Name = "aspectRatioPanel";
            this.aspectRatioPanel.Size = new System.Drawing.Size(124, 46);
            this.aspectRatioPanel.TabIndex = 8;
            // 
            // SonicVisualSplitSettings
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.Controls.Add(this.selectGameLabel);
            this.Controls.Add(this.gamesComboBox);
            this.Controls.Add(this.aspectRatioPanel);
            this.Controls.Add(this.compositeOrRgbPanel);
            this.Controls.Add(this.settingsLabel);
            this.Controls.Add(this.linkLabel2);
            this.Controls.Add(this.copyrightLabel);
            this.Controls.Add(this.linkLabel1);
            this.Controls.Add(this.livePreviewLabel);
            this.Controls.Add(this.gameCapturePreview);
            this.Margin = new System.Windows.Forms.Padding(2, 2, 2, 2);
            this.Name = "SonicVisualSplitSettings";
            this.Size = new System.Drawing.Size(478, 486);
            ((System.ComponentModel.ISupportInitialize)(this.gameCapturePreview)).EndInit();
            this.compositeOrRgbPanel.ResumeLayout(false);
            this.compositeOrRgbPanel.PerformLayout();
            this.aspectRatioPanel.ResumeLayout(false);
            this.aspectRatioPanel.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion
        private System.Windows.Forms.PictureBox gameCapturePreview;
        private System.Windows.Forms.Label livePreviewLabel;
        private System.Windows.Forms.LinkLabel linkLabel1;
        private System.Windows.Forms.Label copyrightLabel;
        private System.Windows.Forms.LinkLabel linkLabel2;
        private System.Windows.Forms.Label settingsLabel;
        private System.Windows.Forms.ComboBox gamesComboBox;
        private System.Windows.Forms.Label selectGameLabel;
        private System.Windows.Forms.RadioButton compositeButton;
        private System.Windows.Forms.RadioButton rgbButton;
        private System.Windows.Forms.Panel compositeOrRgbPanel;
        private System.Windows.Forms.RadioButton fourByThreeButton;
        private System.Windows.Forms.Panel aspectRatioPanel;
        private System.Windows.Forms.RadioButton sixteenByNineButton;
    }
}
