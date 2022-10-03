; $Id: VPoxGuestAdditionsW2KXP.nsh $
;; @file
; VPoxGuestAdditionsW2KXP.nsh - Guest Additions installation for Windows 2000/XP.
;

;
; Copyright (C) 2006-2020 Oracle Corporation
;
; This file is part of VirtualPox Open Source Edition (OSE), as
; available from http://www.virtualpox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License (GPL) as published by the Free Software
; Foundation, in version 2 as it comes in the "COPYING" file of the
; VirtualPox OSE distribution. VirtualPox OSE is distributed in the
; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;

Function W2K_SetVideoResolution

  ; NSIS only supports global vars, even in functions -- great
  Var /GLOBAL i
  Var /GLOBAL tmp
  Var /GLOBAL tmppath
  Var /GLOBAL dev_id
  Var /GLOBAL dev_desc

  ; Check for all required parameters
  StrCmp $g_iScreenX "0" exit
  StrCmp $g_iScreenY "0" exit
  StrCmp $g_iScreenBpp "0" exit

  ${LogVerbose} "Setting display parameters ($g_iScreenXx$g_iScreenY, $g_iScreenBpp BPP) ..."

  ; Enumerate all video devices (up to 32 at the moment, use key "MaxObjectNumber" key later)
  ${For} $i 0 32

    ReadRegStr $tmp HKLM "HARDWARE\DEVICEMAP\VIDEO" "\Device\Video$i"
    StrCmp $tmp "" dev_not_found

    ; Extract path to video settings
    ; Ex: \Registry\Machine\System\CurrentControlSet\Control\Video\{28B74D2B-F0A9-48E0-8028-D76F6BB1AE65}\0000
    ; Or: \Registry\Machine\System\CurrentControlSet\Control\Video\vpoxvideo\Device0
    ; Result: Machine\System\CurrentControlSet\Control\Video\{28B74D2B-F0A9-48E0-8028-D76F6BB1AE65}\0000
    Push "$tmp" ; String
    Push "\" ; SubString
    Push ">" ; SearchDirection
    Push ">" ; StrInclusionDirection
    Push "0" ; IncludeSubString
    Push "2" ; Loops
    Push "0" ; CaseSensitive
    Call StrStrAdv
    Pop $tmppath ; $1 only contains the full path
    StrCmp $tmppath "" dev_not_found

    ; Get device description
    ReadRegStr $dev_desc HKLM "$tmppath" "Device Description"
!ifdef _DEBUG
    ${LogVerbose} "Registry path: $tmppath"
    ${LogVerbose} "Registry path to device name: $temp"
!endif
    ${LogVerbose} "Detected video device: $dev_desc"

    ${If} $dev_desc == "VirtualPox Graphics Adapter"
      ${LogVerbose} "VirtualPox video device found!"
      Goto dev_found
    ${EndIf}
  ${Next}
  Goto dev_not_found

dev_found:

  ; If we're on Windows 2000, skip the ID detection ...
  ${If} $g_strWinVersion == "2000"
    Goto change_res
  ${EndIf}
  Goto dev_found_detect_id

dev_found_detect_id:

  StrCpy $i 0 ; Start at index 0
  ${LogVerbose} "Detecting device ID ..."

dev_found_detect_id_loop:

  ; Resolve real path to hardware instance "{GUID}"
  EnumRegKey $dev_id HKLM "SYSTEM\CurrentControlSet\Control\Video" $i
  StrCmp $dev_id "" dev_not_found ; No more entries? Jump out
!ifdef _DEBUG
  ${LogVerbose} "Got device ID: $dev_id"
!endif
  ReadRegStr $dev_desc HKLM "SYSTEM\CurrentControlSet\Control\Video\$dev_id\0000" "Device Description" ; Try to read device name
  ${If} $dev_desc == "VirtualPox Graphics Adapter"
    ${LogVerbose} "Device ID of $dev_desc: $dev_id"
    Goto change_res
  ${EndIf}

  IntOp $i $i + 1 ; Increment index
  goto dev_found_detect_id_loop

dev_not_found:

  ${LogVerbose} "No VirtualPox video device (yet) detected! No custom mode set."
  Goto exit

change_res:

