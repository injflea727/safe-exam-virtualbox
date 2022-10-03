@echo off
rem $Id: VPoxSFInst.cmd $
rem rem @file
rem Windows NT batch script for manually installing the shared folders guest addition driver.
rem

rem
rem Copyright (C) 2009-2020 Oracle Corporation
rem
rem This file is part of VirtualPox Open Source Edition (OSE), as
rem available from http://www.virtualpox.org. This file is free software;
rem you can redistribute it and/or modify it under the terms of the GNU
rem General Public License (GPL) as published by the Free Software
rem Foundation, in version 2 as it comes in the "COPYING" file of the
rem VirtualPox OSE distribution. VirtualPox OSE is distributed in the
rem hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
rem


setlocal ENABLEEXTENSIONS
setlocal

rem
rem VPoxSF.sys should be in the same directory as this script or in the additions output dir.
rem
set MY_VPOXSF_SYS=%~dp0VPoxSF.sys
if exist "%MY_VPOXSF_SYS%" goto found_vpoxsf
set MY_VPOXSF_SYS=%~dp0..\..\..\..\..\out\win.%KBUILD_TARGET_ARCH%\%KBUILD_TYPE%\bin\additions\VPoxSF.sys
if exist "%MY_VPOXSF_SYS%" goto found_vpoxsf
echo VPoxSFInst.cmd: failed to find VPoxSF.sys in either "%~dp0" or "%~dp0..\..\..\..\..\out\win.%KBUILD_TARGET_ARCH%\%KBUILD_TYPE%\bin\additions\".
goto end
:found_vpoxsf

rem
rem VPoxMRXNP.dll should be in the same directory as this script or in the additions output dir.
rem
set MY_VPOXMRXNP_DLL=%~dp0VPoxMRXNP.dll
if exist "%MY_VPOXMRXNP_DLL%" goto found_vpoxmrxnp
set MY_VPOXMRXNP_DLL=%~dp0..\..\..\..\..\out\win.%KBUILD_TARGET_ARCH%\%KBUILD_TYPE%\bin\additions\VPoxMRXNP.dll
if exist "%MY_VPOXMRXNP_DLL%" goto found_vpoxmrxnp
echo VPoxSFInst.cmd: failed to find VPoxMRXNP.dll in either "%~dp0" or "%~dp0..\..\..\..\..\out\win.%KBUILD_TARGET_ARCH%\%KBUILD_TYPE%\bin\additions\".
goto end
:found_vpoxmrxnp

rem 32-bit version of same.
if not "%PROCESSOR_ARCHITECTURE%" == "AMD64" goto found_vpoxmrxnp_x86
set MY_VPOXMRXNP_X86_DLL=%~dp0VPoxMRXNP-x86.dll
if exist "%MY_VPOXMRXNP_X86_DLL%" goto found_vpoxmrxnp_x86
set MY_VPOXMRXNP_X86_DLL=%~dp0..\..\..\..\..\out\win.%KBUILD_TARGET_ARCH%\%KBUILD_TYPE%\bin\additions\VPoxMRXNP-x86.dll
if exist "%MY_VPOXMRXNP_X86_DLL%" goto found_vpoxmrxnp_x86
echo VPoxSFInst.cmd: failed to find VPoxMRXNP-x86.dll in either "%~dp0" or "%~dp0..\..\..\..\..\out\win.%KBUILD_TARGET_ARCH%\%KBUILD_TYPE%\bin\additions\".
goto end
:found_vpoxmrxnp_x86

rem
rem VPoxDrvInst.exe should be in the same directory as this script or in the additions output dir.
rem
set MY_VPOXDRVINST=%~dp0VPoxDrvInst.exe
if exist "%MY_VPOXDRVINST%" goto found_vpoxdrvinst
set MY_VPOXDRVINST=%~dp0..\..\..\..\..\out\win.%KBUILD_TARGET_ARCH%\%KBUILD_TYPE%\bin\additions\VPoxDrvInst.exe
if exist "%MY_VPOXDRVINST%" goto found_vpoxdrvinst
echo VPoxSFInst.cmd: failed to find VPoxDrvInst.exe in either "%~dp0" or "%~dp0..\..\..\..\..\out\win.%KBUILD_TARGET_ARCH%\%KBUILD_TYPE%\bin\additions\".
goto end
:found_vpoxdrvinst

rem
rem Deregister the service, provider and delete old files.
rem
echo "Uninstalling..."
sc stop VPoxSF
reg delete /f "HKLM\SYSTEM\CurrentControlSet\Services\VPoxSF\NetworkProvider" /v "DeviceName"
reg delete /f "HKLM\SYSTEM\CurrentControlSet\Services\VPoxSF\NetworkProvider" /v "Name"
reg delete /f "HKLM\SYSTEM\CurrentControlSet\Services\VPoxSF\NetworkProvider" /v "ProviderPath"

"%MY_VPOXDRVINST%" service delete VPoxSF

del "%SYSTEMROOT%\system32\drivers\VPoxSF.sys"
del "%SYSTEMROOT%\system32\VPoxMRXNP.dll"
if "%PROCESSOR_ARCHITECTURE%" == "AMD64" del "%SYSTEMROOT%\SysWOW64\VPoxMRXNP.dll"


rem
rem Install anything?
rem
if "%1" == "-u" goto end
if "%1" == "--uninstall" goto end

rem
rem Copy the new files to the system dir.
rem
echo "Copying files..."
copy "%MY_VPOXSF_SYS%"    "%SYSTEMROOT%\system32\drivers\"
copy "%MY_VPOXMRXNP_DLL%" "%SYSTEMROOT%\system32\"
if "%PROCESSOR_ARCHITECTURE%" == "AMD64" copy "%MY_VPOXMRXNP_X86_DLL%" "%SYSTEMROOT%\SysWow64\VPoxMRXNP.dll"

rem
rem Register the service.
rem
echo "Installing service..."
"%MY_VPOXDRVINST%" service create VPoxSF "VirtualPox Shared Folders" 2 1 "%SYSTEMROOT%\System32\drivers\VPoxSF.sys" NetworkProvider

echo "Configuring network provider..."
reg add "HKLM\SYSTEM\CurrentControlSet\Services\VPoxSF\NetworkProvider" /v "DeviceName"    /d "\Device\VPoxMiniRdr"
reg add "HKLM\SYSTEM\CurrentControlSet\Services\VPoxSF\NetworkProvider" /v "Name"          /d "VirtualPox Shared Folders"
reg add "HKLM\SYSTEM\CurrentControlSet\Services\VPoxSF\NetworkProvider" /v "ProviderPath"  /d "%SYSTEMROOT%\System32\VPoxMRXNP.dll"

"%MY_VPOXDRVINST%" netprovider add VPoxSF 0

rem
rem Start the service?
rem
if "%1" == "-n" goto end
if "%1" == "--no-start" goto end
sc start VPoxSF

:end
endlocal
endlocal

