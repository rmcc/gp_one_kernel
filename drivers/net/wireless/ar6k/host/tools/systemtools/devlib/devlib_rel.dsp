# Microsoft Developer Studio Project File - Name="devlib" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=devlib - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "devlib_rel.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "devlib_rel.mak" CFG="devlib - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "devlib - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "devlib - Win32 customerRelease" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName "devlib"
# PROP Scc_LocalPath "..\..\..\.."
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "devlib - Win32 Debug"

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
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MAUILIB_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W4 /Gm /GX /ZI /Od /I "." /I "..\common" /I "..\src2\include" /I "..\..\..\include" /I "..\devlib\Linuxini" /I "..\..\..\core\ini" /I "..\..\..\..\target\ram\apps\systemtools" /I ".\ar5210" /I ".\ar5211" /I ".\ar5212" /I ".\ar2413" /I ".\ar6000" /I ".\ar5513" /I "..\common\include" /I "ini" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MAUILIB_EXPORTS" /D "MANLIB_EXPORTS" /D "CUSTOMER_REL" /D "NO_LIB_PRINT" /D "AR6002" /FR /FD /GZ /TP /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 ws2_32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib GPIB-32.obj NrpControl.lib /nologo /dll /debug /machine:I386 /out:"Debug/devlib.dll" /pdbtype:sept /libpath:"addnl_inst"
# SUBTRACT LINK32 /pdb:none
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=IF EXIST ..\..\..\..\..\win_release\art\bin copy debug\devlib.dll  ..\..\..\..\..\win_release\art\bin
# End Special Build Tool

!ELSEIF  "$(CFG)" == "devlib - Win32 customerRelease"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "devlib___Win32_customerRelease"
# PROP BASE Intermediate_Dir "devlib___Win32_customerRelease"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "customerRel"
# PROP Intermediate_Dir "customerRel"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W4 /GX /O2 /I "..\..\..\include" /I "..\..\..\core\ini" /I "..\client" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MAUILIB_EXPORTS" /D "MANLIB_EXPORTS" /FR /FD /TP /c
# ADD CPP /MT /W4 /GX /O2 /I "." /I "..\common" /I "..\src2\include" /I "..\devlib\Linuxini" /I "..\include" /I "..\..\..\..\target\ram\apps\systemtools" /I "..\art" /I ".\ar5210" /I ".\ar5211" /I ".\ar5212" /I ".\ar2413" /I ".\ar6000" /I ".\ar5513" /I "..\common\include" /I "ini" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MAUILIB_EXPORTS" /D "MANLIB_EXPORTS" /D "CUSTOMER_REL" /D "NO_LIB_PRINT" /D "AR6002" /FR /FD /TP /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ..\manlib\GPIB-32.obj /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib GPIB-32.obj NrpControl.lib /nologo /dll /machine:I386 /out:"customerRel/devlib.dll" /libpath:"addnl_inst"

!ENDIF 

# Begin Target

# Name "devlib - Win32 Debug"
# Name "devlib - Win32 customerRelease"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\art_ani.c
# End Source File
# Begin Source File

SOURCE=.\artEar.c
# End Source File
# Begin Source File

SOURCE=.\athreg.c
# End Source File
# Begin Source File

SOURCE=.\mAlloc.c
# End Source File
# Begin Source File

SOURCE=.\mCal.c
# End Source File
# Begin Source File

SOURCE=.\mConfig.c
# End Source File
# Begin Source File

SOURCE=.\mCont.c
# End Source File
# Begin Source File

SOURCE=.\mData.c
# End Source File
# Begin Source File

SOURCE=.\mDevtbl.c
# End Source File
# Begin Source File

SOURCE=.\mEeprom.c
# End Source File
# Begin Source File

SOURCE=.\mInst.c

!IF  "$(CFG)" == "devlib - Win32 Debug"

# SUBTRACT CPP /D "MANLIB_EXPORTS"

!ELSEIF  "$(CFG)" == "devlib - Win32 customerRelease"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\common\rate_constants.c
# End Source File
# Begin Source File

SOURCE=..\common\stats_routines.c
# End Source File
# Begin Source File

SOURCE=.\addnl_inst\usb_pm.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\include\ar5210reg.h
# End Source File
# Begin Source File

SOURCE=.\ar5211\ar5211reg.h
# End Source File
# Begin Source File

SOURCE=.\art_ani.h
# End Source File
# Begin Source File

SOURCE=.\artEar.h
# End Source File
# Begin Source File

SOURCE=.\athreg.h
# End Source File
# Begin Source File

SOURCE="..\manlib\decl-32.h"
# End Source File
# Begin Source File

SOURCE=.\ini\dk_0016.ini
# End Source File
# Begin Source File