!ifdef _DEBUG
  ${LogVerbose} "Device description: $dev_desc"
  ${LogVerbose} "Device ID: $dev_id"
!endif

  Var /GLOBAL reg_path_device
  Var /GLOBAL reg_path_monitor

  ${LogVerbose} "Custom mode set: Platform is Windows $g_strWinVersion"
  ${If} $g_strWinVersion == "2000"
  ${OrIf} $g_strWinVersion == "Vista"
    StrCpy $reg_path_device "SYSTEM\CurrentControlSet\SERVICES\VPoxVideo\Device0"
    StrCpy $reg_path_monitor "SYSTEM\CurrentControlSet\SERVICES\VPoxVideo\Device0\Mon00000001"
  ${ElseIf} $g_strWinVersion == "XP"
  ${OrIf} $g_strWinVersion == "7"
  ${OrIf} $g_strWinVersion == "8"
  ${OrIf} $g_strWinVersion == "8_1"
  ${OrIf} $g_strWinVersion == "10"
    StrCpy $reg_path_device "SYSTEM\CurrentControlSet\Control\Video\$dev_id\0000"
    StrCpy $reg_path_monitor "SYSTEM\CurrentControlSet\Control\VIDEO\$dev_id\0000\Mon00000001"
  ${Else}
    ${LogVerbose} "Custom mode set: Windows $g_strWinVersion not supported yet"
    Goto exit
  ${EndIf}

  ; Write the new value in the adapter config (VPoxVideo.sys) using hex values in binary format
  ${CmdExecute} "$\"$INSTDIR\VPoxDrvInst.exe$\" registry write HKLM $reg_path_device CustomXRes REG_BIN $g_iScreenX DWORD" "false"
  ${CmdExecute} "$\"$INSTDIR\VPoxDrvInst.exe$\" registry write HKLM $reg_path_device CustomYRes REG_BIN $g_iScreenY DWORD" "false"
  ${CmdExecute} "$\"$INSTDIR\VPoxDrvInst.exe$\" registry write HKLM $reg_path_device CustomBPP REG_BIN $g_iScreenBpp DWORD" "false"

  ; ... and tell Windows to use that mode on next start!
  WriteRegDWORD HKCC $reg_path_device "DefaultSettings.XResolution" "$g_iScreenX"
  WriteRegDWORD HKCC $reg_path_device "DefaultSettings.YResolution" "$g_iScreenY"
  WriteRegDWORD HKCC $reg_path_device "DefaultSettings.BitsPerPixel" "$g_iScreenBpp"

  WriteRegDWORD HKCC $reg_path_monitor "DefaultSettings.XResolution" "$g_iScreenX"
  WriteRegDWORD HKCC $reg_path_monitor "DefaultSettings.YResolution" "$g_iScreenY"
  WriteRegDWORD HKCC $reg_path_monitor "DefaultSettings.BitsPerPixel" "$g_iScreenBpp"

  ${LogVerbose} "Custom mode set to $g_iScreenXx$g_iScreenY, $g_iScreenBpp BPP on next restart."

exit:

FunctionEnd

Function W2K_Prepare

  ${If} $g_bNoVPoxServiceExit == "false"
    ; Stop / kill VPoxService
    Call StopVPoxService
  ${EndIf}

  ${If} $g_bNoVPoxTrayExit == "false"
    ; Stop / kill VPoxTray
    Call StopVPoxTray
  ${EndIf}

  ; Delete VPoxService from registry
  DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "VPoxService"

  ; Delete old VPoxService.exe from install directory (replaced by VPoxTray.exe)
  Delete /REBOOTOK "$INSTDIR\VPoxService.exe"

!ifdef VPOX_SIGN_ADDITIONS && VPOX_WITH_VPOX_LEGACY_TS_CA
  ; On guest OSes < Windows 10 we always go for the PreW10 drivers and install our legacy timestamp CA.
  ${If} $g_strWinVersion != "10"
    ${LogVerbose} "Installing legacy timestamp CA certificate ..."
    SetOutPath "$INSTDIR\cert"
    FILE "$%PATH_OUT%\bin\additions\vpox-legacy-timestamp-ca.cer"
    FILE "$%PATH_OUT%\bin\additions\VPoxCertUtil.exe"
    ${CmdExecute} "$\"$INSTDIR\cert\VPoxCertUtil.exe$\" add-trusted-publisher --root $\"$INSTDIR\cert\vpox-legacy-timestamp-ca.cer$\"" "true"
    ${CmdExecute} "$\"$INSTDIR\cert\VPoxCertUtil.exe$\" display-all" "true"
  ${EndIf}
