# Microsoft Developer Studio Project File - Name="sound" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=sound - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE
!MESSAGE NMAKE /f "sound.mak".
!MESSAGE
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE
!MESSAGE NMAKE /f "sound.mak" CFG="sound - Win32 Release"
!MESSAGE
!MESSAGE Possible choices for configuration are:
!MESSAGE
!MESSAGE "sound - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "sound - Win32 Release"

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
# ADD BASE CPP /nologo /MT /W3 /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_USRDLL" /YX /FD /c
# ADD CPP /nologo /MT /W3 /O2 /I "include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_USRDLL" /GR- /Zl /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib ole32.lib winmm.lib dsound.lib dxguid.lib ..\lpc10.lib\Release\lpc10.lib libcimt.lib libcmt.lib /nologo /dll /machine:I386 /incremental:yes /def:"Sound.def" /out:"Release/sound.dll"

!ENDIF

# Begin Target

# Name "sound - Win32 Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\src\ambiencemgr.cpp
# ADD CPP /GX /Ob2 /YX
# End Source File
# Begin Source File

SOURCE=.\src\avimgr.cpp
# ADD CPP /GX /YX
# End Source File
# Begin Source File

SOURCE=.\src\Cmix.cpp
# ADD CPP /Ob2 /YX
# End Source File
# Begin Source File

SOURCE=.\src\CritReq.cpp
# ADD CPP /YX
# End Source File
# Begin Source File

SOURCE=.\src\CTimer.cpp
# ADD CPP /GX /YX
# End Source File
# Begin Source File

SOURCE=.\src\dllmain.cpp
# ADD CPP /GX /Ob2 /YX
# End Source File
# Begin Source File

SOURCE=.\src\dllmididevice.cpp
# ADD CPP /GX /Ob2 /YX
# End Source File
# Begin Source File

SOURCE=.\src\dlltrackset.cpp
# ADD CPP /YX
# End Source File
# Begin Source File

SOURCE=.\src\dllwavedevice.cpp
# ADD CPP /GX /Ob2 /YX
# End Source File
# Begin Source File

SOURCE=.\src\dllwaveindevice.cpp
# ADD CPP /GX /Ob2 /YX
# End Source File
# Begin Source File

SOURCE=.\src\dsb.cpp
# ADD CPP /Ob2 /YX
# End Source File
# Begin Source File

SOURCE=.\src\dsmgr.cpp
# ADD CPP /GX /Ob2 /YX
# End Source File
# Begin Source File

SOURCE=.\src\filters.cpp
# ADD CPP /YX
# End Source File
# Begin Source File

SOURCE=.\src\keymap.cpp
# ADD CPP /GX /Ob2 /YX
# End Source File
# Begin Source File

SOURCE=.\src\memtrack.cpp
# ADD CPP /GX /Ob2 /YX
# End Source File
# Begin Source File

SOURCE=.\src\miditools.cpp
# ADD CPP /GX /Ob2
# End Source File
# Begin Source File

SOURCE=.\src\midifile.cpp
# ADD CPP /GX /Ob2 /YX
# End Source File
# Begin Source File

SOURCE=.\src\midimgr.cpp
# ADD CPP /GX /Ob2 /YX
# End Source File
# Begin Source File

SOURCE=.\src\MMError.cpp
# ADD CPP /Ob2 /YX
# End Source File
# Begin Source File

SOURCE=.\src\mmxfilters.cpp
# ADD CPP /Ob2 /YX
# End Source File
# Begin Source File

SOURCE=.\src\mpeg.cpp
# ADD CPP /GX /YX
# End Source File
# Begin Source File

SOURCE=.\src\random.cpp
# ADD CPP /Ob2 /YX
# End Source File
# Begin Source File

SOURCE=.\src\SndWin.cpp
# ADD CPP /Ob2 /YX
# End Source File
# Begin Source File

SOURCE=.\src\sounddevice.cpp
# ADD CPP /GX /Ob2
# End Source File
# Begin Source File

SOURCE=.\src\soundthread.cpp
# ADD CPP /Ob2 /YX
# End Source File
# Begin Source File

SOURCE=.\src\sound.cpp
# ADD CPP /GX /Ob2
# End Source File
# Begin Source File

SOURCE=.\src\sound_pch.cpp
# ADD CPP /GX /YX
# End Source File
# Begin Source File

SOURCE=.\src\SoundList.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\trace.cpp
# ADD CPP /YX
# End Source File
# Begin Source File

SOURCE=.\src\VoiceRxMgr.cpp
# ADD CPP /GX /Ob2 /YX
# End Source File
# Begin Source File

SOURCE=.\src\VoiceTxMgr.cpp
# ADD CPP /GX /Ob2 /YX
# End Source File
# Begin Source File

SOURCE=.\src\wavefile.cpp
# ADD CPP /GX /Ob2 /YX
# End Source File
# Begin Source File

SOURCE=.\src\WaveInDeviceMgr.cpp
# ADD CPP /GX /Ob2 /YX
# End Source File
# Begin Source File

SOURCE=.\src\wavemgr.cpp
# ADD CPP /GX /Ob2 /YX
# End Source File
# Begin Source File

SOURCE=.\Sound.def
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# End Target
# End Project
