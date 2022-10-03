; $Id: German.nsh $
;; @file
; NLS for German language.
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

LangString VPOX_TEST ${LANG_GERMAN}                                 "Das ist eine Test-Nachricht von $(^Name)!"

LangString VPOX_NOADMIN ${LANG_GERMAN}                              "Sie benötigen Administrations-Rechte zum (De-)Installieren der $(^Name)!$\r$\nDas Setup wird nun beendet."

LangString VPOX_NOTICE_ARCH_X86 ${LANG_GERMAN}                      "Diese Applikation läuft nur auf 32-bit Windows-Systemen. Bitte installieren Sie die 64-bit Version der $(^Name)!"
LangString VPOX_NOTICE_ARCH_AMD64 ${LANG_GERMAN}                    "Diese Applikation läuft nur auf 64-bit Windows-Systemen. Bitte installieren Sie die 32-bit Version der $(^Name)!"
LangString VPOX_NT4_NO_SP6 ${LANG_GERMAN}                           "Es ist kein Service Pack 6 für NT 4.0 installiert.$\r$\nEs wird empfohlen das Service-Pack vor dieser Installation zu installieren. Trotzdem jetzt ohne Service-Pack installieren?"

LangString VPOX_PLATFORM_UNSUPPORTED ${LANG_GERMAN}                 "Diese Plattform wird noch nicht durch diese Guest Additions unterstützt!"

LangString VPOX_SUN_FOUND ${LANG_GERMAN}                            "Eine veraltete Version der Sun Guest Additions ist auf diesem System bereits installiert. Diese muss erst deinstalliert werden bevor aktuelle Guest Additions installiert werden können.$\r$\n$\r$\nJetzt die alten Guest Additions deinstallieren?"
LangString VPOX_SUN_ABORTED ${LANG_GERMAN}                          "Die Installation der Guest Additions kann nicht fortgesetzt werden.$\r$\nBitte deinstallieren Sie erst die alten Sun Guest Additions!"

LangString VPOX_INNOTEK_FOUND ${LANG_GERMAN}                        "Eine veraltete Version der immotek Guest Additions ist auf diesem System bereits installiert. Diese muss erst deinstalliert werden bevor aktuelle Guest Additions installiert werden können.$\r$\n$\r$\nJetzt die alten Guest Additions deinstallieren?"
LangString VPOX_INNOTEK_ABORTED ${LANG_GERMAN}                      "Die Installation der Guest Additions kann nicht fortgesetzt werden.$\r$\nBitte deinstallieren Sie erst die alten immotek Guest Additions!"

LangString VPOX_UNINSTALL_START ${LANG_GERMAN}                      "Auf OK klicken um mit der Deinstallation zu beginnen.$\r$\nBitte warten Sie dann während die Deinstallation im Hintergrund ausgeführt wird ..."
LangString VPOX_UNINSTALL_REBOOT ${LANG_GERMAN}                     "Es wird dringend empfohlen das System neu zu starten bevor die neuen Guest Additions installiert werden.$\r$\nBitte starten Sie die Installation nach dem Neustart erneut.$\r$\n$\r$\nJetzt neu starten?"

LangString VPOX_COMPONENT_MAIN ${LANG_GERMAN}                       "VirtualPox Guest Additions"
LangString VPOX_COMPONENT_MAIN_DESC ${LANG_GERMAN}                  "Hauptkomponenten der VirtualPox Guest Additions"

LangString VPOX_COMPONENT_AUTOLOGON ${LANG_GERMAN}                  "Unterstützung für automatisches Anmelden"
LangString VPOX_COMPONENT_AUTOLOGON_DESC ${LANG_GERMAN}             "Ermöglicht automatisches Anmelden von Benutzern"
LangString VPOX_COMPONENT_AUTOLOGON_WARN_3RDPARTY ${LANG_GERMAN}    "Es ist bereits eine Komponente für das automatische Anmelden installiert.$\r$\nFalls Sie diese Komponente nun mit der von VirtualPox ersetzen, könnte das System instabil werden.$\r$\nDennoch installieren?"

