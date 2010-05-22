# Microsoft Developer Studio Project File - Name="art_rel" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=art_rel - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "art_rel.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "art_rel.mak" CFG="art_rel - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "art_rel - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "art_rel - Win32 customerRelease" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName "dk_install"
# PROP Scc_LocalPath ".."
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "art_rel - Win32 Debug"

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
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "." /I "..\art" /I "..\src2\include" /I "..\devlib\Linuxini" /I "..\devlib\ar2413" /I "..\kerplug" /I "..\devlib\ar5212" /I "..\devlib\ar6000" /I "..\include" /I "..\devlib" /I "..\mdk" /I "..\common" /I "..\common\include" /I "..\..\..\..\target\ram\apps\systemtools\include" /I "..\..\..\..\target\ram\apps\systemtools\sta\anwi\anwi_wdm\inc" /D "_DEBUG" /D "_WINDOWS" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "_MLD" /D "ART_BUILD" /D "NO_LIB_PRINT" /D "ANWI" /D "AR6002" /FR /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 ws2_32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib devlib.lib setupapi.lib devdrv.lib /nologo /subsystem:console /debug /machine:I386 /out:"Debug/art_debug.exe" /pdbtype:sept /libpath:"..\devlib\Debug" /libpath:"..\winene_utility\devdll\src\devlib\release_lib"
# SUBTRACT LINK32 /pdb:none
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=IF EXIST ..\..\..\..\..\win_release\art\bin copy debug\art_debug.exe ..\..\..\..\..\win_release\art\bin
# End Special Build Tool

!ELSEIF  "$(CFG)" == "art_rel - Win32 customerRelease"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "mld___Win32_customerRelease"
# PROP BASE Intermediate_Dir "mld___Win32_customerRelease"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "customerRel"
# PROP Intermediate_Dir "customerRel"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /I "..\..\..\include" /I "..\devlib" /I "..\manlib" /I "..\mdk" /I "..\common" /D "NDEBUG" /D "_WINDOWS" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "_MLD" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "." /I "..\art" /I "..\src2\include" /I "..\devlib\Linuxini" /I "..\devlib\ar2413" /I "..\devlib\ar5212" /I "..\devlib\ar6000" /I "..\include" /I "..\devlib" /I "..\mdk" /I "..\common" /I "..\common\include" /I "..\..\..\..\target\ram\apps\systemtools\include" /I "..\..\..\..\target\ram\apps\systemtools\sta\anwi\anwi_wdm\inc" /D "NDEBUG" /D "ANWI" /D "ART_BUILD" /D "_WINDOWS" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "_MLD" /D "CUSTOMER_REL" /D "NO_LIB_PRINT" /D "AR6002" /FR /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib devlib.lib /nologo /subsystem:console /machine:I386 /out:"Release/art.exe" /libpath:"..\devlib\release"
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib devlib.lib ws2_32.lib setupapi.lib devdrv.lib /nologo /subsystem:console /machine:I386 /out:"customerRel/art.exe" /libpath:"..\devlib\customerRel" /libpath:"..\winene_utility\devdll\src\devlib\release_lib"
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "art_rel - Win32 Debug"
# Name "art_rel - Win32 customerRelease"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\common\anwi_hw.c
# End Source File
# Begin Source File

SOURCE=.\art_comms.c
# End Source File
# Begin Source File

SOURCE=.\art_if.c
# End Source File
# Begin Source File

SOURCE=.\cal_com.c
# End Source File
# Begin Source File

SOURCE=.\cal_gen3.c
# End Source File
# Begin Source File

SOURCE=.\cal_gen5.c
# End Source File
# Begin Source File

SOURCE=.\cal_gen6.c
# End Source File
# Begin Source File

SOURCE=.\cmdTest.c
# End Source File
# Begin Source File

SOURCE=..\common\dk_mem.c
# End Source File
# Begin Source File

SOURCE=.\dkdownload.c
# End Source File
# Begin Source File

SOURCE=.\dynamic_optimizations.c
# End Source File
# Begin Source File

SOURCE=.\dynArray.c
# End Source File
# Begin Source File

SOURCE=.\eeprom.c
# End Source File
# Begin Source File

SOURCE=..\common\hw_routines.c
# End Source File
# Begin Source File

SOURCE=.\mathRoutines.c
# End Source File
# Begin Source File

SOURCE=.\maui_cal.c
# End Source File
# Begin Source File

SOURCE=..\common\misc_routines.c
# End Source File
# Begin Source File

SOURCE=.\MLIBif.c
# End Source File
# Begin Source File

SOURCE=.\nrutil.c
# End Source File
# Begin Source File

SOURCE=..\common\osWrap_win.c
# End Source File
# Begin Source File

SOURCE=.\parse.c
# End Source File
# Begin Source File

SOURCE=.\parseLoadEar.c
# End Source File
# Begin Source File

SOURCE=.\rssi_power.c
# End Source File
# Begin Source File

SOURCE=.\test.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\..\target\ram\apps\systemtools\sta\anwi\anwi_wdm\inc\anwi_guid.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\target\ram\apps\systemtools\sta\anwi\anwi_wdm\inc\anwiIoctl.h
# End Source File
# Begin Source File

SOURCE=..\common\include\ar5210reg.h
# End Source File
# Begin Source File

SOURCE=.\art_if.h
# End Source File
# Begin Source File

SOURCE=.\cal_gen3.h
# End Source File
# Begin Source File

SOURCE=.\cal_gen5.h
# End Source File
# Begin Source File

SOURCE=..\common\include\dk_cmds.h
# End Source File
# Begin Source File

SOURCE=..\mdk\dk_ver.h
# End Source File
# Begin Source File

SOURCE=.\dynamic_optimizations.h
# End Source File
# Begin Source File

SOURCE=.\dynArray.h
# End Source File
# Begin Source File

SOURCE=.\ear_defs.h
# End Source File
# Begin Source File

SOURCE=.\ear_externs.h
# End Source File
# Begin Source File

SOURCE=..\devlib\manlib.h
# End Source File
# Begin Source File

SOURCE=.\mathRoutines.h
# End Source File
# Begin Source File

SOURCE=.\maui_cal.h
# End Source File
# Begin Source File

SOURCE=..\devlib\mConfig.h
# End Source File
# Begin Source File

SOURCE=.\mld_anwi.h
# End Source File
# Begin Source File

SOURCE=.\MLIBif.h
# End Source File
# Begin Source File

SOURCE=.\nrutil.h
# End Source File
# Begin Source File

SOURCE=.\parse.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\pci.h
# End Source File
# Begin Source File

SOURCE=.\sock_win.h
# End Source File
# Begin Source File

SOURCE=.\test.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\wlantype.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
