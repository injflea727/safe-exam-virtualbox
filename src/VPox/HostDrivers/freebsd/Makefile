#
# Makefile for the VirtualPox FreeBSD Host Drivers.
#

#
# Copyright (C) 2008-2020 Oracle Corporation
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

KBUILD_VERBOSE =

all:
	@echo "=== Building 'vpoxdrv' module ==="
	@$(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vpoxdrv
	@cp vpoxdrv/vpoxdrv.ko .
	@echo
	@if [ -d vpoxnetflt ]; then \
	    if [ -f vpoxdrv/Module.symvers ]; then \
	        cp vpoxdrv/Module.symvers vpoxnetflt; \
	    fi; \
	    echo "=== Building 'vpoxnetflt' module ==="; \
	    $(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vpoxnetflt; \
	    cp vpoxnetflt/vpoxnetflt.ko .; \
	    echo; \
	fi
	@if [ -d vpoxnetadp ]; then \
	    if [ -f vpoxdrv/Module.symvers ]; then \
	        cp vpoxdrv/Module.symvers vpoxnetadp; \
	    fi; \
	    echo "=== Building 'vpoxnetadp' module ==="; \
	    $(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vpoxnetadp; \
	    cp vpoxnetadp/vpoxnetadp.ko .; \
	    echo; \
	fi


install:
	@$(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vpoxdrv install
	@if [ -d vpoxnetflt ]; then \
	    $(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vpoxnetflt install; \
	fi
	@if [ -d vpoxnetadp ]; then \
	    $(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vpoxnetadp install; \
	fi

clean:
	@$(MAKE) -C vpoxdrv clean
	@if [ -d vpoxnetflt ]; then \
	    $(MAKE) -C vpoxnetflt clean; \
	fi
	@if [ -d vpoxnetadp ]; then \
	    $(MAKE) -C vpoxnetadp clean; \
	fi
	rm -f vpoxdrv.ko vpoxnetflt.ko vpoxnetadp.ko

load:
	@for module in vpoxnetadp vpoxnetflt vpoxdrv; do \
		if kldstat -n "$$module" >/dev/null; then \
			echo "Removing previously installed $$module module"; \
			/sbin/kldunload $$module; \
		fi; \
	done
	@for module in vpoxdrv vpoxnetflt vpoxnetadp; do \
		if test -f $$module.ko; then \
			echo "Installing $$module module"; \
			/sbin/kldload ./$$module.ko; \
		fi; \
	done