LangString VPOX_COMPONENT_D3D  ${LANG_GERMAN}                       "Direct3D-Unterstützung (Experimentell)"
LangString VPOX_COMPONENT_D3D_DESC  ${LANG_GERMAN}                  "Ermöglicht Direct3D-Unterstützung für Gäste (Experimentell)"
LangString VPOX_COMPONENT_D3D_NO_SM ${LANG_GERMAN}                  "Windows befindet sich aktuell nicht im abgesicherten Modus.$\r$\nDaher kann die D3D-Unterstützung nicht installiert werden."
LangString VPOX_COMPONENT_D3D_NOT_SUPPORTED ${LANG_GERMAN}          "Direct3D Gast-Unterstützung nicht verfügbar unter Windows $g_strWinVersion!"
;LangString VPOX_COMPONENT_D3D_HINT_VRAM ${LANG_GERMAN}              "Bitte beachten Sie, dass die virtuelle Maschine für die Benutzung von 3D-Beschleunigung einen Grafikspeicher von mindestens 128 MB für einen Monitor benötigt und für den Multi-Monitor-Betrieb bis zu 256 MB empfohlen wird.$\r$\n$\r$\nSie können den Grafikspeicher in den VM-Einstellungen in der Kategorie $\"Anzeige$\" ändern."
LangString VPOX_COMPONENT_D3D_INVALID ${LANG_GERMAN}                "Das Setup hat eine ungültige/beschädigte DirectX-Installation festgestellt.$\r$\n$\r$\nUm die Direct3D-Unterstützung installieren zu können wird empfohlen, zuerst das VirtualPox Benutzerhandbuch zu konsultieren.$\r$\n$\r$\nMit der Installation jetzt trotzdem fortfahren?"
LangString VPOX_COMPONENT_D3D_INVALID_MANUAL ${LANG_GERMAN}         "Soll nun das VirtualPox-Handbuch angezeigt werden um nach einer Lösung zu suchen?"

LangString VPOX_COMPONENT_STARTMENU ${LANG_GERMAN}                  "Startmenü-Einträge"
LangString VPOX_COMPONENT_STARTMENU_DESC ${LANG_GERMAN}             "Erstellt Einträge im Startmenü"

LangString VPOX_WFP_WARN_REPLACE ${LANG_GERMAN}                     "Das Setup hat gerade Systemdateien ersetzt um die ${PRODUCT_NAME} korrekt installieren zu können.$\r$\nFalls nun ein Warn-Dialog des Windows-Dateischutzes erscheint, diesen bitte abbrechen und die Dateien nicht wiederherstellen lassen!"
LangString VPOX_REBOOT_REQUIRED ${LANG_GERMAN}                      "Um alle Änderungen durchführen zu können, muss das System neu gestartet werden. Jetzt neu starten?"

LangString VPOX_EXTRACTION_COMPLETE ${LANG_GERMAN}                  "$(^Name): Die Dateien wurden erfolgreich nach $\"$INSTDIR$\" entpackt!"

LangString VPOX_ERROR_INST_FAILED ${LANG_GERMAN}                    "Es trat ein Fehler während der Installation auf!$\r$\nBitte werfen Sie einen Blick in die Log-Datei unter '$INSTDIR\install_ui.log' für mehr Informationen."
LangString VPOX_ERROR_OPEN_LINK ${LANG_GERMAN}                      "Link konnte nicht im Standard-Browser geöffnet werden."

LangString VPOX_UNINST_CONFIRM ${LANG_GERMAN}                       "Wollen Sie wirklich die $(^Name) deinstallieren?"
LangString VPOX_UNINST_SUCCESS ${LANG_GERMAN}                       "$(^Name) wurden erfolgreich deinstalliert."
LangString VPOX_UNINST_INVALID_D3D ${LANG_GERMAN}                   "Unvollständige oder ungültige Installation der Direct3D-Unterstützung erkannt; Deinstallation wird übersprungen."
LangString VPOX_UNINST_UNABLE_TO_RESTORE_D3D ${LANG_GERMAN}         "Konnte Direct3D-Originaldateien nicht wiederherstellen. Bitte DirectX neu installieren."
