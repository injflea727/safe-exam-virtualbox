# $Id: Makefile $
## @file
# Makefile for the VirtualPox Linux Host PCI Driver.
#

#
# Copyright (C) 2011-2020 Oracle Corporation
#
# This file is part of VirtualPox Open Source Edition (OSE), as
# available from http://www.virtualpox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualPox OSE distribution. VirtualPox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#
# The contents of this file may alternatively be used under the terms
# of the Common Development and Distribution License Version 1.0
# (CDDL) only, as it comes in the "COPYING.CDDL" file of the
# VirtualPox OSE distribution, in which case the provisions of the
# CDDL are applicable instead of those of the GPL.
#
# You may elect to license modified versions of this file under the
# terms and conditions of either the GPL or the CDDL or both.
#


# Linux kbuild sets this to our source directory if we are called from there
obj ?= $(CURDIR)
include $(obj)/Makefile-header.gmk
VPOXPCI_DIR := $(VPOX_MODULE_SRC_DIR)

# Allow building directly from the subdirectory without assuming the toplevel
# makefile has done the copying. Not the default use case, but can be handy.
ifndef KBUILD_EXTRA_SYMBOLS
KBUILD_EXTRA_SYMBOLS=$(abspath $(VPOXPCI_DIR)/../vpoxdrv/Module.symvers)
endif

# override is required by the Debian guys
VPOXMOD_NAME = vpoxpci
VPOXMOD_OBJS = \
	linux/VPoxPci-linux.o \
	VPoxPci.o \
	SUPR0IdcClient.o \
	SUPR0IdcClientComponent.o \
	linux/SUPR0IdcClient-linux.o
ifeq ($(VPOX_KBUILD_TARGET_ARCH),x86)
VPOXMOD_OBJS += \
	math/gcc/divdi3.o \
	math/gcc/moddi3.o \
	math/gcc/qdivrem.o \
	math/gcc/udivdi3.o \
	math/gcc/udivmoddi4.o \
	math/gcc/divdi3.o \
	math/gcc/umoddi3.o
endif
VPOXMOD_INCL = \
	$(VPOXPCI_DIR) \
	$(VPOXPCI_DIR)include \
	$(VPOXPCI_DIR)r0drv/linux
VPOXMOD_DEFS = \
	RT_OS_LINUX \
	IN_RING0 \
	IN_RT_R0 \
	IN_SUP_R0 \
	VPOX \
	RT_WITH_VPOX \
	VPOX_WITH_HARDENING
VPOXMOD_CFLAGS = -include $(VPOXPCI_DIR)include/VPox/SUPDrvMangling.h -fno-pie

include $(obj)/Makefile-footer.gmk
