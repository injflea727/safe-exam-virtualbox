@echo off
rem $Id: Combined-3-RepackAdditions.cmd $
rem rem @file
rem Windows NT batch script for repacking signed amd64 and x86 drivers.
rem

rem
rem Copyright (C) 2018-2020 Oracle Corporation
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
rem Globals and checks for required enviornment variables.
rem
if ".%KBUILD_DEVTOOLS%" == "." (echo KBUILD_DEVTOOLS is not set & goto end_failed)
if ".%KBUILD_BIN_PATH%" == "." (echo KBUILD_BIN_PATH is not set & goto end_failed)
set _MY_SCRIPT_DIR=%~dp0
set _MY_SAVED_CD=%CD%
set _MY_VER_REV=@VPOX_VERSION_STRING@r@VPOX_SVN_REV@

rem
rem Parse arguments.
rem
set _MY_OPT_UNTAR_DIR=%_MY_SCRIPT_DIR%\..\..\..\
for %%i in (%_MY_OPT_UNTAR_DIR%) do set _MY_OPT_UNTAR_DIR=%%~fi
set _MY_OPT_BUILD_TYPE=@KBUILD_TYPE@
set _MY_OPT_OUTDIR=%_MY_OPT_UNTAR_DIR%\output
set _MY_OPT_SRC_DIR=%_MY_SCRIPT_DIR%\resources\

:argument_loop
if ".%1" == "."             goto no_more_arguments

if ".%1" == ".-h"           goto opt_h
if ".%1" == ".-?"           goto opt_h
if ".%1" == "./h"           goto opt_h
if ".%1" == "./H"           goto opt_h
if ".%1" == "./?"           goto opt_h
if ".%1" == ".-help"        goto opt_h
if ".%1" == ".--help"       goto opt_h

if ".%1" == ".-o"                   goto opt_o
if ".%1" == ".--outdir"             goto opt_o
if ".%1" == ".-s"                   goto opt_s
if ".%1" == ".--source"             goto opt_s
if ".%1" == ".--signed-amd64"       goto opt_signed_amd64
if ".%1" == ".--signed-x86"         goto opt_signed_x86
if ".%1" == ".-t"                   goto opt_t
if ".%1" == ".--build-type"         goto opt_t
if ".%1" == ".-u"                   goto opt_u
if ".%1" == ".--vpoxall-untar-dir"  goto opt_u
echo syntax error: Unknown option: %1
echo               Try --help to list valid options.
goto end_failed

:argument_loop_next_with_value
shift
shift
goto argument_loop

:opt_h
echo Toplevel combined package: Repack the guest additions.
echo .
echo Usage: Combined-3-RepackAdditions.cmd [-o output-dir]
echo            [-u/--vpoxall-dir unpacked-vpoxall-dir] [-t build-type]
echo            [--signed-amd64 signed-amd64.zip]
echo            [--signed-x86 signed-x86.zip]
echo
echo .
echo Default -u/--vpoxall-untar-dir value:  %_MY_OPT_UNTAR_DIR%
echo Default -o/--outdir value:             %_MY_OPT_OUTDIR%
echo Default -t/--build-type value:         %_MY_OPT_BUILD_TYPE%
echo .
goto end_failed

:opt_o
if ".%~2" == "."            goto syntax_error_missing_value
set _MY_OPT_OUTDIR=%~f2
goto argument_loop_next_with_value

:opt_s
if ".%~2" == "."            goto syntax_error_missing_value
set _MY_OPT_SRC_DIR=%~f2
goto argument_loop_next_with_value

:opt_signed_amd64
if ".%~2" == "."            goto syntax_error_missing_value
set _MY_OPT_SIGNED_AMD64=%~f2
goto argument_loop_next_with_value

:opt_signed_x86
if ".%~2" == "."            goto syntax_error_missing_value
set _MY_OPT_SIGNED_X86=%~f2
goto argument_loop_next_with_value

