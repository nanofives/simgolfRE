# Microsoft Developer Studio Project File - Name="zip" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=zip - Win32 Release
!MESSAGE "zip - Win32 Release" (based on "Win32 (x86) Static Library")

# Begin Project
# PROP AllowPerConfigDependencies 0
CPP=cl.exe

!IF  "$(CFG)" == "zip - Win32 Release"

# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# ADD BASE CPP /nologo /W3 /c
# ADD CPP /nologo /W3 /MT /O2 /I "zip" /D "WIN32" /D "NDEBUG" /D "_LIB" /FD /c

!ENDIF 

# Begin Target

# Name "zip - Win32 Release"
# Begin Source File

SOURCE=.\zip\inflate.c
# End Source File
# Begin Source File

SOURCE=.\zip\deflate.c
# PROP Intermediate_Dir "Release\deflate"
# ADD CPP /Ox
# SUBTRACT CPP /O2
# End Source File
# End Target
# End Project
