; $Id: VPoxGuestAdditionsUninstallOld.nsh $
;; @file
; VPoxGuestAdditionsUninstallOld.nsh - Guest Additions uninstallation handling for legacy packages.
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

!macro Uninstall_RunExtUnInstaller un
Function ${un}Uninstall_RunExtUnInstaller

  Pop  $0
  Push $1
  Push $2

  ; Try to run the current uninstaller
  StrCpy $1 "$0\uninst.exe"
  IfFileExists "$1" run 0
    MessageBox MB_OK "VirtualPox Guest Additions uninstaller not found! Path = $1" /SD IDOK
    StrCpy $0 1 ; Tell the caller that we were not able to start the uninstaller
    Goto exit

run:

  ; Always try to run in silent mode
  Goto run_uninst_silent

run_uninst_silent:

  ExecWait '"$1" /S _?=$0' $2 ; Silently run uninst.exe in it's dir and don't copy it to a temp. location
  Goto handle_result

run_uninst:

  ExecWait '"$1" _?=$0' $2 ; Run uninst.exe in it's dir and don't copy it to a temp. location
  Goto handle_result

handle_result:

  ; Note that here a race might going on after the user clicked on
  ; "Reboot now" in the installer ran above and this installer cleaning
  ; up afterwards

  ; ... so try to abort the current reboot / shutdown caused by the installer ran before
  Call ${un}AbortShutdown

;!ifdef _DEBUG
;      MessageBox MB_OK 'Debug Message: Uninstaller was called, result is: $2' /SD IDOK
;!endif

  ${Switch} $2 ; Check exit codes
    ${Case} 1  ; Aborted by user
      StrCpy $0 1 ; Tell the caller that we were aborted by the user
      ${Break}
    ${Case} 2  ; Aborted by script (that might be okay)
      StrCpy $0 0 ; All went well
      ${Break}
    ${Default} ; Normal exixt
      StrCpy $0 0 ; All went well
      ${Break}
  ${EndSwitch}
  Goto exit

exit:

  Pop $2
  Pop $1
  Push $0

FunctionEnd
!macroend
!insertmacro Uninstall_RunExtUnInstaller ""
!insertmacro Uninstall_RunExtUnInstaller "un."

!macro Uninstall_WipeInstallationDirectory un
Function ${un}Uninstall_WipeInstallationDirectory

  Pop  $0
  Push $1
  Push $2

  ; Do some basic sanity checks for not screwing up too fatal ...
  ${LogVerbose} "Removing old installation directory ($0) ..."
  ${If} $0    != $PROGRAMFILES
  ${AndIf} $0 != $PROGRAMFILES32
  ${AndIf} $0 != $PROGRAMFILES64
  ${AndIf} $0 != $COMMONFILES32
  ${AndIf} $0 != $COMMONFILES64
  ${AndIf} $0 != $WINDIR
  ${AndIf} $0 != $SYSDIR
    ${LogVerbose} "Wiping ($0) ..."
    Goto wipe
  ${EndIf}
  Goto wipe_abort

wipe:

  RMDir /r /REBOOTOK "$0"
  StrCpy $0 0 ; All went well
  Goto exit

wipe_abort:

  ${LogVerbose} "Won't remove directory ($0)!"
  StrCpy $0 1 ; Signal some failure
  Goto exit

exit:

  Pop $2
  Pop $1
  Push $0

FunctionEnd
!macroend
!insertmacro Uninstall_WipeInstallationDirectory ""
!insertmacro Uninstall_WipeInstallationDirectory "un."

; This function cleans up an old Sun installation
!macro Uninstall_Sun un
Function ${un}Uninstall_Sun

  Push $0
  Push $1
  Push $2

  ; Get current installation path
  ReadRegStr $0 HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Sun VirtualPox Guest Additions" "UninstallString"
  StrCmp $0 "" exit

  ; Extract path
  Push "$0"       ; String
  Push "\"        ; SubString
  Push "<"        ; SearchDirection
  Push "<"        ; StrInclusionDirection
  Push "0"        ; IncludeSubString
  Push "0"        ; Loops
  Push "0"        ; CaseSensitive
  Call ${un}StrStrAdv
  Pop $1          ; $1 only contains the full path

  StrCmp $1 "" exit

  ; Save current i8042prt info to new uninstall registry path
  ReadRegStr $0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Sun VirtualPox Guest Additions" ${ORG_MOUSE_PATH}
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" ${ORG_MOUSE_PATH} $0

  ; Try to wipe current installation directory
  Push $1 ; Push uninstaller path to stack
  Call ${un}Uninstall_WipeInstallationDirectory
  Pop $2  ; Get uninstaller exit code from stack
  StrCmp $2 0 common exit ; Only process common part if exit code is 0, otherwise exit