:opt_t
if ".%~2" == "."            goto syntax_error_missing_value
set _MY_OPT_BUILD_TYPE=%~2
goto argument_loop_next_with_value

:opt_u
if ".%~2" == "."            goto syntax_error_missing_value
set _MY_OPT_UNTAR_DIR=%~f2
goto argument_loop_next_with_value

:syntax_error_missing_value
echo syntax error: missing or empty option value after %1
goto end_failed

:error_vpoxall_untar_dir_not_found
echo syntax error: The VPoxAll untar directory was not found: "%_MY_OPT_UNTAR_DIR%"
goto end_failed

:error_amd64_bindir_not_found
echo syntax error: The AMD64 bin directory was not found: "%_MY_BINDIR_AMD64%"
goto end_failed

:error_x86_bindir_not_found
echo syntax error: The X86 bin directory was not found: "%_MY_BINDIR_X86%"
goto end_failed

:error_amd64_repack_dir_not_found
echo syntax error: The AMD64 repack directory was not found: "%_MY_REPACK_DIR_AMD64%"
goto end_failed

:error_x86_repack_dir_not_found
echo syntax error: The X86 repack directory was not found: "%_MY_REPACK_DIR_X86%"
goto end_failed

:error_extpack_not_found
echo syntax error: Specified extension pack not found: "%_MY_OPT_EXTPACK%"
goto end_failed

:error_enterprise_extpack_not_found
echo syntax error: Specified enterprise extension pack not found: "%_MY_OPT_EXTPACK_ENTERPRISE%"
goto end_failed

:error_signed_amd64_not_found
echo syntax error: Zip with signed AMD64 drivers not found: "%_MY_OPT_SIGNED_AMD64%"
goto end_failed

:error_signed_x86_not_found
echo syntax error: Zip with signed X86 drivers not found: "%_MY_OPT_SIGNED_X86%"
goto end_failed

:error_src_dir_not_found
echo syntax error: src directory not found: "%_MY_OPT_SRC_DIR%"
goto end_failed


:no_more_arguments
rem
rem Validate and adjust specified options.
rem

if not exist "%_MY_OPT_UNTAR_DIR%"      goto error_vpoxall_untar_dir_not_found

set _MY_BINDIR_AMD64=%_MY_OPT_UNTAR_DIR%\win.amd64\%_MY_OPT_BUILD_TYPE%\bin\additions
set _MY_BINDIR_X86=%_MY_OPT_UNTAR_DIR%\win.x86\%_MY_OPT_BUILD_TYPE%\bin\additions
if not exist "%_MY_BINDIR_AMD64%"       goto error_amd64_bindir_not_found
if not exist "%_MY_BINDIR_X86%"         goto error_x86_bindir_not_found

set _MY_REPACK_DIR_AMD64=%_MY_OPT_UNTAR_DIR%\win.amd64\%_MY_OPT_BUILD_TYPE%\repackadd
set _MY_REPACK_DIR_X86=%_MY_OPT_UNTAR_DIR%\win.x86\%_MY_OPT_BUILD_TYPE%\repackadd
if not exist "%_MY_REPACK_DIR_AMD64%"   goto error_amd64_repack_dir_not_found
if not exist "%_MY_REPACK_DIR_X86%"     goto error_x86_repack_dir_not_found

if not ".%_MY_OPT_SIGNED_AMD64%" == "." goto skip_set_default_amd64_signed
set _MY_OPT_SIGNED_AMD64="%_MY_OPT_OUTDIR%/VPoxDrivers-@VPOX_VERSION_STRING@r@VPOX_SVN_REV@-amd64.cab.Signed.zip"
:skip_set_default_amd64_signed

if not ".%_MY_OPT_SIGNED_X86%" == "." goto skip_set_default_x86_signed
set _MY_OPT_SIGNED_X86="%_MY_OPT_OUTDIR%/VPoxDrivers-@VPOX_VERSION_STRING@r@VPOX_SVN_REV@-x86.cab.Signed.zip"
:skip_set_default_x86_signed

