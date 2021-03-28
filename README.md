# SonicVisualSplit
An auto-splitter for classic Sonic games on Sega Genesis, which works from visual input.

Splits Sonic 1, 2, and CD using IGT *(in-game time)*.

## Component in action
![Component in action](https://user-images.githubusercontent.com/55288842/112757626-355c5300-8ff3-11eb-9f74-655326b7385b.png)

## System requirements
Required OS version is **Windows 10 64-bit** (2016's Anniversary Update or newer).

If you want to record (or stream) your game footage, you have to have [OBS Studio](https://obsproject.com/) (or an another streaming software) installed.

## Installation
1. Unpack the contents of SVS.zip into the Components directory of your LiveSplit installation.
2. Add the component: right-click LiveSplit, select "Edit layout...", press the big "+" button,
and find SonicVisualSplit under the "Control" category.
3. Then you'll have to setup your video capture to work with SonicVisualSplit.

## Setting up video capture
*Settings page screenshot:*

![Settings page](https://user-images.githubusercontent.com/55288842/112758053-06df7780-8ff5-11eb-8591-b3b429a1fab2.png)

- To open SonicVisualSplit settings, right-click LiveSplit, click "Edit layout...", click "Layout Settings" at the bottom. Select "SonicVisualSplit" in the top navigation bar.

- If you **don't need to record/stream your runs**, you can just select the your capture card from the video sources dropdown list
(provided that your video capture cards is detected as a webcam on your computer).

- If you **do want to share your runs**, then you'll have use OBS Studio
(or an alternative streaming software, such as Streamlabs).
This is due to Windows not allowing two applications to use a webcam simultaneously.
You'll have to use one of the two methods to set everything up.

   - If you use OBS Studio, you can simply choose "OBS Window Capture" from the video sources dropdown list.
     SonicVisualSplit will get the video by capturing screenshots of the opened OBS Studio window.
     This method needs no setup, but may be less stable then the following one.
     
   - Another method is to install the *VirtualCam plugin*.
     In recent versions of OBS Studio, it's included already (or you can download it [here](https://obsproject.com/forum/resources/obs-virtualcam.949/)).
     For Streamlabs, you can read about Virtual Camera installation [here](https://blog.streamlabs.com/streamlabs-obs-now-supports-virtual-camera-9a4e464435c2).
     You have to start the virtual camera before using SonicVisualSplit, and select it in the video sources dropdown list.
   
     Thus, SonicVisualSplit will capture the virtual camera, while the actual capture card video will be recorded by your streaming software of choice.
 
    In order for SonicVisualSplit to recognize the time on screen correctly, you have to make sure
    that the **<u>game capture takes at least 80% of the height</u>** of your stream layout.
    
- After you've setup the video capture correctly, it should appear in the preview.
- Then you have to select the game,
choose the video connector that you use to connect your console to the capture card,
and change the aspect ratio if for some reason your capture card stretches the image.
- The settings are saved with layout.
So, click "yes" when LiveSplit will ask you whether you want to save the layout settings at exit.

## Troubleshooting
- If LiveSplit crashes on startup, run it as administrator.
- If the component has too many failed frames (when it writes a dash instead of a recognized time),
check your settings.
Make sure you've selected the correct video mode and the correct game.
Sometimes *Composite* may work better than *RGB*, even if your capture card is capturing in RGB.
- If your capture card outputs a too dark image, you'll have to apply a color correction filter (in your streaming software).
- If the game preview shows a blank screen on the settings page,
make sure the camera stream isn't used by another program.
- If you found a bug, please open an issue [here on GitHub](https://github.com/gottagofaster236/SonicVisualSplit/issues/new).
- If you couldn't solve your problem or have other questions, feel free to ask a question using [GitHub Discussions](https://github.com/gottagofaster236/SonicVisualSplit/discussions).

## For developers
Contributions are welcome! You should probably read [BUILDING.md](BUILDING.md) on how to build the project.