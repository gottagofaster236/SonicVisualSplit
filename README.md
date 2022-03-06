# SonicVisualSplit
An auto-splitter for classic Sonic games on Sega Genesis, which works from visual input.

Splits Sonic 1, 2, and CD using IGT *(in-game time)*.

## Component in action
![Component in action](https://user-images.githubusercontent.com/55288842/112757626-355c5300-8ff3-11eb-9f74-655326b7385b.png)

## System requirements
Required OS version is **Windows 10 64-bit** (2016's Anniversary Update or newer).

You have to have the latest version of [LiveSplit](http://livesplit.org/downloads/) installed.

If you want to record (or stream) your game footage, you have to have [OBS Studio](https://obsproject.com/) (or other streaming software) installed.

## Installation
1. Install the latest version of Visual C++ Redistributable from [here](https://aka.ms/vs/17/release/vc_redist.x64.exe).
2. Unpack the contents of [SVS.zip](https://github.com/gottagofaster236/SonicVisualSplit/releases/latest/download/SVS.zip)
into the Components directory of your LiveSplit installation.
4. Add the component: right-click LiveSplit, select "Edit layout...", press the big "+" button,
and find SonicVisualSplit under the "Control" category.
4. Then you'll have to setup your video capture to work with SonicVisualSplit.

## Setting up video capture
*Settings page screenshot:*

![Settings page](https://user-images.githubusercontent.com/55288842/135631892-c7b4b861-8318-428a-8927-f723d6daeddd.png)

- To open SonicVisualSplit settings, right-click LiveSplit, click "Edit layout...", click "Layout Settings" at the bottom. Select "SonicVisualSplit" in the top navigation bar.

- If you **don't need to record/stream your runs**, you can just select the your capture card from the video sources dropdown list
(provided that your video capture cards is detected as a webcam on your computer).

- If you **do want to share your runs**, then you'll have use OBS Studio
(or alternative streaming software, such as Streamlabs).
This is due to Windows not allowing two applications to use a webcam simultaneously.
You'll have to use one of the two methods to set everything up.

   - If you use OBS Studio, you can simply choose "OBS Window Capture" from the video sources dropdown list.
     SonicVisualSplit will get the video by capturing screenshots of the opened OBS Studio window.
     This method needs no setup, but may be less stable then the following one
     (and isn't recommended for Sonic CD).
     
   - Another method is to install the *VirtualCam plugin*.
     For OBS, download and install it from [here](https://github.com/Fenrirthviti/obs-virtual-cam/releases).
     Then enable it in Tools → VirtualCam.
     	
     For Streamlabs, you can read about Virtual Camera installation [here](https://blog.streamlabs.com/streamlabs-obs-now-supports-virtual-camera-9a4e464435c2).
     
     You have to start the virtual camera before using SonicVisualSplit, and select it in the video sources dropdown list.
     Thus, SonicVisualSplit will capture the virtual camera, while the actual capture card video will be recorded by your streaming software of choice.
 
    In order for SonicVisualSplit to recognize the time on screen correctly, you have to make sure
    that the **<ins>game capture takes at least 80% of the height</ins>** of your stream layout.
- Make sure your capture card outputs an acceptable picture.
   - If your capture card outputs an image that's too dark, you'll have to apply a color correction filter in your streaming software.
     Try increasing gamma/brightness.
   - Check that the aspect ratio of the game is either 4:3 or 16:9. If it's stretched, fix it with your streaming software.
- After you setup the video capture correctly, it should appear in the preview.
- Then you have to select the game,
choose the video connector that you use to connect your console to the capture card,
and change the aspect ratio if for some reason your capture card stretches the image.
- The settings are saved with layout.
So, click "yes" when LiveSplit will ask you whether you want to save the layout settings at exit.

## Practice mode
The component currently cannot tell whether you just want to practice or start an actual run.
For that in SVS there's *practice mode*. It temporarily disables the component, so that you can practice the game without the timer running.

To toggle the practice mode, press <kbd>Ctrl</kbd> + <kbd>P</kbd> (make sure LiveSplit is the focused window first).<br>
Alternatively, you can right-click LiveSplit, select "Control", and click "Toggle practice mode" in the bottom.

## Known limitations
- If you die on *Scrap Brain 3* in Sonic 1, or Sonic 2's *Sky Chase* and *Wing Fortress*, you'll have to _undo the split_ manually. The actual time will be correct regardless.
- In Sonic 2, when you hit the boss, the timer is flashing and SVS fails to recognize that.
This is fine, it'll recover soon after.
- You must have one split per each act, and the splits should start from the first act. In particular, SVS is (probably) not usable for multi-game runs.

## Troubleshooting
- If the component doesn't reset automatically, it's most likely due to colors on your capture being slightly off. Open up an emulator and see if you can adjust the brightness to the same level.

  Another reason may be that your aspect ratio is incorrect, make sure it's either 4:3 or 16:9.

- If the component fails to read the digits too often (when it shows a dash instead of a time),
check your settings.
Make sure you selected the correct video mode and the correct game.
Sometimes *Composite* may work better than *RGB*, even if your capture card is capturing in RGB.<br>
Note that occasional incorrectly recognized frames are fine, thanks to error detection.
- If the game preview shows a blank image on the settings page,
make sure the camera stream isn't used by another program.
- If your capture card is outputting a dark image, you should apply a color correction filter as described [here](#setting-up-video-capture).
- If you found a bug, please open an issue [here on GitHub](https://github.com/gottagofaster236/SonicVisualSplit/issues/new).
If it's an issue with time recognizing incorrectly, a video or a screenshot of the game
at the point where SVS fails would be appreciated.

## Questions / Suggestions
If you couldn't solve your problem or have questions or suggestions, feel free to post them using [GitHub Discussions](https://github.com/gottagofaster236/SonicVisualSplit/discussions). You can also message me [on Discord](https://discordapp.com/users/gottagofaster#3355).

## For developers
Contributions are welcome! You should probably read [BUILDING.md](BUILDING.md) on how to build the project.
