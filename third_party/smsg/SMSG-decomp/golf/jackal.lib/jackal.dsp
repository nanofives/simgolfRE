# Microsoft Developer Studio Project File - Name="jackal" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=jackal - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "jackal.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "jackal.mak" CFG="jackal - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "jackal - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "jackal - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /c
# ADD CPP /nologo /W3 /MT /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /GR- /Zl /I "include" /I "..\golf.exe\include" /YX /FD /c
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "jackal - Win32 Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\src\sprite.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\Heap.cpp
# End Source File
# Begin Source File

SOURCE=.\src\ScratchSprite.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\buffer.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\window.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\GraphicWin.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\FlicAnim.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\FlicSprite.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\palette.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\font.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\MessagePump.cpp
# End Source File
# Begin Source File

SOURCE=.\src\sound.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\EngineInit.cpp
# End Source File
# Begin Source File

SOURCE=.\src\PCXFile.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\EditBox.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\Time.cpp
# End Source File
# Begin Source File

SOURCE=.\src\BinkPlayer.cpp
# End Source File
# Begin Source File

SOURCE=.\src\SoundDevice.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\Text.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\MouseCursor.cpp
# End Source File
# Begin Source File

SOURCE=.\src\BaseButton.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\Dialog.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\ListBox.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\BasePop.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\FixedMath.cpp
# End Source File
# Begin Source File

SOURCE=.\src\DataPath.cpp
# End Source File
# Begin Source File

SOURCE=.\src\SpriteExtract.cpp
# End Source File
# Begin Source File

SOURCE=.\src\display.cpp
# End Source File
# Begin Source File

SOURCE=.\src\StringUtil.cpp
# End Source File
# Begin Source File

SOURCE=.\src\spot.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\filemap.cpp
# End Source File
# Begin Source File

SOURCE=.\src\PolyFill.cpp
# End Source File
# Begin Source File

SOURCE=.\src\MiscUtil.cpp
# End Source File
# Begin Source File

SOURCE=.\src\ComboBox.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\TextTemplate.cpp
# End Source File
# Begin Source File

SOURCE=.\src\DottedLine16.cpp
# End Source File
# Begin Source File

SOURCE=.\src\PCXLoad8.cpp
# End Source File
# Begin Source File

SOURCE=.\src\PCXLoad16.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\SysMessageHandler.cpp
# End Source File
# Begin Source File

SOURCE=.\src\AutoSound.cpp
# End Source File
# Begin Source File

SOURCE=.\src\scroll.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\Network.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\multidebug.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\PaletteList.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\Video.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\strings.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\SmallSprite.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\filewin.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\radiobut.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\CheckBoxSprites.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\checkbox.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\GraphsyLoader.cpp
# End Source File
# Begin Source File

SOURCE=.\src\DataFile.cpp
# End Source File
# Begin Source File

SOURCE=.\src\log.cpp
# End Source File
# Begin Source File

SOURCE=.\src\ButtonGroup.cpp
# End Source File
# Begin Source File

SOURCE=.\src\StringList.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\Dialogs.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\ImageButton.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\EditGroup.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\SpriteBox.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\PullDown.cpp
# End Source File
# Begin Source File

SOURCE=.\src\NetFifo.cpp
# End Source File
# Begin Source File

SOURCE=.\src\fifo.cpp
# End Source File
# Begin Source File

SOURCE=.\src\FrameStyle.cpp
# End Source File
# Begin Source File

SOURCE=.\src\FlatFrame.cpp
# End Source File
# End Group
# End Target
# End Project
