; $Id: VPoxGuestAdditionsNT4.nsh $
;; @file
; VPoxGuestAdditionsNT4.nsh - Guest Additions installation for NT4.
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

Function NT4_SetVideoResolution

  ; Check for all required parameters
  StrCmp $g_iScreenX "0" missingParms
  StrCmp $g_iScreenY "0" missingParms
  StrCmp $g_iScreenBpp "0" missingParms
  Goto haveParms

missingParms:

  ${LogVerbose} "Missing display parameters for NT4, setting default (640x480, 8 BPP) ..."

  StrCpy $g_iScreenX '640'   ; Default value
  StrCpy $g_iScreenY '480'   ; Default value
  StrCpy $g_iScreenBpp '8'   ; Default value

  ; Write setting into registry to show the desktop applet on next boot
  WriteRegStr HKLM "SYSTEM\CurrentControlSet\Control\GraphicsDrivers\NewDisplay" "" ""

haveParms:

  ${LogVerbose} "Setting display parameters for NT4 ($g_iScreenXx$g_iScreenY, $g_iScreenBpp BPP) ..."

  WriteRegDWORD HKLM "SYSTEM\CurrentControlSet\Hardware Profiles\Current\System\CurrentControlSet\Services\vpoxvideo\Device0" "DefaultSettings.BitsPerPel" $g_iScreenBpp
  WriteRegDWORD HKLM "SYSTEM\CurrentControlSet\Hardware Profiles\Current\System\CurrentControlSet\Services\vpoxvideo\Device0" "DefaultSettings.Flags" 0x00000000
  WriteRegDWORD HKLM "SYSTEM\CurrentControlSet\Hardware Profiles\Current\System\CurrentControlSet\Services\vpoxvideo\Device0" "DefaultSettings.VRefresh" 0x00000001
  WriteRegDWORD HKLM "SYSTEM\CurrentControlSet\Hardware Profiles\Current\System\CurrentControlSet\Services\vpoxvideo\Device0" "DefaultSettings.XPanning" 0x00000000
  WriteRegDWORD HKLM "SYSTEM\CurrentControlSet\Hardware Profiles\Current\System\CurrentControlSet\Services\vpoxvideo\Device0" "DefaultSettings.XResolution" $g_iScreenX
  WriteRegDWORD HKLM "SYSTEM\CurrentControlSet\Hardware Profiles\Current\System\CurrentControlSet\Services\vpoxvideo\Device0" "DefaultSettings.YPanning" 0x00000000
  WriteRegDWORD HKLM "SYSTEM\CurrentControlSet\Hardware Profiles\Current\System\CurrentControlSet\Services\vpoxvideo\Device0" "DefaultSettings.YResolution" $g_iScreenY

FunctionEnd

Function NT4_SaveMouseDriverInfo

  Push $0

  ; !!! NOTE !!!
  ; Due to some re-branding (see functions Uninstall_Sun, Uninstall_Innotek and
  ; Uninstall_SunXVM) the installer *has* to transport the very first saved i8042prt
  ; value to the current installer's "uninstall" directory in both mentioned
  ; functions above, otherwise NT4 will be screwed because it then would store
  ; "VPoxMouseNT.sys" as the original i8042prt driver which obviously isn't there
  ; after uninstallation anymore
  ; !!! NOTE !!!

  ; Save current mouse driver info so we may restore it on uninstallation
  ; But first check if we already installed the additions otherwise we will
  ; overwrite it with the VPoxMouseNT.sys
  ReadRegStr $0 HKLM "${PRODUCT_UNINST_KEY}" ${ORG_MOUSE_PATH}
  StrCmp $0 "" 0 exists

  ${LogVerbose} "Saving mouse driver info ..."
  ReadRegStr $0 HKLM "SYSTEM\CurrentControlSet\Services\i8042prt" "ImagePath"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" ${ORG_MOUSE_PATH} $0
  Goto exit

exists:

  ${LogVerbose} "Mouse driver info already saved."
  Goto exit

exit:

!ifdef _DEBUG
  ${LogVerbose} "Mouse driver info: $0"
!endif

  Pop $0

FunctionEnd

Function NT4_Prepare

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

FunctionEnd