!endif

FunctionEnd

Function W2K_CopyFiles

  Push $0
  SetOutPath "$INSTDIR"

  ; Video driver
  FILE "$%PATH_OUT%\bin\additions\VPoxVideo.sys"
  FILE "$%PATH_OUT%\bin\additions\VPoxDisp.dll"

  ; Mouse driver
  FILE "$%PATH_OUT%\bin\additions\VPoxMouse.sys"
  FILE "$%PATH_OUT%\bin\additions\VPoxMouse.inf"
!ifdef VPOX_SIGN_ADDITIONS
  ${If} $g_strWinVersion == "10"
    FILE "$%PATH_OUT%\bin\additions\VPoxMouse.cat"
  ${Else}
    FILE "/oname=VPoxMouse.cat" "$%PATH_OUT%\bin\additions\VPoxMouse-PreW10.cat"
  ${EndIf}
!endif

  ; Guest driver
  FILE "$%PATH_OUT%\bin\additions\VPoxGuest.sys"
  FILE "$%PATH_OUT%\bin\additions\VPoxGuest.inf"
!ifdef VPOX_SIGN_ADDITIONS
  ${If} $g_strWinVersion == "10"
    FILE "$%PATH_OUT%\bin\additions\VPoxGuest.cat"
  ${Else}
    FILE "/oname=VPoxGuest.cat" "$%PATH_OUT%\bin\additions\VPoxGuest-PreW10.cat"
  ${EndIf}
!endif

  ; Guest driver files
  FILE "$%PATH_OUT%\bin\additions\VPoxTray.exe"
  FILE "$%PATH_OUT%\bin\additions\VPoxControl.exe" ; Not used by W2K and up, but required by the .INF file

  ; WHQL fake
!ifdef WHQL_FAKE
  FILE "$%PATH_OUT%\bin\additions\VPoxWHQLFake.exe"
!endif

  SetOutPath $g_strSystemDir

  ; VPoxService
  ${If} $g_bNoVPoxServiceExit == "false"
    ; VPoxService has been terminated before, so just install the file
    ; in the regular way
    FILE "$%PATH_OUT%\bin\additions\VPoxService.exe"
  ${Else}
    ; VPoxService is in use and wasn't terminated intentionally. So extract the
    ; new version into a temporary location and install it on next reboot
    Push $0
    ClearErrors
    GetTempFileName $0
    IfErrors 0 +3
      ${LogVerbose} "Error getting temp file for VPoxService.exe"
      StrCpy "$0" "$INSTDIR\VPoxServiceTemp.exe"
    ${LogVerbose} "VPoxService is in use, will be installed on next reboot (from '$0')"
    File "/oname=$0" "$%PATH_OUT%\bin\additions\VPoxService.exe"
    IfErrors 0 +2
      ${LogVerbose} "Error copying VPoxService.exe to '$0'"
    Rename /REBOOTOK "$0" "$g_strSystemDir\VPoxService.exe"
    IfErrors 0 +2
      ${LogVerbose} "Error renaming '$0' to '$g_strSystemDir\VPoxService.exe'"
    Pop $0
  ${EndIf}

