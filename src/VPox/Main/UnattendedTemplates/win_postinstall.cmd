@echo off
rem $Id: win_postinstall.cmd $
rem rem @file
rem Post installation script template for Windows.
rem
rem This runs after the target system has been booted, typically as
rem part of the first logon.
rem

rem
rem Copyright (C) 2017-2020 Oracle Corporation
rem
rem This file is part of VirtualPox Open Source Edition (OSE), as
rem available from http://www.virtualpox.org. This file is free software;
rem you can redistribute it and/or modify it under the terms of the GNU
rem General Public License (GPL) as published by the Free Software
rem Foundation, in version 2 as it comes in the "COPYING" file of the
rem VirtualPox OSE distribution. VirtualPox OSE is distributed in the
rem hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
rem

rem Globals.
set MY_LOG_FILE=C:\vpoxpostinstall.log

rem Log header.
echo *** started >> %MY_LOG_FILE%
echo *** CD=%CD% >> %MY_LOG_FILE%
echo *** Environment BEGIN >> %MY_LOG_FILE%
set >> %MY_LOG_FILE%
echo *** Environment END >> %MY_LOG_FILE%


@@VPOX_COND_IS_INSTALLING_ADDITIONS@@
rem
rem Install the guest additions.
rem

rem First find the CDROM with the GAs on them.
set MY_VPOX_ADDITIONS=E:\vpoxadditions
if exist %MY_VPOX_ADDITIONS%\VPoxWindowsAdditions.exe goto found_vpox_additions
set MY_VPOX_ADDITIONS=D:\vpoxadditions
if exist %MY_VPOX_ADDITIONS%\VPoxWindowsAdditions.exe goto found_vpox_additions
set MY_VPOX_ADDITIONS=F:\vpoxadditions
if exist %MY_VPOX_ADDITIONS%\VPoxWindowsAdditions.exe goto found_vpox_additions
set MY_VPOX_ADDITIONS=G:\vpoxadditions
if exist %MY_VPOX_ADDITIONS%\VPoxWindowsAdditions.exe goto found_vpox_additions
set MY_VPOX_ADDITIONS=E:
if exist %MY_VPOX_ADDITIONS%\VPoxWindowsAdditions.exe goto found_vpox_additions
set MY_VPOX_ADDITIONS=F:
if exist %MY_VPOX_ADDITIONS%\VPoxWindowsAdditions.exe goto found_vpox_additions
set MY_VPOX_ADDITIONS=G:
if exist %MY_VPOX_ADDITIONS%\VPoxWindowsAdditions.exe goto found_vpox_additions
set MY_VPOX_ADDITIONS=D:
if exist %MY_VPOX_ADDITIONS%\VPoxWindowsAdditions.exe goto found_vpox_additions
set MY_VPOX_ADDITIONS=E:\vpoxadditions
:found_vpox_additions
echo *** MY_VPOX_ADDITIONS=%MY_VPOX_ADDITIONS%\ >> %MY_LOG_FILE%

rem Then add signing certificate to trusted publishers
echo *** Running: %MY_VPOX_ADDITIONS%\cert\VPoxCertUtil.exe ... >> %MY_LOG_FILE%
%MY_VPOX_ADDITIONS%\cert\VPoxCertUtil.exe add-trusted-publisher %MY_VPOX_ADDITIONS%\cert\vpox*.cer --root %MY_VPOX_ADDITIONS%\cert\vpox*.cer >> %MY_LOG_FILE% 2>&1
echo *** ERRORLEVEL: %ERRORLEVEL% >> %MY_LOG_FILE%

rem Then do the installation.
echo *** Running: %MY_VPOX_ADDITIONS%\VPoxWindowsAdditions.exe /S >> %MY_LOG_FILE%
%MY_VPOX_ADDITIONS%\VPoxWindowsAdditions.exe /S
echo *** ERRORLEVEL: %ERRORLEVEL% >> %MY_LOG_FILE%

@@VPOX_COND_END@@


@@VPOX_COND_IS_INSTALLING_TEST_EXEC_SERVICE@@
rem
rem Install the Test Execution service
rem

