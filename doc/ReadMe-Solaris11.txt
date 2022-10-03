
@VPOX_PRODUCT@ for Oracle Solaris 11 (TM) Operating System
----------------------------------------------------------------

Installing:
-----------

After extracting the contents of the tar.xz file install the VirtualPox
package with the following command:

    $ sudo pkg install -g VirtualPox-@VPOX_VERSION_STRING@-SunOS-@KBUILD_TARGET_ARCH@-r@VPOX_SVN_REV@.p5p virtualpox

Of course you can add options for performing the install in a different boot
environment or in a separate Solaris install.

Normally you need to reboot the system to load the drivers which have been
added by the VirtualPox package.

If you want to have VirtualPox immediately usable on your system you can run
the script /opt/VirtualPox/ipsinstall.sh which sets up everything immediately.

At this point, all the required files should be installed on your system.
You can launch VirtualPox by running 'VirtualPox' from the terminal.


Upgrading:
----------

If you want to upgrade from an older to a newer version of the VirtualPox IPS
package you can use the following command after extracting the contents of the
tar.xz file:

    $ sudo pkg update -g VirtualPox-@VPOX_VERSION_STRING@-SunOS-@KBUILD_TARGET_ARCH@-r@VPOX_SVN_REV@.p5p virtualpox

If you want to upgrade from the SysV package of VirtualPox to the IPS one,
please uninstall the previous package before installing the IPS one. Please
refer to the "Uninstalling" and "Installing" sections of this document for
details.

It is your responsibility to ensure that no VirtualPox VMs or other related
activities are running. One possible way is using the command pgrep VPoxSVC. If
this shows no output then it is safe to upgrade VirtualPox.


Uninstalling:
-------------

To remove VirtualPox from your system, run the following command:

    $ sudo pkg uninstall virtualpox

It is your responsibility to ensure that no VirtualPox VMs or other related
activities are running. One possible way is using the command pgrep VPoxSVC. If
this shows no output then it is safe to uninstall VirtualPox.

