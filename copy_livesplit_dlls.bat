REM This file is needed for initial development environment setup.
REM Since the LiveSplit repository is difficult to build, as described here: https://github.com/LiveSplit/LiveSplit#common-compiling-issues
REM It is easier to simply run this script, provided that you have LiveSplit installed.
set LivesplitInstallationFolder=C:\Program Files\LiveSplit
set LivesplitRepositoryFolder=%USERPROFILE%\source\repos\LiveSplit\LiveSplit
xcopy /Y /I /F "%LivesplitInstallationFolder%\LiveSplit.Core.dll" "%LivesplitRepositoryFolder%\bin\Debug\"
xcopy /Y /I /F "%LivesplitInstallationFolder%\LiveSplit.Core.dll" "%LivesplitRepositoryFolder%\bin\Release\"
xcopy /Y /I /F "%LivesplitInstallationFolder%\LiveSplit.View.dll" "%LivesplitRepositoryFolder%\bin\Debug\"
xcopy /Y /I /F "%LivesplitInstallationFolder%\LiveSplit.View.dll" "%LivesplitRepositoryFolder%\bin\Release\"
xcopy /Y /I /F "%LivesplitInstallationFolder%\UpdateManager.dll" "%LivesplitRepositoryFolder%\bin\Debug\"
xcopy /Y /I /F "%LivesplitInstallationFolder%\UpdateManager.dll" "%LivesplitRepositoryFolder%\bin\Release\"
