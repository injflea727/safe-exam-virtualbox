#
# Makefile for the VirtualPox Linux Guest Drivers.
#

#
# Copyright (C) 2009-2020 Oracle Corporation
#
# This file is part of VirtualPox Open Source Edition (OSE), as
# available from http://www.virtualpox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualPox OSE distribution. VirtualPox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

ifneq ($(KERNELRELEASE),)

# Building from kBuild (make -C <kernel_directory> M=`pwd`)
# or inside a kernel source tree.

obj-m = vpoxguest/ vpoxsf/ vpoxvideo/

else # ! KERNELRELEASE

KBUILD_VERBOSE =
 ifeq ($(KBUILD_VERBOSE),)
VPOX_QUIET    := @
VPOX_QUIET_SH := @
 else
VPOX_QUIET    :=
VPOX_QUIET_SH := set -x;
 endif

all: vpoxguest vpoxsf vpoxvideo

vpoxguest:
	@echo "=== Building 'vpoxguest' module ==="
	+ $(VPOX_QUIET)$(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vpoxguest
	$(VPOX_QUIET_SH)if [ -f vpoxguest/vpoxguest.ko ]; then \
	    cp vpoxguest/vpoxguest.ko .; \
	 else \
	    cp vpoxguest/vpoxguest.o .; \
	 fi
	@echo

vpoxsf: vpoxguest
	+ $(VPOX_QUIET_SH)if [ -d vpoxsf ]; then \
	    if [ -f vpoxguest/Module.symvers ]; then \
	        cp vpoxguest/Module.symvers vpoxsf; \
	    fi; \
	    echo "=== Building 'vpoxsf' module ==="; \
	    $(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) KBUILD_EXTRA_SYMBOLS=$(abspath vpoxsf/Module.symvers) -C vpoxsf || exit 1; \
	    if [ -f vpoxsf/vpoxsf.ko ]; then \
	        cp vpoxsf/vpoxsf.ko .; \
	    else \
	        cp vpoxsf/vpoxsf.o .; \
	    fi; \
	    echo; \
	fi

vpoxvideo:
	+ $(VPOX_QUIET_SH)if [ -d vpoxvideo ]; then \
	    echo "=== Building 'vpoxvideo' module ==="; \
	    $(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vpoxvideo || exit 1; \
	    if [ -f vpoxvideo/vpoxvideo.ko ]; then \
	        cp vpoxvideo/vpoxvideo.ko .; \
	    elif [ -f vpoxvideo/vpoxvideo.o ]; then \
	        cp vpoxvideo/vpoxvideo.o .; \
	    fi; \
	    echo; \
	fi

install-vpoxguest:
	+ $(VPOX_QUIET)$(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vpoxguest install

install-vpoxsf:
	+ $(VPOX_QUIET_SH)if [ -d vpoxsf ]; then \
	    $(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vpoxsf install; \
	fi

install-vpoxvideo:
	+ $(VPOX_QUIET_SH)if [ -d vpoxvideo ]; then \
	    $(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vpoxvideo install; \
	fi

install: install-vpoxguest install-vpoxsf install-vpoxvideo

clean-vpoxguest:
	+ $(VPOX_QUIET)$(MAKE) -C vpoxguest clean
	rm -f vpoxguest.*o

clean-vpoxsf:
	+ $(VPOX_QUIET_SH)if [ -d vpoxsf ]; then \
	    $(MAKE) -C vpoxsf clean; \
	fi
	rm -f vpoxsf.*o

clean-vpoxvideo:
	+ $(VPOX_QUIET_SH)if [ -d vpoxvideo ]; then \
	    $(MAKE) -C vpoxvideo clean; \
	fi
	rm -f vpoxvideo.*o

clean: clean-vpoxguest clean-vpoxsf clean-vpoxvideo

check:
	$(VPOX_QUIET)$(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vpoxguest check

unload:
	$(VPOX_QUIET)/sbin/rmmod vpoxvideo || true
	$(VPOX_QUIET)/sbin/rmmod vpoxvfs || true
	$(VPOX_QUIET)/sbin/rmmod vpoxsf || true
	$(VPOX_QUIET)/sbin/rmmod vpoxguest || true

load: unload
	$(VPOX_QUIET)/sbin/insmod vpoxguest.ko
	$(VPOX_QUIET)if [ -f vpoxsf.ko ]; then /sbin/insmod vpoxsf.ko; fi
	$(VPOX_QUIET)if [ -f vpoxvideo.ko ]; then /sbin/insmod vpoxvideo.ko; fi

.PHONY: all install clean check unload load \
    vpoxguest vpoxsf vpoxvideo \
    install-vpoxguest install-vpoxsf install-vpoxvideo \
    clean-vpoxguest clean-vpoxsf clean-vpoxvideo

endif # ! KERNELRELEASE
