## @file
# Makefile for a sample/testcase using the 'glue' Java API bindings

#
# Copyright (C) 2010-2020 Oracle Corporation
#
# This file is part of VirtualPox Open Source Edition (OSE), as
# available from http://www.virtualpox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualPox OSE distribution. VirtualPox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

#
# User serviceable parts: adjust the following lines appropriately
#

# The host OS: linux, solaris, win, darwin, freebsd
HOSTOS=linux
# Absolute (!) path to the VirtualPox install directory
VPOX_BIN = /opt/VirtualPox
# Path to the sdk directory of the unpacked VirtualPox SDK
VPOX_SDK = ../../..
# On Windows, if you want to use the COM API: install directory of Jacob lib
JACOB_DIR =
# Extra classpath entries needed for compiling/running the sample
CLASSPATH =
# Java compiler to use
JAVAC = javac
# Java VM to use
JAVA = java


#
# No changes should be necessary after this point.
#

ifeq ($(HOSTOS),win)
 JACOB_JAR=$(JACOB_DIR)/jacob.jar
 VPOX_JAR=$(VPOX_SDK)/bindings/mscom/java/vpoxjmscom.jar
 SEP=\;
 JAVA_COM_ARGS += -Djava.library.path=$(JACOB_DIR)
 CLASSPATH += $(JACOB_JAR)$(SEP)
else
 VPOX_JAR=$(VPOX_SDK)/bindings/xpcom/java/vpoxjxpcom.jar
 SEP=:
 JAVA_COM_ARGS += -Dvpox.home=$(VPOX_BIN)
endif

VPOX_JAR_WS=$(VPOX_SDK)/bindings/webservice/java/jax-ws/vpoxjws.jar


all: ws/TestVPox.class com/TestVPox.class

test: ws/test-TestVPox com/test-TestVPox

com/TestVPox.class:
	@mkdir com 2>/dev/null || true
	$(JAVAC) -d com -cp $(VPOX_JAR)$(SEP)$(CLASSPATH) TestVPox.java

com/test-TestVPox: com/TestVPox.class
	$(JAVA) $(JAVA_COM_ARGS) -cp com$(SEP)$(VPOX_JAR)$(SEP)$(CLASSPATH) TestVPox

ws/TestVPox.class:
	@mkdir ws 2>/dev/null || true
	$(JAVAC) -d ws -cp $(VPOX_JAR_WS)$(SEP)$(CLASSPATH) TestVPox.java

ws/test-TestVPox: ws/TestVPox.class
	$(JAVA) $(JAVA_WS_ARGS) -cp ws$(SEP)$(VPOX_JAR_WS)$(SEP)$(CLASSPATH) TestVPox -w -url http://localhost:18083/