if not exist "%_MY_OPT_SIGNED_AMD64%"   goto error_signed_amd64_not_found
if not exist "%_MY_OPT_SIGNED_X86%"     goto error_signed_x86_not_found

rem Make sure the output dir exists.
if not exist "%_MY_OPT_OUTDIR%"     (mkdir "%_MY_OPT_OUTDIR%" || goto end_failed)

if not exist "%_MY_OPT_SRC_DIR%"              goto error_src_dir_not_found

rem
rem Unpacking the two driver zips.
rem
echo **************************************************************************
echo * AMD64: Unpacking signed drivers...
echo **************************************************************************
cd /d "%_MY_REPACK_DIR_AMD64%" || goto end_failed
call "%_MY_REPACK_DIR_AMD64%\UnpackBlessedDrivers.cmd" -n -b "%_MY_BINDIR_AMD64%" -i "%_MY_OPT_SIGNED_AMD64%" --guest-additions || goto end_failed
echo .

echo **************************************************************************
echo * X86: Unpacking signed drivers...
echo **************************************************************************
cd /d "%_MY_REPACK_DIR_X86%" || goto end_failed
call "%_MY_REPACK_DIR_X86%\UnpackBlessedDrivers.cmd" -n -b "%_MY_BINDIR_X86%" -i "%_MY_OPT_SIGNED_X86%" --guest-additions || goto end_failed
echo .


rem
rem Building amd64 installer
rem
echo **************************************************************************
echo * Building amd64 installer
echo **************************************************************************

del %_MY_OPT_UNTAR_DIR%\win.amd64\release\bin\additions\VPoxWindowsAdditions-amd64.exe
cp %_MY_REPACK_DIR_AMD64%\..\obj\uninst.exe %_MY_REPACK_DIR_AMD64%

rem TBD: that has to be converted to invoke auto-generated .cmd

%KBUILD_BIN_PATH%\kmk_redirect.exe -C %_MY_OPT_SRC_DIR% ^
        -E "PATH_OUT=%_MY_REPACK_DIR_AMD64%\.." ^
        -E "PATH_TARGET=%_MY_REPACK_DIR_AMD64%" ^
        -E "PATH_TARGET_X86=%_MY_REPACK_DIR_X86%\resources" ^
        -E "VPOX_PATH_ADDITIONS_WIN_X86=%_MY_REPACK_DIR_AMD64%\..\bin\additions" ^
        -E "VPOX_PATH_DIFX=%KBUILD_DEVTOOLS%\win.amd64\DIFx\v2.1-r3" ^
        -E "VPOX_VENDOR=Oracle Corporation" -E "VPOX_VENDOR_SHORT=Oracle" -E "VPOX_PRODUCT=Oracle VM VirtualPox" ^
        -E "VPOX_C_YEAR=@VPOX_C_YEAR@" -E "VPOX_VERSION_STRING=@VPOX_VERSION_STRING@" -E "VPOX_VERSION_STRING_RAW=@VPOX_VERSION_STRING_RAW@" ^
        -E "VPOX_VERSION_MAJOR=@VPOX_VERSION_MAJOR@" -E "VPOX_VERSION_MINOR=@VPOX_VERSION_MINOR@" -E "VPOX_VERSION_BUILD=@VPOX_VERSION_BUILD@" -E "VPOX_SVN_REV=@VPOX_SVN_REV@" ^
        -E "VPOX_WINDOWS_ADDITIONS_ICON_FILE=%_MY_OPT_SRC_DIR%\VirtualPoxGA-vista.ico" ^
        -E "VPOX_NSIS_ICON_FILE=%_MY_OPT_SRC_DIR%\VirtualPoxGA-nsis.ico" ^
        -E "VPOX_WITH_GUEST_INSTALL_HELPER=1" -E "VPOX_WITH_GUEST_INSTALLER_UNICODE=1" -E "VPOX_WITH_LICENSE_INSTALL_RTF=1" ^
        -E "VPOX_WITH_WDDM=1" -E "VPOX_WITH_MESA3D=1" -E "VPOX_BRAND_WIN_ADD_INST_DLGBMP=%_MY_OPT_SRC_DIR%\welcome.bmp" ^
        -E "VPOX_BRAND_LICENSE_RTF=%_MY_OPT_SRC_DIR%\License-gpl-2.0.rtf"  -E "KBUILD_TYPE=%_MY_OPT_BUILD_TYPE%" -E "BUILD_TARGET_ARCH=amd64" ^
        --  %KBUILD_DEVTOOLS%/win.x86/nsis/v3.04-log/makensis.exe /NOCD /V2 ^
                "/DVPOX_SIGN_ADDITIONS=1" ^
                "/DEXTERNAL_UNINSTALLER=1" ^
                "%_MY_OPT_SRC_DIR%\VPoxGuestAdditions.nsi"

