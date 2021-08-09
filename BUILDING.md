# Building
1. Install [Visual Studio 2022](https://visualstudio.microsoft.com/vs/preview/vs2022/).
2. Install [vcpkg](https://github.com/microsoft/vcpkg), integrate it with Visual Studio (see the link).
3. Install OpenCV: open the folder where you installed vcpkg in the command line, and execute `vcpkg install opencv:x64-windows` (takes time to build).
in order to commit spaces instead of tabs.
4. Download [LiveSplit](https://livesplit.org/downloads/). Extract it to `C:/Program Files/LiveSplit` (this exact path is needed for the build script).
5. Open Visual Studio **as administrator**. Clone the [LiveSplit repository](https://github.com/LiveSplit/LiveSplit) with Visual Studio **(not with git command line)**.
6. If you want, you can build the LiveSplit repository, but you'll have to overcome a few obstacles. Alternatively, here's what you can do.  
   Copy these three files: `LiveSplit.Core.dll`, `LiveSplit.View.dll` and `UpdateManager.dll` from your LiveSplit installation to these two folders:
   `%USERPROFILE%/source/repos/LiveSplit/LiveSplit/bin/Debug` and `%USERPROFILE%/source/repos/LiveSplit/LiveSplit/bin/Release`.
7. Then, clone [this](https://github.com/gottagofaster236/SonicVisualSplit) repository (also with Visual Studio).
8. In order for LiveSplit to be launched when you click the "Start" button in Visual Studio, you have to do the following.
In Visual Studio, open Solution Explorer (with Ctrl+Alt+L), right-click SonicVisualSplit project, select Properties. On the left pane, click "Debug". Select "All configurations" instead of "Active configuration" in the "Configuration:" dropdown list. Select "Start external program" action, and click "Browse".
Then select `C:\Program Files\LiveSplit\LiveSplit.exe`.
You also have to check the "Enable native code debugging" checkbox at the bottom, in order to debug the C++ part of the project.
Press `Ctrl+S` to save the changes.

   ![Start external program illustration](https://user-images.githubusercontent.com/55288842/111886772-65976680-89e1-11eb-9a7c-e9af55dd3b94.png)
9. Now you're good to go. When you'll click the "Start" button, the build script will copy the build results to `C:/Program Files/LiveSplit/Components`, and LiveSplit will start automatically. If you haven't already, you'll have to follow the steps from [README.md](README.md).
10. To build a new release of SonicVisualSplit, for now you can just copy the needed files from `%USERPROFILE%/source/repos/gottagofaster236/SonicVisualSplit/SonicVisualSplit/bin/x64/Release`
and wrap them in a zip archive.
