# Microsoft Developer Studio Project File - Name="util" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=util - Win32 Debug
!MESSAGE "util - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "util - Win32 Debug" (based on "Win32 (x86) Static Library")

# Begin Project
# PROP AllowPerConfigDependencies 0
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "util - Win32 Release"

# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /W3 /MT /O2 /I "include" /D "WIN32" /D "NDEBUG" /D "_LIB" /GR- /Zl /FD /c

!ELSEIF  "$(CFG)" == "util - Win32 Debug"

# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /ZI /Od /I "include" /D "WIN32" /D "_DEBUG" /D "_LIB" /Zl /FD /GZ /c

!ENDIF 

# Begin Target

# Name "util - Win32 Release"
# Name "util - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\src\strings.cpp
# ADD CPP /Yc"util.h"
# End Source File
# Begin Source File

SOURCE=.\src\table.cpp
# ADD CPP /Yu"util.h" /FI"util.h"
# End Source File
# End Group
# End Target
# End Project
