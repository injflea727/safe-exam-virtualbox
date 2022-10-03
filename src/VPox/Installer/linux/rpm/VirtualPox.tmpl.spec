# $Id: VirtualPox.tmpl.spec $
## @file
# Spec file for creating VirtualPox rpm packages
#

#
# Copyright (C) 2006-2020 Oracle Corporation
#
# This file is part of VirtualPox Open Source Edition (OSE), as
# available from http://www.virtualpox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualPox OSE distribution. VirtualPox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

%define %SPEC% 1
%define %OSE% 1
%define %PYTHON% 1
%define %CHM% 1
%define VPOXDOCDIR %{_defaultdocdir}/%NAME%
%global __requires_exclude_from ^/usr/lib/virtualpox/VPoxPython.*$|^/usr/lib/python.*$|^.*\\.py$
%{!?python_sitelib: %define python_sitelib %(%{__python} -c "from distutils.sysconfig import get_python_lib; print(get_python_lib())")}

Summary:   Oracle VM VirtualPox
Name:      %NAME%
Version:   %BUILDVER%_%BUILDREL%
Release:   1
URL:       http://www.virtualpox.org/
Source:    VirtualPox.tar
License:   GPLv2
Group:     Applications/System
Vendor:    Oracle Corporation
BuildRoot: %BUILDROOT%
Requires:  %INITSCRIPTS% %LIBASOUND% %NETTOOLS%

%if %{?rpm_suse:1}%{!?rpm_suse:0}
%debug_package
%endif

%MACROSPYTHON%
%if %{?__python3:1}%{!?__python3:0}
%define vpox_python %{__python3}
%define vpox_python_sitelib %{python3_sitelib}
%else
%define vpox_python %{__python}
%{?rpm_suse: %define vpox_python_sitelib %{py_sitedir}}
%{!?rpm_suse: %define vpox_python_sitelib %{python_sitelib}}
%endif

# our Qt5 libs are built on EL5 with ld 2.17 which does not provide --link-id=
%undefine _missing_build_ids_terminate_build

# Remove source code from debuginfo package, needed for Fedora 27 and later
# as we build the binaries before creating the RPMs.
%if 0%{?fedora} >= 27
%undefine _debugsource_packages
%undefine _debuginfo_subpackages
%endif
%if 0%{?rhel} >= 8
%undefine _debugsource_packages
%undefine _debuginfo_subpackages
%endif

%description
VirtualPox is a powerful PC virtualization solution allowing
you to run a wide range of PC operating systems on your Linux
system. This includes Windows, Linux, FreeBSD, DOS, OpenBSD
and others. VirtualPox comes with a broad feature set and
excellent performance, making it the premier virtualization
software solution on the market.


%prep
%setup -q
DESTDIR=""
unset DESTDIR


%build


%install
# Mandriva: prevent replacing 'echo' by 'gprintf'
export DONT_GPRINTIFY=1
rm -rf $RPM_BUILD_ROOT
install -m 755 -d $RPM_BUILD_ROOT/sbin
install -m 755 -d $RPM_BUILD_ROOT%{_initrddir}
install -m 755 -d $RPM_BUILD_ROOT/lib/modules
install -m 755 -d $RPM_BUILD_ROOT/etc/vpox
install -m 755 -d $RPM_BUILD_ROOT/usr/bin
install -m 755 -d $RPM_BUILD_ROOT/usr/src
install -m 755 -d $RPM_BUILD_ROOT/usr/share/applications
install -m 755 -d $RPM_BUILD_ROOT/usr/share/pixmaps
install -m 755 -d $RPM_BUILD_ROOT/usr/share/icons/hicolor
install -m 755 -d $RPM_BUILD_ROOT%{VPOXDOCDIR}
install -m 755 -d $RPM_BUILD_ROOT/usr/lib/virtualpox
install -m 755 -d $RPM_BUILD_ROOT/usr/share/virtualpox
install -m 755 -d $RPM_BUILD_ROOT/usr/share/mime/packages
%if %{?with_python:1}%{!?with_python:0}
(export VPOX_INSTALL_PATH=/usr/lib/virtualpox && \
  cd ./sdk/installer && \
  %{vpox_python} ./vpoxapisetup.py install --prefix %{_prefix} --root $RPM_BUILD_ROOT)
