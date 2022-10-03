/* $Id: VPoxMouseLog.h $ */
/** @file
 * VPox Mouse drivers, logging helper
 */

/*
 * Copyright (C) 2011-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_WINNT_Mouse_common_VPoxMouseLog_h
#define GA_INCLUDED_SRC_WINNT_Mouse_common_VPoxMouseLog_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VPox/log.h>
#include <iprt/assert.h>

#define VPOX_MOUSE_LOG_NAME "VPoxMouse"

/* Uncomment to show file/line info in the log */
/*#define VPOX_MOUSE_LOG_SHOWLINEINFO*/

#define VPOX_MOUSE_LOG_PREFIX_FMT VPOX_MOUSE_LOG_NAME"::"LOG_FN_FMT": "
#define VPOX_MOUSE_LOG_PREFIX_PARMS __PRETTY_FUNCTION__

#ifdef VPOX_MOUSE_LOG_SHOWLINEINFO
# define VPOX_MOUSE_LOG_SUFFIX_FMT " (%s:%d)\n"
# define VPOX_MOUSE_LOG_SUFFIX_PARMS ,__FILE__, __LINE__
#else
# define VPOX_MOUSE_LOG_SUFFIX_FMT "\n"
# define VPOX_MOUSE_LOG_SUFFIX_PARMS
#endif

#define _LOGMSG(_logger, _a)                                                \
    do                                                                      \
    {                                                                       \
        _logger((VPOX_MOUSE_LOG_PREFIX_FMT, VPOX_MOUSE_LOG_PREFIX_PARMS));  \
        _logger(_a);                                                        \
        _logger((VPOX_MOUSE_LOG_SUFFIX_FMT  VPOX_MOUSE_LOG_SUFFIX_PARMS));  \
    } while (0)

#if 1 /* Exclude yourself if you're not keen on this. */
# define BREAK_WARN() AssertFailed()
#else
# define BREAK_WARN() do {} while(0)
#endif

#define WARN(_a)                                                                  \
    do                                                                            \
    {                                                                             \
        Log((VPOX_MOUSE_LOG_PREFIX_FMT"WARNING! ", VPOX_MOUSE_LOG_PREFIX_PARMS)); \
        Log(_a);                                                                  \
        Log((VPOX_MOUSE_LOG_SUFFIX_FMT VPOX_MOUSE_LOG_SUFFIX_PARMS));             \
        BREAK_WARN(); \
    } while (0)

#define LOG(_a) _LOGMSG(Log, _a)
#define LOGREL(_a) _LOGMSG(LogRel, _a)
#define LOGF(_a) _LOGMSG(LogFlow, _a)
#define LOGF_ENTER() LOGF(("ENTER"))
#define LOGF_LEAVE() LOGF(("LEAVE"))

#endif /* !GA_INCLUDED_SRC_WINNT_Mouse_common_VPoxMouseLog_h */