!if $%VPOX_WITH_WDDM% == "1"
  ${If} $g_bWithWDDM == "true"
    ; WDDM Video driver
    SetOutPath "$INSTDIR"

    !ifdef VPOX_SIGN_ADDITIONS
      ${If} $g_strWinVersion == "10"
        FILE "$%PATH_OUT%\bin\additions\VPoxWddm.cat"
      ${Else}
        FILE "/oname=VPoxWddm.cat" "$%PATH_OUT%\bin\additions\VPoxWddm-PreW10.cat"
      ${EndIf}
    !endif
    FILE "$%PATH_OUT%\bin\additions\VPoxWddm.sys"
    FILE "$%PATH_OUT%\bin\additions\VPoxWddm.inf"

    FILE "$%PATH_OUT%\bin\additions\VPoxDispD3D.dll"
    !if $%VPOX_WITH_MESA3D% == "1"
      FILE "$%PATH_OUT%\bin\additions\VPoxNine.dll"
      FILE "$%PATH_OUT%\bin\additions\VPoxSVGA.dll"
      FILE "$%PATH_OUT%\bin\additions\VPoxICD.dll"
      FILE "$%PATH_OUT%\bin\additions\VPoxGL.dll"
    !endif

    !if $%BUILD_TARGET_ARCH% == "amd64"
      FILE "$%PATH_OUT%\bin\additions\VPoxDispD3D-x86.dll"
      !if $%VPOX_WITH_MESA3D% == "1"
        FILE "$%PATH_OUT%\bin\additions\VPoxNine-x86.dll"
        FILE "$%PATH_OUT%\bin\additions\VPoxSVGA-x86.dll"
        FILE "$%PATH_OUT%\bin\additions\VPoxICD-x86.dll"
        FILE "$%PATH_OUT%\bin\additions\VPoxGL-x86.dll"
      !endif
    !endif ; $%BUILD_TARGET_ARCH% == "amd64"

    Goto doneCr
  ${EndIf}
!endif ; $%VPOX_WITH_WDDM% == "1"

doneCr:

  Pop $0

FunctionEnd

!ifdef WHQL_FAKE

Function W2K_WHQLFakeOn

  StrCmp $g_bFakeWHQL "true" do
  Goto exit

do:

  ${LogVerbose} "Turning off WHQL protection..."
  ${CmdExecute} "$\"$INSTDIR\VPoxWHQLFake.exe$\" $\"ignore$\"" "true"

exit:

FunctionEnd

Function W2K_WHQLFakeOff

  StrCmp $g_bFakeWHQL "true" do
  Goto exit

do:

  ${LogVerbose} "Turning back on WHQL protection..."
  ${CmdExecute} "$\"$INSTDIR\VPoxWHQLFake.exe$\" $\"warn$\"" "true"

exit:

FunctionEnd

!endif