%endif
rm -rf sdk/installer
mv UnattendedTemplates $RPM_BUILD_ROOT/usr/share/virtualpox
mv nls $RPM_BUILD_ROOT/usr/share/virtualpox
cp -a src $RPM_BUILD_ROOT/usr/share/virtualpox
mv VPox.sh $RPM_BUILD_ROOT/usr/bin/VPox
mv VPoxSysInfo.sh $RPM_BUILD_ROOT/usr/share/virtualpox
cp icons/128x128/virtualpox.png $RPM_BUILD_ROOT/usr/share/pixmaps/virtualpox.png
cd icons
  for i in *; do
    if [ -f $i/virtualpox.* ]; then
      install -d $RPM_BUILD_ROOT/usr/share/icons/hicolor/$i/apps
      mv $i/virtualpox.* $RPM_BUILD_ROOT/usr/share/icons/hicolor/$i/apps
    fi
    install -d $RPM_BUILD_ROOT/usr/share/icons/hicolor/$i/mimetypes
    mv $i/* $RPM_BUILD_ROOT/usr/share/icons/hicolor/$i/mimetypes || true
    rmdir $i
  done
cd -
rmdir icons
mv virtualpox.xml $RPM_BUILD_ROOT/usr/share/mime/packages
mv VPoxTunctl $RPM_BUILD_ROOT/usr/bin
%if %{?is_ose:0}%{!?is_ose:1}
for d in /lib/modules/*; do
  if [ -L $d/build ]; then
    rm -f /tmp/vpoxdrv-Module.symvers
    ./src/vpoxhost/build_in_tmp \
      --save-module-symvers /tmp/vpoxdrv-Module.symvers \
      --module-source `pwd`/src/vpoxhost/vpoxdrv \
      KBUILD_VERBOSE= KERN_VER=$(basename $d) INSTALL_MODULE_PATH=$RPM_BUILD_ROOT -j4 \
      %INSTMOD%
    ./src/vpoxhost/build_in_tmp \
      --use-module-symvers /tmp/vpoxdrv-Module.symvers \
      --module-source `pwd`/src/vpoxhost/vpoxnetflt \
      KBUILD_VERBOSE= KERN_VER=$(basename $d) INSTALL_MODULE_PATH=$RPM_BUILD_ROOT -j4 \
      %INSTMOD%
    ./src/vpoxhost/build_in_tmp \
      --use-module-symvers /tmp/vpoxdrv-Module.symvers \
      --module-source `pwd`/src/vpoxhost/vpoxnetadp \
      KBUILD_VERBOSE= KERN_VER=$(basename $d) INSTALL_MODULE_PATH=$RPM_BUILD_ROOT -j4 \
      %INSTMOD%
    if [ -e `pwd`/src/vpoxhost/vpoxpci ]; then
      ./src/vpoxhost/build_in_tmp \
        --use-module-symvers /tmp/vpoxdrv-Module.symvers \
        --module-source `pwd`/src/vpoxhost/vpoxpci \
        KBUILD_VERBOSE= KERN_VER=$(basename $d) INSTALL_MODULE_PATH=$RPM_BUILD_ROOT -j4 \
        %INSTMOD%
    fi
  fi
done
rm -r src
%endif
%if %{?is_ose:0}%{!?is_ose:1}
  for i in rdesktop-vrdp.tar.gz rdesktop-vrdp-keymaps; do
    mv $i $RPM_BUILD_ROOT/usr/share/virtualpox; done
  # Very little needed tool causing python compatibility trouble. Do not ship.
  rm -f $RPM_BUILD_ROOT/usr/share/virtualpox/rdesktop-vrdp-keymaps/convert-map
  mv rdesktop-vrdp $RPM_BUILD_ROOT/usr/bin
%endif
for i in additions/VPoxGuestAdditions.iso; do
  mv $i $RPM_BUILD_ROOT/usr/share/virtualpox; done
ln -s VPox $RPM_BUILD_ROOT/usr/bin/VirtualPox
ln -s VPox $RPM_BUILD_ROOT/usr/bin/virtualpox
ln -s VPox $RPM_BUILD_ROOT/usr/bin/VirtualPoxVM
ln -s VPox $RPM_BUILD_ROOT/usr/bin/virtualpoxvm
ln -s VPox $RPM_BUILD_ROOT/usr/bin/VPoxManage
ln -s VPox $RPM_BUILD_ROOT/usr/bin/vpoxmanage
test -f VPoxSDL && ln -s VPox $RPM_BUILD_ROOT/usr/bin/VPoxSDL
test -f VPoxSDL && ln -s VPox $RPM_BUILD_ROOT/usr/bin/vpoxsdl
ln -s VPox $RPM_BUILD_ROOT/usr/bin/VPoxVRDP
ln -s VPox $RPM_BUILD_ROOT/usr/bin/VPoxHeadless
ln -s VPox $RPM_BUILD_ROOT/usr/bin/vpoxheadless
ln -s VPox $RPM_BUILD_ROOT/usr/bin/VPoxDTrace
ln -s VPox $RPM_BUILD_ROOT/usr/bin/vpoxdtrace
ln -s VPox $RPM_BUILD_ROOT/usr/bin/VPoxBugReport
ln -s VPox $RPM_BUILD_ROOT/usr/bin/vpoxbugreport
ln -s VPox $RPM_BUILD_ROOT/usr/bin/VPoxBalloonCtrl
ln -s VPox $RPM_BUILD_ROOT/usr/bin/vpoxballoonctrl
ln -s VPox $RPM_BUILD_ROOT/usr/bin/VPoxAutostart
ln -s VPox $RPM_BUILD_ROOT/usr/bin/vpoxautostart
test -f vpoxwebsrv && ln -s VPox $RPM_BUILD_ROOT/usr/bin/vpoxwebsrv
ln -s /usr/lib/virtualpox/vpox-img $RPM_BUILD_ROOT/usr/bin/vpox-img
ln -s /usr/lib/virtualpox/vpoximg-mount $RPM_BUILD_ROOT/usr/bin/vpoximg-mount
ln -s /usr/share/virtualpox/src/vpoxhost $RPM_BUILD_ROOT/usr/src/vpoxhost-%VER%
mv virtualpox.desktop $RPM_BUILD_ROOT/usr/share/applications/virtualpox.desktop
mv VPox.png $RPM_BUILD_ROOT/usr/share/pixmaps/VPox.png
%{!?is_ose: mv LICENSE $RPM_BUILD_ROOT%{VPOXDOCDIR}}
mv UserManual*.pdf $RPM_BUILD_ROOT%{VPOXDOCDIR}
%{?with_chm: mv VirtualPox*.chm $RPM_BUILD_ROOT%{VPOXDOCDIR}}
install -m 755 -d $RPM_BUILD_ROOT/usr/lib/debug/usr/lib/virtualpox
%if %{?rpm_suse:1}%{!?rpm_suse:0}
rm *.debug
%else
mv *.debug $RPM_BUILD_ROOT/usr/lib/debug/usr/lib/virtualpox
%endif
mv * $RPM_BUILD_ROOT/usr/lib/virtualpox
if [ -f $RPM_BUILD_ROOT/usr/lib/virtualpox/libQt5CoreVPox.so.5 ]; then
  $RPM_BUILD_ROOT/usr/lib/virtualpox/chrpath --keepgoing --replace /usr/lib/virtualpox \
    $RPM_BUILD_ROOT/usr/lib/virtualpox/*.so.5 \
    $RPM_BUILD_ROOT/usr/lib/virtualpox/plugins/platforms/*.so \
    $RPM_BUILD_ROOT/usr/lib/virtualpox/plugins/xcbglintegrations/*.so || true
  echo "[Paths]" > $RPM_BUILD_ROOT/usr/lib/virtualpox/qt.conf
  echo "Plugins = /usr/lib/virtualpox/plugins" >> $RPM_BUILD_ROOT/usr/lib/virtualpox/qt.conf
fi
if [ -d $RPM_BUILD_ROOT/usr/lib/virtualpox/legacy ]; then
  mv $RPM_BUILD_ROOT/usr/lib/virtualpox/legacy/* $RPM_BUILD_ROOT/usr/lib/virtualpox
  rmdir $RPM_BUILD_ROOT/usr/lib/virtualpox/legacy
fi
rm -f $RPM_BUILD_ROOT/usr/lib/virtualpox/chrpath
ln -s ../VPoxVMM.so $RPM_BUILD_ROOT/usr/lib/virtualpox/components/VPoxVMM.so
for i in VPoxHeadless VPoxNetDHCP VPoxNetNAT VPoxNetAdpCtl; do
  chmod 4511 $RPM_BUILD_ROOT/usr/lib/virtualpox/$i; done
if test -e $RPM_BUILD_ROOT/usr/lib/virtualpox/VirtualPoxVM; then
  chmod 4511 $RPM_BUILD_ROOT/usr/lib/virtualpox/VirtualPoxVM
else
  chmod 4511 $RPM_BUILD_ROOT/usr/lib/virtualpox/VirtualPox
fi
if [ -f $RPM_BUILD_ROOT/usr/lib/virtualpox/VPoxVolInfo ]; then
  chmod 4511 $RPM_BUILD_ROOT/usr/lib/virtualpox/VPoxVolInfo
fi
test -f $RPM_BUILD_ROOT/usr/lib/virtualpox/VPoxSDL && \
  chmod 4511 $RPM_BUILD_ROOT/usr/lib/virtualpox/VPoxSDL
%if %{?with_python:1}%{!?with_python:0}
if [ -x /usr/bin/pathfix.py ]; then
  /usr/bin/pathfix.py -pni "%{__python3} %{py3_shbang_opts}" $RPM_BUILD_ROOT/usr/lib/virtualpox/vpoxshell.py
fi
%endif


%pre
# defaults
[ -r /etc/default/virtualpox ] && . /etc/default/virtualpox

# check for old installation
if [ -r /etc/vpox/vpox.cfg ]; then
  . /etc/vpox/vpox.cfg
  if [ "x$INSTALL_DIR" != "x" -a -d "$INSTALL_DIR" ]; then
    echo "An old installation of VirtualPox was found. To install this package the"
    echo "old package has to be removed first. Have a look at /etc/vpox/vpox.cfg to"
    echo "determine the installation directory of the previous installation. After"
    echo "uninstalling the old package remove the file /etc/vpox/vpox.cfg."
    exit 1
  fi
fi

# check for active VMs of the installed (old) package
# Execute the installed packages pre-uninstaller if present.
/usr/lib/virtualpox/prerm-common.sh 2>/dev/null
# Stop services from older versions without pre-uninstaller.
/etc/init.d/vpoxballoonctrl-service stop 2>/dev/null
/etc/init.d/vpoxautostart-service stop 2>/dev/null
/etc/init.d/vpoxweb-service stop 2>/dev/null
VPOXSVC_PID=`pidof VPoxSVC 2>/dev/null || true`
if [ -n "$VPOXSVC_PID" ]; then
  # ask the daemon to terminate immediately
  kill -USR1 $VPOXSVC_PID
  sleep 1
  if pidof VPoxSVC > /dev/null 2>&1; then
    echo "A copy of VirtualPox is currently running.  Please close it and try again."
    echo "Please note that it can take up to ten seconds for VirtualPox (in particular"
    echo "the VPoxSVC daemon) to finish running."
    exit 1
  fi
fi


%post
LOG="/var/log/vpox-install.log"

# defaults
[ -r /etc/default/virtualpox ] && . /etc/default/virtualpox

# remove old cruft
if [ -f /etc/init.d/vpoxdrv.sh ]; then
  echo "Found old version of /etc/init.d/vpoxdrv.sh, removing."
  rm /etc/init.d/vpoxdrv.sh
fi
if [ -f /etc/vpox/vpox.cfg ]; then
  echo "Found old version of /etc/vpox/vpox.cfg, removing."
  rm /etc/vpox/vpox.cfg
fi
rm -f /etc/vpox/module_not_compiled

# create users groups (disable with INSTALL_NO_GROUP=1 in /etc/default/virtualpox)
if [ "$INSTALL_NO_GROUP" != "1" ]; then
  echo
  echo "Creating group 'vpoxusers'. VM users must be member of that group!"
  echo
  groupadd -r -f vpoxusers 2> /dev/null
fi

%if %{?rpm_mdv:1}%{!?rpm_mdv:0}
/sbin/ldconfig
%update_menus
%endif
update-mime-database /usr/share/mime &> /dev/null || :
update-desktop-database -q > /dev/null 2>&1 || :
touch --no-create /usr/share/icons/hicolor
gtk-update-icon-cache -q /usr/share/icons/hicolor 2> /dev/null || :

# Disable module compilation with INSTALL_NO_VPOXDRV=1 in /etc/default/virtualpox
if test "${INSTALL_NO_VPOXDRV}" = 1; then
  POSTINST_START=--nostart
else
  POSTINST_START=
fi
# Install and start the new service scripts.
/usr/lib/virtualpox/prerm-common.sh || true
/usr/lib/virtualpox/postinst-common.sh ${POSTINST_START} > /dev/null || true


%preun
# Called before the package is removed, or during upgrade after (not before)
# the new version's "post" scriptlet.
# $1==0: remove the last version of the package
# $1>=1: upgrade
if [ "$1" = 0 ]; then
  /usr/lib/virtualpox/prerm-common.sh || exit 1
  rm -f /etc/udev/rules.d/60-vpoxdrv.rules
  rm -f /etc/vpox/license_agreed
  rm -f /etc/vpox/module_not_compiled
fi

%postun
%if %{?rpm_mdv:1}%{!?rpm_mdv:0}
/sbin/ldconfig
%{clean_desktop_database}
%clean_menus
%endif
update-mime-database /usr/share/mime &> /dev/null || :
update-desktop-database -q > /dev/null 2>&1 || :
touch --no-create /usr/share/icons/hicolor
gtk-update-icon-cache -q /usr/share/icons/hicolor 2> /dev/null || :
rm -rf /usr/lib/virtualpox/ExtensionPacks


%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root)
%doc %{VPOXDOCDIR}/*
%if %{?with_python:1}%{!?with_python:0}
%{vpox_python_sitelib}/*
%endif
/etc/vpox
/usr/bin/*
/usr/src/vpox*
/usr/lib/virtualpox
/usr/share/applications/*
/usr/share/icons/hicolor/*/apps/*
/usr/share/icons/hicolor/*/mimetypes/*
/usr/share/mime/packages/*
/usr/share/pixmaps/*
/usr/share/virtualpox
