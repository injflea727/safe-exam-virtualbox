# -*- coding: utf-8 -*-
# $Id: vpoxcon.py $

"""
VirtualPox Constants.

See VPoxConstantWrappingHack for details.
"""

__copyright__ = \
"""
Copyright (C) 2010-2020 Oracle Corporation

This file is part of VirtualPox Open Source Edition (OSE), as
available from http://www.virtualpox.org. This file is free software;
you can redistribute it and/or modify it under the terms of the GNU
General Public License (GPL) as published by the Free Software
Foundation, in version 2 as it comes in the "COPYING" file of the
VirtualPox OSE distribution. VirtualPox OSE is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.

The contents of this file may alternatively be used under the terms
of the Common Development and Distribution License Version 1.0
(CDDL) only, as it comes in the "COPYING.CDDL" file of the
VirtualPox OSE distribution, in which case the provisions of the
CDDL are applicable instead of those of the GPL.

You may elect to license modified versions of this file under the
terms and conditions of either the GPL or the CDDL or both.
"""
__version__ = "$Revision: 135976 $"


# Standard Python imports.
import sys


class VPoxConstantWrappingHack(object):                                         # pylint: disable=too-few-public-methods
    """
    This is a hack to avoid the self.oVPoxMgr.constants.MachineState_Running
    ugliness that forces one into the right margin...  Anyone using this module
    can get to the constants easily by:

        from testdriver import vpoxcon
        if self.o.machine.state == vpoxcon.MachineState_Running:
            do stuff;

    For our own convenience there's a vpoxcon attribute set up in vpox.py,
    class TestDriver which is the basis for the VirtualPox testcases. It takes
    care of setting things up properly through the global variable
    'goHackModuleClass' that refers to the instance of this class(if we didn't
    we'd have to use testdriver.vpoxcon.MachineState_Running).
    """
    def __init__(self, oWrapped):
        self.oWrapped = oWrapped;
        self.oVPoxMgr = None;
        self.fpApiVer = 99.0;

    def __getattr__(self, sName):
        # Our self.
        try:
            return getattr(self.oWrapped, sName)
        except AttributeError:
            # The VPox constants.
            if self.oVPoxMgr is None:
                raise;
            try:
                return getattr(self.oVPoxMgr.constants, sName);
            except AttributeError:
                # Do some compatability mappings to keep it working with
                # older versions.
                if self.fpApiVer < 3.3:
                    if sName == 'SessionState_Locked':
                        return getattr(self.oVPoxMgr.constants, 'SessionState_Open');
                    if sName == 'SessionState_Unlocked':
                        return getattr(self.oVPoxMgr.constants, 'SessionState_Closed');
                    if sName == 'SessionState_Unlocking':
                        return getattr(self.oVPoxMgr.constants, 'SessionState_Closing');
                raise;


goHackModuleClass = VPoxConstantWrappingHack(sys.modules[__name__]);                         # pylint: disable=invalid-name
sys.modules[__name__] = goHackModuleClass;

