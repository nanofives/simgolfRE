# Microsoft Developer Studio Project File - Name="jpeg" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=jpeg - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "jpeg.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "jpeg.mak" CFG="jpeg - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "jpeg - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "jpeg - Win32 Release"

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
# ADD CPP /nologo /W3 /MT /O2 /D "WIN32" /D "NDEBUG" /D "_LIB" /GR- /Zl /I "." /I "jpeg-6a" /FD /c
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

# Name "jpeg - Win32 Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\CreateJPG.c
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JCAPIMIN.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JCAPISTD.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JCCOEFCT.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JCCOLOR.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JCDCTMGR.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JCHUFF.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JCINIT.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JCMAINCT.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JCMARKER.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JCMASTER.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JCOMAPI.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JCPARAM.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JCPHUFF.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JCPREPCT.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JCSAMPLE.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JCTRANS.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JDAPIMIN.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JDAPISTD.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JDATADST.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JDATASRC.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JDCOEFCT.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JDCOLOR.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JDDCTMGR.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JDHUFF.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JDINPUT.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JDMAINCT.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JDMARKER.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JDMASTER.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JDMERGE.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JDPHUFF.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JDPOSTCT.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JDSAMPLE.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JDTRANS.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JERROR.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JFDCTFLT.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JFDCTFST.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JFDCTINT.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JIDCTFLT.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JIDCTFST.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JIDCTINT.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JIDCTRED.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JMEMMGR.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JMEMNOBS.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JQUANT1.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JQUANT2.C
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JUTILS.C
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\jpeg-6a\JCHUFF.H
# End Source File
# Begin Source File

SOURCE=.\jconfig.h
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JDCT.H
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JDHUFF.H
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JERROR.H
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JINCLUDE.H
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JMEMSYS.H
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JMORECFG.H
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JPEGINT.H
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JPEGLIB.H
# End Source File
# Begin Source File

SOURCE=.\jpeg-6a\JVERSION.H
# End Source File
# End Group
# End Target
# End Project
