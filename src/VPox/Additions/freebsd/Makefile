#
# Makefile for the VirtualPox FreeBSD Guest Drivers.
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

KBUILD_VERBOSE =

all:
	@echo "=== Building 'vpoxguest' module ==="
	@$(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vpoxguest
	@if [ -f vpoxguest/vpoxguest.ko ]; then \
	    cp vpoxguest/vpoxguest.ko .; \
	 fi
	@echo
	@if [ -d vpoxvfs ]; then \
	    echo "=== Building 'vpoxvfs' module ==="; \
	    $(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vpoxvfs; \
	    if [ -f vpoxvfs/vpoxvfs.ko ]; then \
	        cp vpoxvfs/vpoxvfs.ko .; \
	    fi; \
	fi


install:
	@$(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vpoxguest install
	@if [ -d vpoxvfs ]; then \
	    $(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vpoxvfs install; \
	fi

clean:
	@$(MAKE) -C vpoxguest clean
	@if [ -d vpoxvfs ]; then \
	    $(MAKE) -C vpoxvfs clean; \
	fi
	rm -f vpoxguest.*o vpoxvfs.*o

load:
	@/sbin/kldunload vpoxvfs || true
	@/sbin/kldunload vpoxguest || true
	@/sbin/kldload ./vpoxguest.ko
	@if [ -f vpoxvfs.ko ]; then /sbin/kldload ./vpoxvfs.ko; fi
