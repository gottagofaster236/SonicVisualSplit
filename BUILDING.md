# Building
1. Install [Visual Studio 2026](https://visualstudio.microsoft.com/vs/) with .NET and C++ support (including vcpkg).
2. Download [LiveSplit](https://livesplit.org/downloads/). Extract it to `C:/Program Files/LiveSplit`.
3. Open Visual Studio **as administrator**. Clone the [LiveSplit repository](https://github.com/LiveSplit/LiveSplit) with Visual Studio **(not with git command line)**. The clone path should be exactly the following: `%USERPROFILE%\source\repos\LiveSplit`.
4. Then, clone [this](https://github.com/gottagofaster236/SonicVisualSplit) repository (also with Visual Studio). The clone path should be exactly: `%USERPROFILE%\source\repos\gottagofaster236\SonicVisualSplit`.
6. In order for LiveSplit to be launched when you click the "Start" button in Visual Studio, you have to do the following.
In Visual Studio, open Solution Explorer (with Ctrl+Alt+L), right-click SonicVisualSplit project, select Properties. On the left pane, click "Debug". Select "All configurations" instead of "Active configuration" in the "Configuration:" dropdown list. Select "Start external program" action, and click "Browse".
Then select `C:\Program Files\LiveSplit\LiveSplit.exe`.
You also have to check the "Enable native code debugging" checkbox at the bottom, in order to debug the C++ part of the project.
Press `Ctrl+S` to save the changes.

   ![Start external program illustration](https://user-images.githubusercontent.com/55288842/111886772-65976680-89e1-11eb-9a7c-e9af55dd3b94.png)
7. Now you're good to go. When you'll click the "Start" button, the build script will copy the build results to `C:/Program Files/LiveSplit/Components`, and LiveSplit will start automatically. If you haven't already, you'll have to follow the steps from [README.md](README.md).
8. To build a new release of SonicVisualSplit, for now you can just copy the needed files from `%USERPROFILE%/source/repos/gottagofaster236/SonicVisualSplit/SonicVisualSplit/bin/x64/Release`
and wrap them in a zip archive.

# Testing
As of now, testing has been done manually. You can download the test videos from [here](https://drive.google.com/drive/folders/111SibOuORifU7C0-G2OWzS94zCTtKkaV?usp=sharing)
and create an OBS scene for every one of them. Then you can run the videos sequentually through LiveSplit and verify that the timer is indeed timing them correctly.

I (gottagofaster) had wanted to automate the tests, but have since stopped actively working on the project.