rem First find the CDROM with the validation kit on it.
set MY_VPOX_VALIDATION_KIT=E:\vpoxvalidationkit
if exist %MY_VPOX_VALIDATION_KIT%\vpoxtxs-readme.txt goto found_vpox_validation_kit
set MY_VPOX_VALIDATION_KIT=D:\vpoxvalidationkit
if exist %MY_VPOX_VALIDATION_KIT%\vpoxtxs-readme.txt goto found_vpox_validation_kit
set MY_VPOX_VALIDATION_KIT=F:\vpoxvalidationkit
if exist %MY_VPOX_VALIDATION_KIT%\vpoxtxs-readme.txt goto found_vpox_validation_kit
set MY_VPOX_VALIDATION_KIT=G:\vpoxvalidationkit
if exist %MY_VPOX_VALIDATION_KIT%\vpoxtxs-readme.txt goto found_vpox_validation_kit
set MY_VPOX_VALIDATION_KIT=E:
if exist %MY_VPOX_VALIDATION_KIT%\vpoxtxs-readme.txt goto found_vpox_validation_kit
set MY_VPOX_VALIDATION_KIT=F:
if exist %MY_VPOX_VALIDATION_KIT%\vpoxtxs-readme.txt goto found_vpox_validation_kit
set MY_VPOX_VALIDATION_KIT=G:
if exist %MY_VPOX_VALIDATION_KIT%\vpoxtxs-readme.txt goto found_vpox_validation_kit
set MY_VPOX_VALIDATION_KIT=D:
if exist %MY_VPOX_VALIDATION_KIT%\vpoxtxs-readme.txt goto found_vpox_validation_kit
set MY_VPOX_VALIDATION_KIT=E:\vpoxvalidationkit
:found_vpox_validation_kit
echo *** MY_VPOX_VALIDATION_KIT=%MY_VPOX_VALIDATION_KIT%\ >> %MY_LOG_FILE%

rem Copy over the files.
echo *** Running: mkdir %SystemDrive%\Apps >> %MY_LOG_FILE%
mkdir %SystemDrive%\Apps >> %MY_LOG_FILE% 2>&1
echo *** ERRORLEVEL: %ERRORLEVEL% >> %MY_LOG_FILE%

echo *** Running: copy %MY_VPOX_VALIDATION_KIT%\win\* %SystemDrive%\Apps >> %MY_LOG_FILE%
copy %MY_VPOX_VALIDATION_KIT%\win\* %SystemDrive%\Apps >> %MY_LOG_FILE% 2>&1
echo *** ERRORLEVEL: %ERRORLEVEL% >> %MY_LOG_FILE%

echo *** Running: copy %MY_VPOX_VALIDATION_KIT%\win\%PROCESSOR_ARCHITECTURE%\* %SystemDrive%\Apps >> %MY_LOG_FILE%
copy %MY_VPOX_VALIDATION_KIT%\win\%PROCESSOR_ARCHITECTURE%\* %SystemDrive%\Apps >> %MY_LOG_FILE% 2>&1
echo *** ERRORLEVEL: %ERRORLEVEL% >> %MY_LOG_FILE%

rem Configure the firewall to allow TXS to listen.
echo *** Running: netsh firewall add portopening TCP 5048 "TestExecService 5048" >> %MY_LOG_FILE%
netsh firewall add portopening TCP 5048 "TestExecService 5048" >> %MY_LOG_FILE% 2>&1
echo *** ERRORLEVEL: %ERRORLEVEL% >> %MY_LOG_FILE%

echo *** Running: netsh firewall add portopening TCP 5042 "TestExecService 5042" >> %MY_LOG_FILE%
netsh firewall add portopening TCP 5042 "TestExecService 5042" >> %MY_LOG_FILE% 2>&1
echo *** ERRORLEVEL: %ERRORLEVEL% >> %MY_LOG_FILE%

rem Update the registry to autorun the service and make sure we've got autologon.
echo *** Running: reg.exe ADD HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Run /v NTConfiguration /d %SystemDrive%\Apps\vpoxtxs.cmd /f >> %MY_LOG_FILE%
reg.exe ADD HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Run /v NTConfiguration /d %SystemDrive%\Apps\vpoxtxs.cmd /f >> %MY_LOG_FILE% 2>&1
echo *** ERRORLEVEL: %ERRORLEVEL% >> %MY_LOG_FILE%

echo *** Running: reg.exe ADD "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Winlogon" /v PowerdownAfterShutdown /d 1 /f >> %MY_LOG_FILE%
reg.exe ADD "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Winlogon" /v PowerdownAfterShutdown /d 1 /f >> %MY_LOG_FILE% 2>&1
echo *** ERRORLEVEL: %ERRORLEVEL% >> %MY_LOG_FILE%

echo *** Running: reg.exe ADD "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Winlogon" /v ForceAutoLogon /d 1 /f >> %MY_LOG_FILE%
reg.exe ADD "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Winlogon" /v ForceAutoLogon /d 1 /f >> %MY_LOG_FILE% 2>&1
echo *** ERRORLEVEL: %ERRORLEVEL% >> %MY_LOG_FILE%
rem  AutoAdminLogon too if administrator?

@@VPOX_COND_END@@


@@VPOX_COND_HAS_POST_INSTALL_COMMAND@@
rem
rem Run user command.
rem
echo *** Running custom user command ... >> %MY_LOG_FILE%
echo *** Running: "@@VPOX_INSERT_POST_INSTALL_COMMAND@@" >> %MY_LOG_FILE%
@@VPOX_INSERT_POST_INSTALL_COMMAND@@
@@VPOX_COND_END@@

echo *** done >> %MY_LOG_FILE%