Function W2K_InstallFiles

  ; The Shared Folder IFS goes to the system directory
  !if $%BUILD_TARGET_ARCH% == "x86"
    ; On x86 we have to use a different shared folder driver linked against an older RDBSS for Windows 7 and older.
    ${If} $g_strWinVersion == "2000"
    ${OrIf} $g_strWinVersion == "Vista"
    ${OrIf} $g_strWinVersion == "XP"
    ${OrIf} $g_strWinVersion == "7"
      !insertmacro ReplaceDLL "$%PATH_OUT%\bin\additions\VPoxSFW2K.sys" "$g_strSystemDir\drivers\VPoxSF.sys" "$INSTDIR"
    ${Else}
      !insertmacro ReplaceDLL "$%PATH_OUT%\bin\additions\VPoxSF.sys" "$g_strSystemDir\drivers\VPoxSF.sys" "$INSTDIR"
    ${EndIf}
  !else
    !insertmacro ReplaceDLL "$%PATH_OUT%\bin\additions\VPoxSF.sys" "$g_strSystemDir\drivers\VPoxSF.sys" "$INSTDIR"
  !endif

  !insertmacro ReplaceDLL "$%PATH_OUT%\bin\additions\VPoxMRXNP.dll" "$g_strSystemDir\VPoxMRXNP.dll" "$INSTDIR"
  AccessControl::GrantOnFile "$g_strSystemDir\VPoxMRXNP.dll" "(BU)" "GenericRead"
  !if $%BUILD_TARGET_ARCH% == "amd64"
    ; Only 64-bit installer: Copy the 32-bit DLL for 32 bit applications.
    !insertmacro ReplaceDLL "$%PATH_OUT%\bin\additions\VPoxMRXNP-x86.dll" "$g_strSysWow64\VPoxMRXNP.dll" "$INSTDIR"
    AccessControl::GrantOnFile "$g_strSysWow64\VPoxMRXNP.dll" "(BU)" "GenericRead"
  !endif

  ; The VPoxTray hook DLL also goes to the system directory; it might be locked
  !insertmacro ReplaceDLL "$%PATH_OUT%\bin\additions\VPoxHook.dll" "$g_strSystemDir\VPoxHook.dll" "$INSTDIR"
  AccessControl::GrantOnFile "$g_strSystemDir\VPoxHook.dll" "(BU)" "GenericRead"

  ${LogVerbose} "Installing drivers ..."

  Push $0 ; For fetching results

  SetOutPath "$INSTDIR"

  ${If} $g_bNoGuestDrv == "false"
    ${LogVerbose} "Installing guest driver ..."
    ${CmdExecute} "$\"$INSTDIR\VPoxDrvInst.exe$\" driver install $\"$INSTDIR\VPoxGuest.inf$\" $\"$INSTDIR\install_drivers.log$\"" "false"
  ${Else}
    ${LogVerbose} "Guest driver installation skipped!"
  ${EndIf}

  ${If} $g_bNoVideoDrv == "false"
    ${If} $g_bWithWDDM == "true"
      ${LogVerbose} "Installing WDDM video driver..."
      ${CmdExecute} "$\"$INSTDIR\VPoxDrvInst.exe$\" driver install $\"$INSTDIR\VPoxWddm.inf$\" $\"$INSTDIR\install_drivers.log$\"" "false"
    ${Else}
      ${LogVerbose} "Installing video driver ..."
      ${CmdExecute} "$\"$INSTDIR\VPoxDrvInst.exe$\" driver install $\"$INSTDIR\VPoxVideo.inf$\" $\"$INSTDIR\install_drivers.log$\"" "false"
    ${EndIf}
  ${Else}
    ${LogVerbose} "Video driver installation skipped!"
  ${EndIf}

  ${If} $g_bNoMouseDrv == "false"
    ${LogVerbose} "Installing mouse driver ..."
    ; The mouse filter does not contain any device IDs but a "DefaultInstall" section;
    ; so this .INF file needs to be installed using "InstallHinfSection" which is implemented
    ; with VPoxDrvInst's "driver executeinf" routine
    ${CmdExecute} "$\"$INSTDIR\VPoxDrvInst.exe$\" driver install $\"$INSTDIR\VPoxMouse.inf$\"" "false"
  ${Else}
    ${LogVerbose} "Mouse driver installation skipped!"
  ${EndIf}

  ; Create the VPoxService service
  ; No need to stop/remove the service here! Do this only on uninstallation!
  ${LogVerbose} "Installing VirtualPox service ..."
  ${CmdExecute} "$\"$INSTDIR\VPoxDrvInst.exe$\" service create $\"VPoxService$\" $\"VirtualPox Guest Additions Service$\" 16 2 $\"%SystemRoot%\System32\VPoxService.exe$\" $\"Base$\"" "false"

  ; Set service description
  WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\VPoxService" "Description" "Manages VM runtime information, time synchronization, remote sysprep execution and miscellaneous utilities for guest operating systems."

sf:

  ${LogVerbose} "Installing Shared Folders service ..."

  ; Create the Shared Folders service ...
  ; No need to stop/remove the service here! Do this only on uninstallation!
  ${CmdExecute} "$\"$INSTDIR\VPoxDrvInst.exe$\" service create $\"VPoxSF$\" $\"VirtualPox Shared Folders$\" 2 1 $\"\SystemRoot\System32\drivers\VPoxSF.sys$\" $\"NetworkProvider$\"" "false"

  ; ... and the link to the network provider
  WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\VPoxSF\NetworkProvider" "DeviceName" "\Device\VPoxMiniRdr"
  WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\VPoxSF\NetworkProvider" "Name" "VirtualPox Shared Folders"
  WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\VPoxSF\NetworkProvider" "ProviderPath" "$SYSDIR\VPoxMRXNP.dll"

  ; Add default network providers (if not present or corrupted)
  ${CmdExecute} "$\"$INSTDIR\VPoxDrvInst.exe$\" netprovider add WebClient" "false"
  ${CmdExecute} "$\"$INSTDIR\VPoxDrvInst.exe$\" netprovider add LanmanWorkstation" "false"
  ${CmdExecute} "$\"$INSTDIR\VPoxDrvInst.exe$\" netprovider add RDPNP" "false"

  ; Add the shared folders network provider
  ${LogVerbose} "Adding network provider (Order = $g_iSfOrder) ..."
  ${CmdExecute} "$\"$INSTDIR\VPoxDrvInst.exe$\" netprovider add VPoxSF $g_iSfOrder" "false"

  Goto done