rem
rem Building amd64 installer
rem
echo **************************************************************************
echo * Building x86 installer
echo **************************************************************************

del %_MY_OPT_UNTAR_DIR%\win.x86\release\bin\additions\VPoxWindowsAdditions-x86.exe
cp %_MY_REPACK_DIR_X86%\..\obj\uninst.exe %_MY_REPACK_DIR_X86%\

rem TBD: that has to be converted to invoke auto-generated .cmd

%KBUILD_BIN_PATH%\kmk_redirect.exe -C %_MY_OPT_SRC_DIR% ^
        -E "PATH_OUT=%_MY_REPACK_DIR_X86%\.." ^
        -E "PATH_TARGET=%_MY_REPACK_DIR_X86%" ^
        -E "PATH_TARGET_X86=%_MY_REPACK_DIR_X86%\resources" ^
        -E "VPOX_PATH_ADDITIONS_WIN_X86=%_MY_REPACK_DIR_X86%\..\bin\additions" ^
        -E "VPOX_PATH_DIFX=%KBUILD_DEVTOOLS%\win.x86\DIFx\v2.1-r3" ^
        -E "VPOX_VENDOR=Oracle Corporation" -E "VPOX_VENDOR_SHORT=Oracle" -E "VPOX_PRODUCT=Oracle VM VirtualPox" ^
        -E "VPOX_C_YEAR=@VPOX_C_YEAR@" -E "VPOX_VERSION_STRING=@VPOX_VERSION_STRING@" -E "VPOX_VERSION_STRING_RAW=@VPOX_VERSION_STRING_RAW@" ^
        -E "VPOX_VERSION_MAJOR=@VPOX_VERSION_MAJOR@" -E "VPOX_VERSION_MINOR=@VPOX_VERSION_MINOR@" -E "VPOX_VERSION_BUILD=@VPOX_VERSION_BUILD@" -E "VPOX_SVN_REV=@VPOX_SVN_REV@" ^
        -E "VPOX_WINDOWS_ADDITIONS_ICON_FILE=%_MY_OPT_SRC_DIR%\VirtualPoxGA-vista.ico" ^
        -E "VPOX_NSIS_ICON_FILE=%_MY_OPT_SRC_DIR%\VirtualPoxGA-nsis.ico" ^
        -E "VPOX_WITH_GUEST_INSTALL_HELPER=1" -E "VPOX_WITH_GUEST_INSTALLER_UNICODE=1" -E "VPOX_WITH_LICENSE_INSTALL_RTF=1" ^
        -E "VPOX_WITH_WDDM=1" -E "VPOX_WITH_MESA3D=1" -E "VPOX_BRAND_WIN_ADD_INST_DLGBMP=%_MY_OPT_SRC_DIR%\welcome.bmp" ^
        -E "VPOX_BRAND_LICENSE_RTF=%_MY_OPT_SRC_DIR%\License-gpl-2.0.rtf"  -E "KBUILD_TYPE=%_MY_OPT_BUILD_TYPE%" -E "BUILD_TARGET_ARCH=x86" ^
        --  %KBUILD_DEVTOOLS%/win.x86/nsis/v3.04-log/makensis.exe /NOCD /V2 ^
                "/DVPOX_SIGN_ADDITIONS=1" ^
                "/DEXTERNAL_UNINSTALLER=1" ^
                "%_MY_OPT_SRC_DIR%\VPoxGuestAdditions.nsi"