Function NT4_CopyFiles

  ${LogVerbose} "Copying files for NT4 ..."

  SetOutPath "$INSTDIR"
  FILE "$%PATH_OUT%\bin\additions\VPoxGuestDrvInst.exe"
  AccessControl::SetOnFile "$INSTDIR\VPoxGuestDrvInst.exe" "(BU)" "GenericRead"
  FILE "$%PATH_OUT%\bin\additions\RegCleanup.exe"
  AccessControl::SetOnFile "$INSTDIR\RegCleanup.exe" "(BU)" "GenericRead"

  ; The files to install for NT 4, they go into the system directories
  SetOutPath "$SYSDIR"
  FILE "$%PATH_OUT%\bin\additions\VPoxDisp.dll"
  AccessControl::SetOnFile "$SYSDIR\VPoxDisp.dll" "(BU)" "GenericRead"
  FILE "$%PATH_OUT%\bin\additions\VPoxTray.exe"
  AccessControl::SetOnFile "$SYSDIR\VPoxTray.exe" "(BU)" "GenericRead"
  FILE "$%PATH_OUT%\bin\additions\VPoxHook.dll"
  AccessControl::SetOnFile "$SYSDIR\VPoxHook.dll" "(BU)" "GenericRead"
  FILE "$%PATH_OUT%\bin\additions\VPoxControl.exe"
  AccessControl::SetOnFile "$SYSDIR\VPoxControl.exe" "(BU)" "GenericRead"

  ; VPoxService
  FILE "$%PATH_OUT%\bin\additions\VPoxService.exe"
  AccessControl::SetOnFile "$SYSDIR\VPoxService.exe" "(BU)" "GenericRead"

  ; The drivers into the "drivers" directory
  SetOutPath "$SYSDIR\drivers"
  FILE "$%PATH_OUT%\bin\additions\VPoxVideo.sys"
  AccessControl::SetOnFile "$SYSDIR\drivers\VPoxVideo.sys" "(BU)" "GenericRead"
  FILE "$%PATH_OUT%\bin\additions\VPoxMouseNT.sys"
  AccessControl::SetOnFile "$SYSDIR\drivers\VPoxMouseNT.sys" "(BU)" "GenericRead"
  FILE "$%PATH_OUT%\bin\additions\VPoxGuest.sys"
  AccessControl::SetOnFile "$SYSDIR\drivers\VPoxGuest.sys" "(BU)" "GenericRead"
  ;FILE "$%PATH_OUT%\bin\additions\VPoxSFNT.sys" ; Shared Folders not available on NT4!
  ;AccessControl::SetOnFile "$SYSDIR\drivers\VPoxSFNT.sys" "(BU)" "GenericRead"

FunctionEnd

Function NT4_InstallFiles

  ${LogVerbose} "Installing drivers for NT4 ..."

  ; Install guest driver
  ${CmdExecute} "$\"$INSTDIR\VPoxDrvInst.exe$\" service create $\"VPoxGuest$\" $\"VPoxGuest Support Driver$\" 1 1 $\"$SYSDIR\drivers\VPoxGuest.sys$\" $\"Base$\"" "false"

  ; Bugfix: Set "Start" to 1, otherwise, VPoxGuest won't start on boot-up!
  ; Bugfix: Correct invalid "ImagePath" (\??\C:\WINNT\...)
  WriteRegDWORD HKLM "SYSTEM\CurrentControlSet\Services\VPoxGuest" "Start" 1
  WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\VPoxGuest" "ImagePath" "\SystemRoot\System32\DRIVERS\VPoxGuest.sys"

  ; Run VPoxTray when Windows NT starts
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "VPoxTray" '"$SYSDIR\VPoxTray.exe"'

  ; Video driver
  ${CmdExecute} "$\"$INSTDIR\VPoxGuestDrvInst.exe$\" /i" "false"

  ${LogVerbose} "Installing VirtualPox service ..."

  ; Create the VPoxService service
  ; No need to stop/remove the service here! Do this only on uninstallation!
  ${CmdExecute} "$\"$INSTDIR\VPoxDrvInst.exe$\" service create $\"VPoxService$\" $\"VirtualPox Guest Additions Service$\" 16 2 $\"%SystemRoot%\system32\VPoxService.exe$\" $\"Base$\"" "false"

   ; Create the Shared Folders service ...
  ;nsSCM::Install /NOUNLOAD "VPoxSF" "VirtualPox Shared Folders" 1 1 "$SYSDIR\drivers\VPoxSFNT.sys" "Network" "" "" ""
  ;Pop $0                      ; Ret value

!ifdef _DEBUG
  ;${LogVerbose} "SCM::Install VPoxSFNT.sys: $0"