SOURCE=.\ini\dk_0016.mod
# End Source File
# Begin Source File

SOURCE=.\ini\dk_boss_0012.ini
# End Source File
# Begin Source File

SOURCE=.\ini\dk_boss_0012.mod
# End Source File
# Begin Source File

SOURCE=.\ini\dk_boss_0013.ini
# End Source File
# Begin Source File

SOURCE=.\ini\dk_boss_0013.mod
# End Source File
# Begin Source File

SOURCE=.\ini\dk_crete_fez.ini
# End Source File
# Begin Source File

SOURCE=.\manlib.h
# End Source File
# Begin Source File

SOURCE=.\manlibInst.h
# End Source File
# Begin Source File

SOURCE=.\mConfig.h
# End Source File
# Begin Source File

SOURCE=.\mdata.h
# End Source File
# Begin Source File

SOURCE=.\mDevtbl.h
# End Source File
# Begin Source File

SOURCE=.\mEeprom.h
# End Source File
# Begin Source File

SOURCE=.\mInst.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\ntdrv.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\pci.h
# End Source File
# Begin Source File

SOURCE=.\addnl_inst\usb_pm.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\wlanos.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\wlanproto.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\wlantype.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Group "Ar5210"

# PROP Default_Filter "cpp;c;h"
# Begin Source File

SOURCE=.\ar5210\mCfg210.c
# End Source File
# Begin Source File

SOURCE=.\ar5210\mCfg210.h
# End Source File
# Begin Source File

SOURCE=.\ar5210\mData210.c
# End Source File
# Begin Source File

SOURCE=.\ar5210\mData210.h
# End Source File
# Begin Source File

SOURCE=.\ar5210\mEEP210.c
# End Source File
# Begin Source File

SOURCE=.\ar5210\mEEP210.h
# End Source File
# End Group
# Begin Group "Ar5211"

# PROP Default_Filter "c;cpp;h"
# Begin Source File

SOURCE=.\ar5211\mCfg211.c
# End Source File
# Begin Source File

SOURCE=.\ar5211\mCfg211.h
# End Source File
# Begin Source File

SOURCE=.\ar5211\mData211.c
# End Source File
# Begin Source File

SOURCE=.\ar5211\mData211.h
# End Source File
# Begin Source File

SOURCE=.\ar5211\mEEP211.c
# End Source File
# Begin Source File

SOURCE=.\ar5211\mEEP211.h
# End Source File
# End Group
# Begin Group "Ar5212"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\ar5212\mAni212.c
# End Source File
# Begin Source File

SOURCE=.\ar5212\mAni212.h
# End Source File
# Begin Source File

SOURCE=.\ar5212\mCfg212.c
# End Source File
# Begin Source File

SOURCE=.\ar5212\mCfg212.h
# End Source File
# Begin Source File

SOURCE=.\ar5212\mCfg212d.c
# End Source File
# Begin Source File

SOURCE=.\ar5212\mCfg212d.h
# End Source File
# Begin Source File

SOURCE=.\ar5212\mData212.c
# End Source File
# Begin Source File

SOURCE=.\ar5212\mData212.h
# End Source File
# Begin Source File

SOURCE=.\ar5212\mEEPROM_d.c
# End Source File
# Begin Source File

SOURCE=.\ar5212\mEEPROM_d.h
# End Source File
# End Group
# Begin Group "ar2413"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\ar2413\mCfg413.c
# End Source File
# Begin Source File

SOURCE=.\ar2413\mCfg413.h
# End Source File
# Begin Source File

SOURCE=.\ar2413\mEEPROM_g.c
# End Source File
# Begin Source File

SOURCE=.\ar2413\mEEPROM_g.h
# End Source File
# End Group
# Begin Group "Ar6000"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\ar6000\ar6000reg.h
# End Source File
# Begin Source File

SOURCE=.\ar6000\mCfg6000.c
# End Source File
# Begin Source File

SOURCE=.\ar6000\mCfg6000.h
# End Source File
# Begin Source File

SOURCE=.\ar6000\mEep6000.c
# End Source File
# Begin Source File

SOURCE=.\ar6000\mEep6000.h
# End Source File
# Begin Source File

SOURCE=.\ar6000\mEepStruct6000.h
# End Source File
# End Group
# Begin Group "Ar5513"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\ar5513\ar5513reg.h
# End Source File
# Begin Source File

SOURCE=.\ar5513\mCfg513.c
# End Source File
# Begin Source File

SOURCE=.\ar5513\mCfg513.h
# End Source File
# Begin Source File

SOURCE=.\ar5513\mData513.c
# End Source File
# Begin Source File

SOURCE=.\ar5513\mData513.h
# End Source File
# End Group
# End Target
# End Project
