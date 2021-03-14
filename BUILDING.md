# Building
1. Install [Visual Studio 2019](https://visualstudio.microsoft.com/vs/).
2. Install [vcpkg](https://github.com/microsoft/vcpkg), integrate it with Visual Studio (see link).
3. Install OpenCV by opening the vcpkg installation folder in the command line and executing `vcpkg install opencv:x64-windows` (takes time to build).
4. Download [LiveSplit](https://livesplit.org/downloads/). Extract it to `C:/Program Files/LiveSplit` (the exact path is needed for the build script).
5. Open Visual Studio. Clone [LiveSplit repository](https://github.com/LiveSplit/LiveSplit) with Visual Studio **(not with git command line)**, then clone this repository (also with Visual Studio).
6. If you want, you can try to build the LiveSplit repository, but you'll have to overcome a few obstacles. Alternatively, you can:
⋅⋅* Copy `LiveSplit.Core.dll` from your LiveSplit installation (`C:/Program Files/LiveSplit`) to `%USERPROFILE%/source/repos/...`