common:

  ; Make sure everything is cleaned up in case the old uninstaller did forget something
  DeleteRegKey HKLM "SOFTWARE\Sun\VirtualPox Guest Additions"
  DeleteRegKey /ifempty HKLM "SOFTWARE\Sun"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Sun VirtualPox Guest Additions"
  Delete /REBOOTOK "$1\netamd.inf"
  Delete /REBOOTOK "$1\pcntpci5.cat"
  Delete /REBOOTOK "$1\PCNTPCI5.sys"

  ; Try to remove old installation directory if empty
  RMDir /r /REBOOTOK "$SMPROGRAMS\Sun VirtualPox Guest Additions"
  RMDir /REBOOTOK "$1"

  ; Get original mouse driver info and restore it
  ;ReadRegStr $0 ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" ${ORG_MOUSE_PATH}
  ;WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\i8042prt" "ImagePath" $0
  ;Delete "$SYSDIR\drivers\VPoxMouseNT.sys"

  ; Delete vendor installation directory (only if completely empty)
!if $%BUILD_TARGET_ARCH% == "x86"       ; 32-bit
  RMDir /REBOOTOK "$PROGRAMFILES32\Sun"
!else   ; 64-bit
  RMDir /REBOOTOK "$PROGRAMFILES64\Sun"
!endif

exit:

  Pop $2
  Pop $1
  Pop $0

FunctionEnd
!macroend
!insertmacro Uninstall_Sun ""
!insertmacro Uninstall_Sun "un."

; This function cleans up an old xVM Sun installation
!macro Uninstall_SunXVM un
Function ${un}Uninstall_SunXVM

  Push $0
  Push $1
  Push $2

  ; Get current installation path
  ReadRegStr $0 HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Sun xVM VirtualPox Guest Additions" "UninstallString"
  StrCmp $0 "" exit

  ; Extract path
  Push "$0"       ; String
  Push "\"        ; SubString
  Push "<"        ; SearchDirection
  Push "<"        ; StrInclusionDirection
  Push "0"        ; IncludeSubString
  Push "0"        ; Loops
  Push "0"        ; CaseSensitive
  Call ${un}StrStrAdv
  Pop $1          ; $1 only contains the full path

  StrCmp $1 "" exit

  ; Save current i8042prt info to new uninstall registry path
  ReadRegStr $0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Sun xVM VirtualPox Guest Additions" ${ORG_MOUSE_PATH}
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" ${ORG_MOUSE_PATH} $0

  ; Try to wipe current installation directory
  Push $1 ; Push uninstaller path to stack
  Call ${un}Uninstall_WipeInstallationDirectory
  Pop $2  ; Get uninstaller exit code from stack
  StrCmp $2 0 common exit ; Only process common part if exit code is 0, otherwise exit

common:

  ; Make sure everything is cleaned up in case the old uninstaller did forget something
  DeleteRegKey HKLM "SOFTWARE\Sun\xVM VirtualPox Guest Additions"
  DeleteRegKey /ifempty HKLM "SOFTWARE\Sun"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Sun xVM VirtualPox Guest Additions"
  Delete /REBOOTOK "$1\netamd.inf"
  Delete /REBOOTOK "$1\pcntpci5.cat"
  Delete /REBOOTOK "$1\PCNTPCI5.sys"

  ; Try to remove old installation directory if empty
  RMDir /r /REBOOTOK "$SMPROGRAMS\Sun xVM VirtualPox Guest Additions"
  RMDir /REBOOTOK "$1"

  ; Delete vendor installation directory (only if completely empty)
!if $%BUILD_TARGET_ARCH% == "x86"       ; 32-bit
  RMDir /REBOOTOK "$PROGRAMFILES32\Sun"
!else   ; 64-bit
  RMDir /REBOOTOK "$PROGRAMFILES64\Sun"
!endif

  ; Get original mouse driver info and restore it
  ;ReadRegStr $0 ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" ${ORG_MOUSE_PATH}
  ;WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\i8042prt" "ImagePath" $0
  ;Delete "$SYSDIR\drivers\VPoxMouseNT.sys"

