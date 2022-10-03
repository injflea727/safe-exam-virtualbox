$Id: vpoxtxs-runvm-readme.txt $


VirtualPox Test eXecution Service
=================================

This readme briefly describes how to install the Test eXecution Service (TXS)
for nested hardware-virtualization smoke testing on the various systems.

The basic idea is to execute one smoke test within the VM and then launch
the regular TXS service in the VM to report success or failure to the host.

Linux Installation
------------------

1.  scp/download latest release build of VirtualPox and install it in the VM.
2.  scp/download the required smoke test VDI from remote test-resource to
    /home/vpox/testrsrc/3.0/tcp/win2k3ent-acpi.vdi
3.  cd /root
3.  scp/download VPoxValidationKit*.zip there.
5.  unzip VPoxValidationKit*.zip
6.  chmod -R u+w,a+x /opt/validationkit/
7a. Gnome: Copy /opt/validationkit/linux/vpoxtxs-runvm.desktop to /etc/xdg/autostart
7b. KDE/Others: TODO: Document other desktop managers
8.  Add the vpox user to sudo group using:
        sudo usermod -a -G sudo vpox
9.  Ensure no password is required for vpox when using sudo:
        sudo echo "vpox ALL=(ALL:ALL) NOPASSWD: ALL" > /etc/sudoers.d/username
10. Check the cdrom location and /dev/kmsg equivalent of your linux distro
    in /opt/validationkit/linux/vpoxtxs-runvm and fix it so it's correct
11. Reboot / done.

TODO: Document other OSes as we add them.

Note: vpoxtxs-runvm uses a GUI session to launch the nested-VM for better
visibility when troubleshooting the nested smoke test.

If this causes problems try troubleshooting the XAUTHORITY and DISPLAY
environment variables in vpoxtxs-runvm.service. It might differ depending
on the display manager of the particular linux distro.



Testing the setup
-----------------

1. Make sure the validationkit.iso is inserted.
2. Boot / reboot the guest.
3. To test the connection - Depending on the TXS transport options:
      nat)   python testdriver/tst-txsclient.py --reversed-setup
      other) python testdriver/tst-txsclient.py --hostname <guest-ip>
4. To test the smoke test:
      python tests/smoketests/tdSmokeTest1.py -v -v -d --vpox-session-type gui --test-vms <guest-name>

