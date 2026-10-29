# Microsoft Developer Studio Project File - Name="jgld" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=jgld - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE
!MESSAGE NMAKE /f "jgld.mak".
!MESSAGE
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE
!MESSAGE NMAKE /f "jgld.mak" CFG="jgld - Win32 Debug"
!MESSAGE
!MESSAGE Possible choices for configuration are:
!MESSAGE
!MESSAGE "jgld - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library" 0x0102)
!MESSAGE

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "jgld - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /c
# ADD CPP /nologo /W3 /MTd /Od /ZI /GZ /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_USRDLL" /GR- /Zl /I "include" /YX /FD /c
# ADD BASE MTL /nologo /mktyplib203 /win32
# ADD MTL /nologo /mktyplib203 /win32
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib /nologo /machine:I386
# ADD LINK32 ..\libpng.lib\Debug\libpng.lib kernel32.lib user32.lib gdi32.lib libcmtd.lib ..\zlib.lib\Debug\zlib.lib /nologo /dll /debug /machine:I386 /def:"jgld.def" /out:"Debug/jgld.dll"

!ENDIF

# Begin Target

# Name "jgld - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\src\filemap.cpp
# End Source File
# Begin Source File

SOURCE=.\src\jglmath.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\gsobject.cpp
# ADD CPP /GF /GX
# End Source File
# Begin Source File

SOURCE=.\src\random.cpp
# End Source File
# Begin Source File

SOURCE=.\src\jglbuffer.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\jglrect.cpp
# End Source File
# Begin Source File

SOURCE=.\src\jglbufferalpha.cpp
# End Source File
# Begin Source File

SOURCE=.\src\jglbufferpcx.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\jglbufferblit8.cpp
# End Source File
# Begin Source File

SOURCE=.\src\jglwinmisc.cpp
# End Source File
# Begin Source File

SOURCE=.\src\jglbufferpng.cpp
# End Source File
# Begin Source File

SOURCE=.\src\jglsprite.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\jglspriteblend.cpp
# End Source File
# Begin Source File

SOURCE=.\src\jglsprite_16_16.cpp
# End Source File
# Begin Source File

SOURCE=.\src\jglspriteall.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\jglpalette.cpp
# End Source File
# Begin Source File

SOURCE=.\src\jglsprite_8_16c.cpp
# End Source File
# Begin Source File

SOURCE=.\src\jglsprite_8_8.cpp
# End Source File
# Begin Source File

SOURCE=.\src\jgl.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\jglfont.cpp
# ADD CPP /GF /GX
# End Source File
# Begin Source File

SOURCE=.\src\messagehandler.cpp
# End Source File
# Begin Source File

SOURCE=.\src\jglpalettemgr.cpp
# ADD CPP /GX
# End Source File
# Begin Source File

SOURCE=.\src\jglmsgpump.cpp
# End Source File
# Begin Source File

SOURCE=.\jgld.def
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# End Target
# End Project
