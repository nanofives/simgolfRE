# Microsoft Developer Studio Project File - Name="lpc10" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=lpc10 - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE
!MESSAGE NMAKE /f "lpc10.mak".
!MESSAGE
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE
!MESSAGE NMAKE /f "lpc10.mak" CFG="lpc10 - Win32 Release"
!MESSAGE
!MESSAGE Possible choices for configuration are:
!MESSAGE
!MESSAGE "lpc10 - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "lpc10 - Win32 Release"

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
# ADD CPP /nologo /MT /W3 /Od /ZI /GZ /D "WIN32" /D "NDEBUG" /D "_LIB" /GR- /Zl /I "lpc10\lpc10-1.5\lpc55-C" /FD /c
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

# Name "lpc10 - Win32 Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10\analys.c
# End Source File
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10\bsynz.c
# End Source File
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10\chanwr.c
# End Source File
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10\dcbias.c
# End Source File
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10\decode.c
# End Source File
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10\deemp.c
# End Source File
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10\difmag.c
# End Source File
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10\dyptrk.c
# End Source File
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10\encode.c
# End Source File
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10\energy.c
# End Source File
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10\f2clib.c
# End Source File
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10\ham84.c
# End Source File
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10\hp100.c
# End Source File
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10\invert.c
# End Source File
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10\irc2pc.c
# End Source File
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10\ivfilt.c
# End Source File
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10\lpcdec.c
# End Source File
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10\lpcenc.c
# End Source File
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10\lpcini.c
# End Source File
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10\lpfilt.c
# End Source File
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10\median.c
# End Source File
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10\mload.c
# End Source File
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10\onset.c
# End Source File
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10\pitsyn.c
# End Source File
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10\placea.c
# End Source File
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10\placev.c
# End Source File
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10\preemp.c
# End Source File
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10\prepro.c
# End Source File
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10\random.c
# End Source File
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10\rcchk.c
# End Source File
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10\synths.c
# End Source File
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10\tbdm.c
# End Source File
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10\voicin.c
# End Source File
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10\vparms.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10\f2c.h
# End Source File
# Begin Source File

SOURCE=.\lpc10\lpc10-1.5\lpc55-C\lpc10.h
# End Source File
# End Group
# End Target
# End Project