done:

  Pop $0

FunctionEnd

Function W2K_Main

  SetOutPath "$INSTDIR"
  SetOverwrite on

  Call W2K_Prepare
  Call W2K_CopyFiles

!ifdef WHQL_FAKE
  Call W2K_WHQLFakeOn
!endif

  Call W2K_InstallFiles

!ifdef WHQL_FAKE
  Call W2K_WHQLFakeOff
!endif

  Call W2K_SetVideoResolution

FunctionEnd

!macro W2K_UninstallInstDir un
Function ${un}W2K_UninstallInstDir

  Delete /REBOOTOK "$INSTDIR\VPoxVideo.sys"
  Delete /REBOOTOK "$INSTDIR\VPoxVideo.inf"
  Delete /REBOOTOK "$INSTDIR\VPoxVideo.cat"
  Delete /REBOOTOK "$INSTDIR\VPoxDisp.dll"

  Delete /REBOOTOK "$INSTDIR\VPoxMouse.sys"
  Delete /REBOOTOK "$INSTDIR\VPoxMouse.inf"
  Delete /REBOOTOK "$INSTDIR\VPoxMouse.cat"

  Delete /REBOOTOK "$INSTDIR\VPoxTray.exe"

  Delete /REBOOTOK "$INSTDIR\VPoxGuest.sys"
  Delete /REBOOTOK "$INSTDIR\VPoxGuest.inf"
  Delete /REBOOTOK "$INSTDIR\VPoxGuest.cat"

  Delete /REBOOTOK "$INSTDIR\VBCoInst.dll" ; Deprecated, does not get installed anymore
  Delete /REBOOTOK "$INSTDIR\VPoxControl.exe"
  Delete /REBOOTOK "$INSTDIR\VPoxService.exe" ; Deprecated, does not get installed anymore

!if $%VPOX_WITH_WDDM% == "1"
  Delete /REBOOTOK "$INSTDIR\VPoxWddm.cat"
  Delete /REBOOTOK "$INSTDIR\VPoxWddm.sys"
  Delete /REBOOTOK "$INSTDIR\VPoxWddm.inf"
  ; Obsolete files begin
  Delete /REBOOTOK "$INSTDIR\VPoxVideoWddm.cat"
  Delete /REBOOTOK "$INSTDIR\VPoxVideoWddm.sys"
  Delete /REBOOTOK "$INSTDIR\VPoxVideoWddm.inf"
  Delete /REBOOTOK "$INSTDIR\VPoxVideoW8.cat"
  Delete /REBOOTOK "$INSTDIR\VPoxVideoW8.sys"
  Delete /REBOOTOK "$INSTDIR\VPoxVideoW8.inf"
  ; Obsolete files end
  Delete /REBOOTOK "$INSTDIR\VPoxDispD3D.dll"
  !if $%VPOX_WITH_MESA3D% == "1"
    Delete /REBOOTOK "$INSTDIR\VPoxNine.dll"
    Delete /REBOOTOK "$INSTDIR\VPoxSVGA.dll"
    Delete /REBOOTOK "$INSTDIR\VPoxICD.dll"
    Delete /REBOOTOK "$INSTDIR\VPoxGL.dll"
  !endif

    Delete /REBOOTOK "$INSTDIR\VPoxD3D9wddm.dll"
    Delete /REBOOTOK "$INSTDIR\wined3dwddm.dll"
    ; Try to delete libWine in case it is there from old installation
    Delete /REBOOTOK "$INSTDIR\libWine.dll"

  !if $%BUILD_TARGET_ARCH% == "amd64"
    Delete /REBOOTOK "$INSTDIR\VPoxDispD3D-x86.dll"
    !if $%VPOX_WITH_MESA3D% == "1"
      Delete /REBOOTOK "$INSTDIR\VPoxNine-x86.dll"
      Delete /REBOOTOK "$INSTDIR\VPoxSVGA-x86.dll"
      Delete /REBOOTOK "$INSTDIR\VPoxICD-x86.dll"
      Delete /REBOOTOK "$INSTDIR\VPoxGL-x86.dll"
    !endif

      Delete /REBOOTOK "$INSTDIR\VPoxD3D9wddm-x86.dll"
      Delete /REBOOTOK "$INSTDIR\wined3dwddm-x86.dll"
  !endif ; $%BUILD_TARGET_ARCH% == "amd64"
