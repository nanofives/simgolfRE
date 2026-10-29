# Microsoft Developer Studio Project File - Name="golf" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=golf - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE
!MESSAGE NMAKE /f "golf.mak".
!MESSAGE
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE
!MESSAGE NMAKE /f "golf.mak" CFG="golf - Win32 Release"
!MESSAGE
!MESSAGE Possible choices for configuration are:
!MESSAGE
!MESSAGE "golf - Win32 Release" (based on "Win32 (x86) Application" 0x0101)
!MESSAGE

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "golf - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /c
# ADD CPP /nologo /W3 /MT /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /GR- /Zl /I "include" /I "..\jackal.lib\include" /YX /FD /c
# ADD BASE MTL /nologo /mktyplib203 /win32
# ADD MTL /nologo /mktyplib203 /win32
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib /nologo /machine:I386
# ADD LINK32 ..\jackal.lib\Release\jackal.lib binkw32.lib terrain.lib dplayx.lib dxguid.lib kernel32.lib user32.lib gdi32.lib winmm.lib vfw32.lib libcmt.lib oldnames.lib ..\jpeg.lib\Release\jpeg.lib /nologo /subsystem:windows /machine:I386 /def:"golf.def" /out:"Release/golf.exe"

!ENDIF

# Begin Target

# Name "golf - Win32 Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\src\Plot.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\Employee.cpp
# End Source File
# Begin Source File

SOURCE=.\src\EmployeeRender.cpp
# End Source File
# Begin Source File

SOURCE=.\src\FlicChannels.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\Globals.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\ScreenText.cpp
# End Source File
# Begin Source File

SOURCE=.\src\SpriteAssets.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\MainLoop.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\GolferSpawn.cpp
# End Source File
# Begin Source File

SOURCE=.\src\GolferShot.cpp
# End Source File
# Begin Source File

SOURCE=.\src\WildlifeUpdate.cpp
# End Source File
# Begin Source File

SOURCE=.\src\Toolbar.cpp
# End Source File
# Begin Source File

SOURCE=.\src\SetupScreens.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\FlicBanks.cpp
# ADD CPP /GX /Oa /Og /Oi /Ot /Oy /Ob1 /Gs /Gf /Gy
# End Source File
# Begin Source File

SOURCE=.\src\TerrainView.cpp
# ADD CPP /D TERRAIN_EXPORTS
# End Source File
# Begin Source File

SOURCE=.\src\InfoScreens.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\InputPoll.cpp
# End Source File
# Begin Source File

SOURCE=.\src\StringPool.cpp
# End Source File
# Begin Source File

SOURCE=.\src\GameMain.cpp
# End Source File
# Begin Source File

SOURCE=.\src\InputWait.cpp
# End Source File
# Begin Source File

SOURCE=.\src\GolferPanel.cpp
# End Source File
# Begin Source File

SOURCE=.\src\GolferPairing.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\TerrainInfluence.cpp
# End Source File
# Begin Source File

SOURCE=.\src\ShadeTable.cpp
# End Source File
# Begin Source File

SOURCE=.\src\RenderQueue.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\Screen.cpp
# End Source File
# Begin Source File

SOURCE=.\src\ThemeLoader.cpp
# End Source File
# Begin Source File

SOURCE=.\src\Golfer.cpp
# End Source File
# Begin Source File

SOURCE=.\src\Geometry.cpp
# End Source File
# Begin Source File

SOURCE=.\src\GolferDialogue.cpp
# End Source File
# Begin Source File

SOURCE=.\src\Tournament.cpp
# End Source File
# Begin Source File

SOURCE=.\src\ListDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\src\WorldMap.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\jgldefs.cpp
# End Source File
# Begin Source File

SOURCE=.\golf.def
# End Source File
# Begin Source File

SOURCE=.\res\golf.rc
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\res\resource.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\res\golf.ico
# End Source File
# End Group
# End Target
# End Project