!endif

  ;IntCmp $0 0 +1 error error  ; Check ret value (0=OK, 1=Error)

  ; ... and the link to the network provider
  ;WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\VPoxSF\NetworkProvider" "DeviceName" "\Device\VPoxMiniRdr"
  ;WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\VPoxSF\NetworkProvider" "Name" "VirtualPox Shared Folders"
  ;WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\VPoxSF\NetworkProvider" "ProviderPath" "$SYSDIR\VPoxMRXNP.dll"

  ; Add the shared folders network provider
  ;${CmdExecute} "$\"$INSTDIR\VPoxDrvInst.exe$\" netprovider add VPoxSF" "false"

  Goto done

error:
  Abort "ERROR: Could not install files for Windows NT4! Installation aborted."

done:

FunctionEnd

Function NT4_Main

  SetOutPath "$INSTDIR"

  Call NT4_Prepare
  Call NT4_CopyFiles

  ; This removes the flag "new display driver installed on the next bootup
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\RunOnce" "VPoxGuestInst" '"$INSTDIR\RegCleanup.exe"'

  Call NT4_SaveMouseDriverInfo
  Call NT4_InstallFiles
  Call NT4_SetVideoResolution

  ; Write mouse driver name to registry overwriting the default name
  WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\i8042prt" "ImagePath" "\SystemRoot\System32\DRIVERS\VPoxMouseNT.sys"

FunctionEnd

!macro NT4_UninstallInstDir un
Function ${un}NT4_UninstallInstDir

  ; Delete remaining files
  Delete /REBOOTOK "$INSTDIR\VPoxGuestDrvInst.exe"
  Delete /REBOOTOK "$INSTDIR\RegCleanup.exe"

FunctionEnd
!macroend
!insertmacro NT4_UninstallInstDir ""
!insertmacro NT4_UninstallInstDir "un."

!macro NT4_Uninstall un
Function ${un}NT4_Uninstall

  Push $0

  ; Remove the guest driver service
  ${CmdExecute} "$\"$INSTDIR\VPoxDrvInst.exe$\" service delete VPoxGuest" "true"
  Delete /REBOOTOK "$SYSDIR\drivers\VPoxGuest.sys"

  ; Delete the VPoxService service
  Call ${un}StopVPoxService
  ${CmdExecute} "$\"$INSTDIR\VPoxDrvInst.exe$\" service delete VPoxService" "true"
  DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "VPoxService"
  Delete /REBOOTOK "$SYSDIR\VPoxService.exe"

  ; Delete the VPoxTray app
  Call ${un}StopVPoxTray
  DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "VPoxTray"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\RunOnce" "VPoxTrayDel" "$SYSDIR\cmd.exe /c del /F /Q $SYSDIR\VPoxTray.exe"
  Delete /REBOOTOK "$SYSDIR\VPoxTray.exe" ; If it can't be removed cause it's running, try next boot with "RunOnce" key above!
  Delete /REBOOTOK "$SYSDIR\VPoxHook.dll"

  ; Delete the VPoxControl utility
  Delete /REBOOTOK "$SYSDIR\VPoxControl.exe"

  ; Delete the VPoxVideo service
  ${CmdExecute} "$\"$INSTDIR\VPoxDrvInst.exe$\" service delete VPoxVideo" "true"

  ; Delete the VPox video driver files
  Delete /REBOOTOK "$SYSDIR\drivers\VPoxVideo.sys"
  Delete /REBOOTOK "$SYSDIR\VPoxDisp.dll"

  ; Get original mouse driver info and restore it
  ReadRegStr $0 ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" ${ORG_MOUSE_PATH}
  ; If we still got our driver stored in $0 then this will *never* work, so
  ; warn the user and set it to the default driver to not screw up NT4 here
  ${If} $0 == "System32\DRIVERS\VPoxMouseNT.sys"
    WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\i8042prt" "ImagePath" "\SystemRoot\System32\DRIVERS\i8042prt.sys"
    ${LogVerbose} "Old mouse driver is set to VPoxMouseNT.sys, defaulting to i8042prt.sys ..."
  ${Else}
    WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\i8042prt" "ImagePath" $0
  ${EndIf}
  Delete /REBOOTOK "$SYSDIR\drivers\VPoxMouseNT.sys"

  Pop $0

FunctionEnd
!macroend
!insertmacro NT4_Uninstall ""
!insertmacro NT4_Uninstall "un."
