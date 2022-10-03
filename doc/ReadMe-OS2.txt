
VirtualPox for OS/2 OSE Edition ALPHA
=====================================

Version 1.5.51_OSE_27858

03.02.2008


This is an early development build of VirtualPox OSE Edition for OS/2.
Please backup your data and don't expect everything to be highly polished
and tuned just yet. If you find a *new* problem, meaning something
not listed below, please report it at http://forums.virtualpox.org.

This package is an official unofficial build of VirtualPox for OS/2.
It means that it is coordinated by volunteers from Sun Microsystems that
are still in touch with OS/2 and keep patching VirtualPox at their spare
time to make sure it runs and more or less works under OS/2.

PLEASE NOTE THAT THE OS/2 HOST (AND THEREFORE THE OS/2 VERSION OF
VIRTUALPOX) IS NOT OFFICIALLY SUPPORTED BY SUN MICROSYSTEMS! DO NOT
CONTACT SUN MICROSYSTEMS REGARDING THE OS/2 VERSION OF VIRTUALPOX NO
MATTER WHAT YOUR QUESTION IS ABOUT! THANK YOU FOR UNDERSTANDING.


Current Issues / TODOs
----------------------

* FE/Qt (Qt GUI frontend):

  - Mouse pointer shape in mouse integration mode.
  - NumLock/ScrollLock synchronization.
  - Seamless mode (no top-level window transparency on OS/2).
  - Keyboard driver to intercept system key combinations
    (Alt+Tab etc.)

* Devices:

  - Host Floppy/DVD.
  - Audio.
  - Host interface networking.
  - Internal networking.
  - USB proxying.

* Misc:

  - Shared clipboard.
  - Starting more than one VM simultaneously.
  - Installer.
  - VMX support.
  - VPoxSDL (resizing/scaling/keyboard/slowness).
  - Very slow Resume after Pause in real mode guest applications.

Also, please pay attention to the section called "OS/2 Specific Features"
below.


How to "Install" and Run
------------------------

1. Unpack this archive somewhere.

2. Make sure you have a dot (.) in your LIBPATH statement in CONFIG.SYS.

3. Put the following line at the beginning of your CONFIG.SYS
   and reboot:

     DEVICE=<somewhere>\VPoxDrv.sys

4. Go to <somewhere> and run VirtualPox.exe (Qt GUI frontend).

5. Note that by default VirtualPox stores all user data in the
   %HOME%\.VirtualPox directory. If %HOME% is not set, it will use
   the <boot_drive>:\.VirtualPox directory. In either case, you may
   overwrite the location of this directory using the VPOX_USER_HOME
   environment variable.

6. For best performance, it is recommended to install the VirtualPox
   Guest Additions to the guest OS. The archive containing the ISO
   image with Guest Additions for supported guest OSes (Windows,
   Linux, OS/2) is named

     VPoxGuestAdditions_XXXXX.zip

   where XXXXX is the version number (it's best if it matches the version
   number of this VirtualPox package).

   Download this ZIP from the same location you took this archive from
   and unpack the contents to the directory containing VirtualPox.exe.
   After that, you can mount the Additions ISO in the Qt GUI by selecting
   Devices -> Install Guest Additions... from the menu.


Documentation and Support
-------------------------

Please visit http://www.virtualpox.org where you can find a lot of useful
information about VirtualPox. There is a Community section where you can
try to request some help from other OS/2 users of VirtualPox.

You can download the User Manual for the latest official release of
VirtualPox using this URL:

  http://www.virtualpox.org/download/UserManual.pdf


OS/2 Specific Features
----------------------

This section describes the features that are specific to the OS/2 version
of VirtualPox and may be absent in versions for other platforms.

1. System key combinations such as Alt+Tab, Ctrl+Esc are currently always
   grabbed by the host and never reach the guest even when the keyboard
   is captured. In order to send these combinations to the guest OS, use
   the following shortcuts (where Host is the host key defined in the
   global settings dialog):

   Host+` (Tilde/Backquote)  =>  Ctrl+Esc
   Host+1                    =>  Alt+Tab
   Host+2                    =>  Alt+Shift+Tab

2. If you use two or more keyboard layouts on the OS/2 host (e.g. English
   and Russian), make sure that the keyboard is switched to the English
   layer when you work in the VirtualPox VM console window. Otherwise, some
   shortcuts that involve the Host key (in particluar, all Host+<latin_letter>
   shortcuts like Host+Q) may not work. Please note that the guest keyboard
   layout has nothing to do with the host layout so you will still be able to
   switch layouts in the guest using its own means.

3. Make sure you do not do 'set LIBPATHSTRICT=T' in the environment you start
   VirtualPox from: it will make the VirtualPox keyboard hook screw up your
   host desktop (a workaround is to be found).


History of Changes
------------------

* 03.02.2008

  - Initial release.

* XX.XX.XXXX

  - Fixed: VirtualPox would hang or crash frequently on SMP machines in
    ACPI mode.

  - Fixed: VPoxSDL keyboard key event to scan code conversion [contributed
    by Paul Smedley].
