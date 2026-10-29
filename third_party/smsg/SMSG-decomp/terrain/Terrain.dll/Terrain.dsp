# Microsoft Developer Studio Project File - Name="Terrain" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=Terrain - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE
!MESSAGE NMAKE /f "Terrain.mak".
!MESSAGE
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE
!MESSAGE NMAKE /f "Terrain.mak" CFG="Terrain - Win32 Debug"
!MESSAGE
!MESSAGE Possible choices for configuration are:
!MESSAGE
!MESSAGE "Terrain - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Terrain - Win32 Debug"

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
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_USRDLL" /D "TERRAIN_EXPORTS" /GR- /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 glu32.lib opengl32.lib kernel32.lib user32.lib gdi32.lib libcmtd.lib /nologo /dll /debug /machine:I386 /def:"Terrain.def" /out:"Debug/Terrain.dll" /pdbtype:sept

!ENDIF

# Begin Target

# Name "Terrain - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\src\Bmp.cpp
# End Source File
# Begin Source File

SOURCE=.\src\FaceBlock.cpp
# End Source File
# Begin Source File

SOURCE=.\src\VertexArrays.cpp
# End Source File
# Begin Source File

SOURCE=.\src\Course.cpp
# End Source File
# Begin Source File

SOURCE=.\src\Terrain.cpp
# End Source File
# Begin Source File

SOURCE=.\src\Tile.cpp
# End Source File
# Begin Source File

SOURCE=.\Terrain.def
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# End Target
# End Project