!endif ; $%VPOX_WITH_WDDM% == "1"

  ; WHQL fake
!ifdef WHQL_FAKE
  Delete /REBOOTOK "$INSTDIR\VPoxWHQLFake.exe"
!endif

  ; Log file
  Delete /REBOOTOK "$INSTDIR\install.log"
  Delete /REBOOTOK "$INSTDIR\install_ui.log"

FunctionEnd
!macroend
!insertmacro W2K_UninstallInstDir ""
!insertmacro W2K_UninstallInstDir "un."

!macro W2K_Uninstall un
Function ${un}W2K_Uninstall

  Push $0

  ; Remove VirtualPox video driver
  ${LogVerbose} "Uninstalling video driver ..."
  ${CmdExecute} "$\"$INSTDIR\VPoxDrvInst.exe$\" driver uninstall $\"$INSTDIR\VPoxVideo.inf$\"" "true"
  ${CmdExecute} "$\"$INSTDIR\VPoxDrvInst.exe$\" service delete VPoxVideo" "true"
  Delete /REBOOTOK "$g_strSystemDir\drivers\VPoxVideo.sys"
  Delete /REBOOTOK "$g_strSystemDir\VPoxDisp.dll"

  ; Remove video driver
!if $%VPOX_WITH_WDDM% == "1"

  ${LogVerbose} "Uninstalling WDDM video driver..."
  ${CmdExecute} "$\"$INSTDIR\VPoxDrvInst.exe$\" driver uninstall $\"$INSTDIR\VPoxWddm.inf$\"" "true"
  ${CmdExecute} "$\"$INSTDIR\VPoxDrvInst.exe$\" service delete VPoxWddm" "true"
  ;misha> @todo driver file removal (as well as service removal) should be done as driver package uninstall
  ;       could be done with "VPoxDrvInst.exe /u", e.g. by passing additional arg to it denoting that driver package is to be uninstalled
  Delete /REBOOTOK "$g_strSystemDir\drivers\VPoxWddm.sys"

  ; Obsolete files begin
  ${LogVerbose} "Uninstalling WDDM video driver for Windows 8..."
  ${CmdExecute} "$\"$INSTDIR\VPoxDrvInst.exe$\" driver uninstall $\"$INSTDIR\VPoxVideoW8.inf$\"" "true"
  ${CmdExecute} "$\"$INSTDIR\VPoxDrvInst.exe$\" service delete VPoxVideoW8" "true"
  ;misha> @todo driver file removal (as well as service removal) should be done as driver package uninstall
  ;       could be done with "VPoxDrvInst.exe /u", e.g. by passing additional arg to it denoting that driver package is to be uninstalled
  Delete /REBOOTOK "$g_strSystemDir\drivers\VPoxVideoW8.sys"

  ${LogVerbose} "Uninstalling WDDM video driver for Windows Vista and 7..."
  ${CmdExecute} "$\"$INSTDIR\VPoxDrvInst.exe$\" driver uninstall $\"$INSTDIR\VPoxVideoWddm.inf$\"" "true"
  ; Always try to remove both VPoxVideoWddm & VPoxVideo services no matter what is installed currently
  ${CmdExecute} "$\"$INSTDIR\VPoxDrvInst.exe$\" service delete VPoxVideoWddm" "true"
  ;misha> @todo driver file removal (as well as service removal) should be done as driver package uninstall
  ;       could be done with "VPoxDrvInst.exe /u", e.g. by passing additional arg to it denoting that driver package is to be uninstalled
  Delete /REBOOTOK "$g_strSystemDir\drivers\VPoxVideoWddm.sys"
  ; Obsolete files end

  Delete /REBOOTOK "$g_strSystemDir\VPoxDispD3D.dll"
  !if $%BUILD_TARGET_ARCH% == "amd64"
    Delete /REBOOTOK "$g_strSysWow64\VPoxDispD3D-x86.dll"
  !endif

  !if $%VPOX_WITH_MESA3D% == "1"
    Delete /REBOOTOK "$g_strSystemDir\VPoxNine.dll"
    Delete /REBOOTOK "$g_strSystemDir\VPoxSVGA.dll"
    Delete /REBOOTOK "$g_strSystemDir\VPoxICD.dll"
    Delete /REBOOTOK "$g_strSystemDir\VPoxGL.dll"

    !if $%BUILD_TARGET_ARCH% == "amd64"
      Delete /REBOOTOK "$g_strSysWow64\VPoxNine-x86.dll"
      Delete /REBOOTOK "$g_strSysWow64\VPoxSVGA-x86.dll"
      Delete /REBOOTOK "$g_strSysWow64\VPoxICD-x86.dll"
      Delete /REBOOTOK "$g_strSysWow64\VPoxGL-x86.dll"
    !endif
  !endif