exit:

  Pop $2
  Pop $1
  Pop $0

FunctionEnd
!macroend
!insertmacro Uninstall_SunXVM ""
!insertmacro Uninstall_SunXVM "un."

; This function cleans up an old immotek installation
!macro Uninstall_Innotek un
Function ${un}Uninstall_Innotek

  Push $0
  Push $1
  Push $2

  ReadRegStr $0 HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\immotek VirtualPox Guest Additions" "UninstallString"
  StrCmp $0 "" exit

  ; Extract path
  Push "$0"       ; String
  Push "\"        ; SubString
  Push "<"        ; SearchDirection
  Push "<"        ; StrInclusionDirection
  Push "0"        ; IncludeSubString
  Push "0"        ; Loops
  Push "0"        ; CaseSensitive
  Call ${un}StrStrAdv
  Pop $1          ; $1 only contains the full path

  StrCmp $1 "" exit

  ; Save current i8042prt info to new uninstall registry path
  ReadRegStr $0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\immotek VirtualPox Guest Additions" ${ORG_MOUSE_PATH}
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" ${ORG_MOUSE_PATH} $0

  ; Try to wipe current installation directory
  Push $1 ; Push uninstaller path to stack
  Call ${un}Uninstall_WipeInstallationDirectory
  Pop $2  ; Get uninstaller exit code from stack
  StrCmp $2 0 common exit ; Only process common part if exit code is 0, otherwise exit

common:

  ; Remove left over files which were not entirely cached by the formerly running
  ; uninstaller
  DeleteRegKey HKLM "SOFTWARE\immotek\VirtualPox Guest Additions"
  DeleteRegKey HKLM "SOFTWARE\immotek"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\immotek VirtualPox Guest Additions"
  Delete /REBOOTOK "$1\VPoxGuestDrvInst.exe"
  Delete /REBOOTOK "$1\VPoxMouseInst.exe"
  Delete /REBOOTOK "$1\VPoxSFDrvInst.exe"
  Delete /REBOOTOK "$1\RegCleanup.exe"
  Delete /REBOOTOK "$1\VPoxService.exe"
  Delete /REBOOTOK "$1\VPoxMouseInst.exe"
  Delete /REBOOTOK "$1\immotek VirtualPox Guest Additions.url"
  Delete /REBOOTOK "$1\uninst.exe"
  Delete /REBOOTOK "$1\iexplore.ico"
  Delete /REBOOTOK "$1\install.log"
  Delete /REBOOTOK "$1\VBCoInst.dll"
  Delete /REBOOTOK "$1\VPoxControl.exe"
  Delete /REBOOTOK "$1\VPoxDisp.dll"
  Delete /REBOOTOK "$1\VPoxGINA.dll"
  Delete /REBOOTOK "$1\VPoxGuest.cat"
  Delete /REBOOTOK "$1\VPoxGuest.inf"
  Delete /REBOOTOK "$1\VPoxGuest.sys"
  Delete /REBOOTOK "$1\VPoxMouse.inf"
  Delete /REBOOTOK "$1\VPoxMouse.sys"
  Delete /REBOOTOK "$1\VPoxVideo.cat"
  Delete /REBOOTOK "$1\VPoxVideo.inf"
  Delete /REBOOTOK "$1\VPoxVideo.sys"

  ; Try to remove old installation directory if empty
  RMDir /r /REBOOTOK "$SMPROGRAMS\immotek VirtualPox Guest Additions"
  RMDir /REBOOTOK "$1"

  ; Delete vendor installation directory (only if completely empty)
!if $%BUILD_TARGET_ARCH% == "x86"       ; 32-bit
  RMDir /REBOOTOK "$PROGRAMFILES32\immotek"
!else   ; 64-bit
  RMDir /REBOOTOK "$PROGRAMFILES64\immotek"
!endif

  ; Get original mouse driver info and restore it
  ;ReadRegStr $0 ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" ${ORG_MOUSE_PATH}
  ;WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\i8042prt" "ImagePath" $0
  ;Delete "$SYSDIR\drivers\VPoxMouseNT.sys"

exit:

  Pop $2
  Pop $1
  Pop $0

FunctionEnd
!macroend
!insertmacro Uninstall_Innotek ""
!insertmacro Uninstall_Innotek "un."
