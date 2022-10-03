#
# Makefile for the VirtualPox Linux Host Drivers.
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

ifneq ($(KERNELRELEASE),)

# Building from kBuild (make -C <kernel_directory> M=`pwd`),
# or inside a kernel source tree.

obj-m = vpoxdrv/
ifneq ($(wildcard $(CURDIR)/vpoxnetflt/Makefile),)
 obj-m += vpoxnetflt/
endif
ifneq ($(wildcard $(CURDIR)/vpoxnetadp/Makefile),)
 obj-m += vpoxnetadp/
endif
ifneq ($(wildcard $(CURDIR)/vpoxpci/Makefile),)
 obj-m += vpoxpci/
endif

else # ! KERNELRELEASE

# convenience Makefile without KERNELRELEASE

KBUILD_VERBOSE ?=
KERN_VER ?= $(shell uname -r)
.PHONY: all install clean check unload load \
    vpoxdrv vpoxnetflt vpoxnetadp vpoxpci \
    install-vpoxdrv install-vpoxnetflt install-vpoxnetadp install-vpoxpci \
    clean-vpoxdrv clean-vpoxnetflt clean-vpoxnetadp clean-vpoxpci

all: vpoxdrv vpoxnetflt vpoxnetadp vpoxpci

# We want to build on Linux 2.6.18 and later kernels.
ifneq ($(filter-out 1.% 2.0.% 2.1.% 2.2.% 2.3.% 2.4.% 2.5.%,$(KERN_VER)),)

vpoxdrv:
	@echo "=== Building 'vpoxdrv' module ==="
	+@$(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vpoxdrv
	@cp vpoxdrv/vpoxdrv.ko .
	@echo

vpoxnetflt: vpoxdrv
	+@if [ -d vpoxnetflt ]; then \
	    if [ -f vpoxdrv/Module.symvers ]; then \
	        cp vpoxdrv/Module.symvers vpoxnetflt; \
	    fi; \
	    echo "=== Building 'vpoxnetflt' module ==="; \
	    $(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) KBUILD_EXTRA_SYMBOLS=$(abspath vpoxnetflt/Module.symvers) -C vpoxnetflt || exit 1; \
	    cp vpoxnetflt/vpoxnetflt.ko .; \
	    echo; \
	fi

vpoxnetadp: vpoxdrv
	+@if [ -d vpoxnetadp ]; then \
	    if [ -f vpoxdrv/Module.symvers ]; then \
	        cp vpoxdrv/Module.symvers vpoxnetadp; \
	    fi; \
	    echo "=== Building 'vpoxnetadp' module ==="; \
	    $(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) KBUILD_EXTRA_SYMBOLS=$(abspath vpoxnetadp/Module.symvers) -C vpoxnetadp || exit 1; \
	    cp vpoxnetadp/vpoxnetadp.ko .; \
	    echo; \
	fi

vpoxpci: vpoxdrv
	+@if [ -d vpoxpci ]; then \
	    if [ -f vpoxdrv/Module.symvers ]; then \
	        cp vpoxdrv/Module.symvers vpoxpci; \
	    fi; \
	    echo "=== Building 'vpoxpci' module ==="; \
	    $(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) KBUILD_EXTRA_SYMBOLS=$(abspath vpoxpci/Module.symvers) -C vpoxpci || exit 1; \
	    cp vpoxpci/vpoxpci.ko .; \
	    echo; \
	fi

install-vpoxdrv:
	+@$(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vpoxdrv install

install-vpoxnetflt:
	+@if [ -d vpoxnetflt ]; then \
	    $(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vpoxnetflt install; \
	fi

install-vpoxnetadp:
	+@if [ -d vpoxnetadp ]; then \
	    $(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vpoxnetadp install; \
	fi

install-vpoxpci:
	+@if [ -d vpoxpci ]; then \
	    $(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vpoxpci install; \
	fi

install: install-vpoxdrv install-vpoxnetflt install-vpoxnetadp install-vpoxpci

else

vpoxdrv:
vpoxnetflt:
vpoxnetadp:
vpoxpci:
install:

endif

clean-vpoxdrv:
	+@$(MAKE) -C vpoxdrv clean
	rm -rf vpoxdrv.ko

clean-vpoxnetflt:
	+@if [ -d vpoxnetflt ]; then \
	    $(MAKE) -C vpoxnetflt clean; \
	fi
	rm -rf vpoxnetflt.ko

clean-vpoxnetadp:
	+@if [ -d vpoxnetadp ]; then \
	    $(MAKE) -C vpoxnetadp clean; \
	fi
	rm -rf vpoxnetadp.ko

clean-vpoxpci:
	+@if [ -d vpoxpci ]; then \
	    $(MAKE) -C vpoxpci clean; \
	fi
	rm -f vpoxpci.ko

clean: clean-vpoxdrv clean-vpoxnetflt clean-vpoxnetadp clean-vpoxpci

check:
	+@$(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) -C vpoxdrv check

unload:
	@for module in vpoxpci vpoxnetadp vpoxnetflt vpoxdrv; do \
		if grep "^$$module " /proc/modules >/dev/null; then \
			echo "Removing previously installed $$module module"; \
			/sbin/rmmod $$module; \
		fi; \
	done

load: unload
	@for module in vpoxdrv vpoxnetflt vpoxnetadp vpoxpci; do \
		if test -f $$module.ko; then \
			echo "Installing $$module module"; \
			/sbin/insmod $$module.ko; \
		fi; \
	done

endif # ! KERNELRELEASE