rem
rem Making .iso
rem
echo **************************************************************************
echo * Making VPoxGuestAdditions.iso
echo **************************************************************************

del %_MY_OPT_OUTDIR%/VPoxGuestAdditions.iso

rem TBD: that has to be converted to invoke auto-generated .cmd

%_MY_SCRIPT_DIR%/../bin/bldRTIsoMaker.exe ^
        --output %_MY_OPT_OUTDIR%/VPoxGuestAdditions.iso ^
        --iso-level 3 ^
        --rock-ridge ^
        --joliet ^
        --rational-attribs ^
        --random-order-verification 2048 ^
        /cert/vpox-sha1.cer=%_MY_SCRIPT_DIR%/../bin/additions/vpox-sha1.cer ^
        /cert/vpox-sha256.cer=%_MY_SCRIPT_DIR%/../bin/additions/vpox-sha256.cer ^
        /windows11-bypass.reg=%_MY_SCRIPT_DIR%/../bin/additions/windows11-bypass.reg ^
        /VPoxWindowsAdditions-x86.exe=%_MY_OPT_UNTAR_DIR%/win.x86/release/bin/additions/VPoxWindowsAdditions-x86.exe ^
        /VPoxWindowsAdditions.exe=%_MY_OPT_UNTAR_DIR%/win.x86/release/bin/additions/VPoxWindowsAdditions.exe ^
        /AUTORUN.INF=%_MY_OPT_SRC_DIR%/AUTORUN.INF ^
        /cert/VPoxCertUtil.exe=%_MY_SCRIPT_DIR%/../bin/additions/VPoxCertUtil.exe ^
        /NT3x/Readme.txt=%_MY_OPT_SRC_DIR%/NT3xReadme.txt ^
        /NT3x/VPoxGuest.sys=%_MY_OPT_UNTAR_DIR%/win.x86/release/bin/additions/VPoxGuest.sys ^
        /NT3x/VPoxGuest.cat=%_MY_OPT_UNTAR_DIR%/win.x86/release/bin/additions/VPoxGuest.cat ^
        /NT3x/VPoxGuest.inf=%_MY_OPT_UNTAR_DIR%/win.x86/release/bin/additions/VPoxGuest.inf ^
        /NT3x/VPoxMouseNT.sys=%_MY_OPT_UNTAR_DIR%/win.x86/release/bin/additions/VPoxMouseNT.sys ^
        /NT3x/VPoxMouse.inf=%_MY_OPT_UNTAR_DIR%/win.x86/release/bin/additions/VPoxMouse.inf ^
        /NT3x/VPoxMouse.cat=%_MY_OPT_UNTAR_DIR%/win.x86/release/bin/additions/VPoxMouse.cat ^
        /NT3x/VPoxMouse.sys=%_MY_OPT_UNTAR_DIR%/win.x86/release/bin/additions/VPoxMouse.sys ^
        /NT3x/VPoxControl.exe=%_MY_OPT_UNTAR_DIR%/win.x86/release/bin/additions/VPoxControl.exe ^
        /NT3x/VPoxService.exe=%_MY_OPT_UNTAR_DIR%/win.x86/release/bin/additions/VPoxService.exe ^
        /VPoxWindowsAdditions-amd64.exe=%_MY_OPT_UNTAR_DIR%/win.amd64/release/bin/additions/VPoxWindowsAdditions-amd64.exe ^
        /VPoxSolarisAdditions.pkg=%_MY_OPT_UNTAR_DIR%/solaris.x86/release/bin/additions/VPoxSolarisAdditions.pkg ^
        /OS2/VPoxGuest.sys=%_MY_OPT_UNTAR_DIR%/os2.x86/release/bin/additions/VPoxGuest.sys ^
        /OS2/VPoxSF.ifs=%_MY_OPT_UNTAR_DIR%/os2.x86/release/bin/additions/VPoxSF.ifs ^
        /OS2/VPoxService.exe=%_MY_OPT_UNTAR_DIR%/os2.x86/release/bin/additions/VPoxService.exe ^
        /OS2/VPoxControl.exe=%_MY_OPT_UNTAR_DIR%/os2.x86/release/bin/additions/VPoxControl.exe ^
        /OS2/VPoxReplaceDll.exe=%_MY_OPT_UNTAR_DIR%/os2.x86/release/bin/additions/VPoxReplaceDll.exe ^
        /OS2/libc06.dll=%_MY_OPT_UNTAR_DIR%/os2.x86/release/bin/additions/libc06.dll ^
        /OS2/libc061.dll=%_MY_OPT_UNTAR_DIR%/os2.x86/release/bin/additions/libc061.dll ^
        /OS2/libc062.dll=%_MY_OPT_UNTAR_DIR%/os2.x86/release/bin/additions/libc062.dll ^
        /OS2/libc063.dll=%_MY_OPT_UNTAR_DIR%/os2.x86/release/bin/additions/libc063.dll ^
        /OS2/libc064.dll=%_MY_OPT_UNTAR_DIR%/os2.x86/release/bin/additions/libc064.dll ^
        /OS2/libc065.dll=%_MY_OPT_UNTAR_DIR%/os2.x86/release/bin/additions/libc065.dll ^
        /OS2/libc066.dll=%_MY_OPT_UNTAR_DIR%/os2.x86/release/bin/additions/libc066.dll ^
        /OS2/readme.txt=%_MY_OPT_UNTAR_DIR%/os2.x86/release/bin/additions/readme.txt ^
        /OS2/gengradd.dll=%_MY_OPT_UNTAR_DIR%/os2.x86/release/bin/additions/gengradd.dll ^
        /OS2/vpoxmouse.sys=%_MY_OPT_UNTAR_DIR%/os2.x86/release/bin/additions/vpoxmouse.sys ^
        /VPoxLinuxAdditions.run=%_MY_OPT_UNTAR_DIR%/linux.x86/release/bin/additions/VPoxLinuxAdditions.run ^
        /runasroot.sh=%_MY_OPT_UNTAR_DIR%/linux.x86/release/bin/additions/runasroot.sh ^
        /autorun.sh=%_MY_OPT_UNTAR_DIR%/linux.x86/release/bin/additions/autorun.sh ^
        /VPoxDarwinAdditions.pkg=%_MY_OPT_UNTAR_DIR%/darwin.amd64/release/dist/additions/VPoxGuestAdditions.pkg ^
        /VPoxDarwinAdditionsUninstall.tool=%_MY_OPT_UNTAR_DIR%/darwin.amd64/release/dist/additions/VPoxDarwinAdditionsUninstall.tool ^
        --chmod a+x:/VPoxLinuxAdditions.run  --chmod a+x:/runasroot.sh  --chmod a+x:/autorun.sh  --chmod a+x:/VPoxDarwinAdditionsUninstall.tool ^
        --volume-id="VPOXADDITIONS_@VPOX_VERSION_STRING@_@VPOX_SVN_REV@" ^
        --name-setup=joliet ^
        --volume-id="VPox_GAs_@VPOX_VERSION_STRING@"

if not exist %_MY_OPT_OUTDIR%/VPoxGuestAdditions.iso goto end_failed
call set _MY_OUT_FILES=%%VPoxGuestAdditions.iso

rem
rem That's that.
rem
echo **************************************************************************
echo * The third and final step is done.
echo *
echo * Successfully created:
for %%i in (%_MY_OUT_FILES%) do echo *    "%_MY_OPT_OUTDIR%\%%i"
goto end


:end_failed
@cd /d "%_MY_SAVED_CD%"
@endlocal
@endlocal
@echo * Failed!
@exit /b 1

:end
@cd /d "%_MY_SAVED_CD%"
@endlocal
@endlocal