!endif ; $%VPOX_WITH_WDDM% == "1"

  ; Remove mouse driver
  ${LogVerbose} "Removing mouse driver ..."
  ${CmdExecute} "$\"$INSTDIR\VPoxDrvInst.exe$\" service delete VPoxMouse" "true"
  Delete /REBOOTOK "$g_strSystemDir\drivers\VPoxMouse.sys"
  ${CmdExecute} "$\"$INSTDIR\VPoxDrvInst.exe$\" registry delmultisz $\"SYSTEM\CurrentControlSet\Control\Class\{4D36E96F-E325-11CE-BFC1-08002BE10318}$\" $\"UpperFilters$\" $\"VPoxMouse$\"" "true"

  ; Delete the VPoxService service
  Call ${un}StopVPoxService
  ${CmdExecute} "$\"$INSTDIR\VPoxDrvInst.exe$\" service delete VPoxService" "true"
  DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "VPoxService"
  Delete /REBOOTOK "$g_strSystemDir\VPoxService.exe"

  ; VPoxGINA
  Delete /REBOOTOK "$g_strSystemDir\VPoxGINA.dll"
  ReadRegStr $0 HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion\WinLogon" "GinaDLL"
  ${If} $0 == "VPoxGINA.dll"
    ${LogVerbose} "Removing auto-logon support ..."
    DeleteRegValue HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion\WinLogon" "GinaDLL"
  ${EndIf}
  DeleteRegKey HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion\WinLogon\Notify\VPoxGINA"

  ; Delete VPoxTray
  Call ${un}StopVPoxTray
  DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "VPoxTray"

  ; Remove guest driver
  ${LogVerbose} "Removing guest driver ..."
  ${CmdExecute} "$\"$INSTDIR\VPoxDrvInst.exe$\" driver uninstall $\"$INSTDIR\VPoxGuest.inf$\"" "true"

  ${CmdExecute} "$\"$INSTDIR\VPoxDrvInst.exe$\" service delete VPoxGuest" "true"
  Delete /REBOOTOK "$g_strSystemDir\drivers\VPoxGuest.sys"
  Delete /REBOOTOK "$g_strSystemDir\VBCoInst.dll" ; Deprecated, does not get installed anymore
  Delete /REBOOTOK "$g_strSystemDir\VPoxTray.exe"
  Delete /REBOOTOK "$g_strSystemDir\VPoxHook.dll"
  DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "VPoxTray" ; Remove VPoxTray autorun
  Delete /REBOOTOK "$g_strSystemDir\VPoxControl.exe"

  ; Remove shared folders driver
  ${LogVerbose} "Removing shared folders driver ..."
  ${CmdExecute} "$\"$INSTDIR\VPoxDrvInst.exe$\" netprovider remove VPoxSF" "true"
  ${CmdExecute} "$\"$INSTDIR\VPoxDrvInst.exe$\" service delete VPoxSF" "true"
  Delete /REBOOTOK "$g_strSystemDir\VPoxMRXNP.dll" ; The network provider DLL will be locked
  !if $%BUILD_TARGET_ARCH% == "amd64"
    ; Only 64-bit installer: Also remove 32-bit DLLs on 64-bit target arch in Wow64 node
    Delete /REBOOTOK "$g_strSysWow64\VPoxMRXNP.dll"
  !endif ; amd64
  Delete /REBOOTOK "$g_strSystemDir\drivers\VPoxSF.sys"

  Pop $0

FunctionEnd
!macroend
!insertmacro W2K_Uninstall ""
!insertmacro W2K_Uninstall "un."

