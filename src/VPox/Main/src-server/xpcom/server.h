/* $Id: server.h $ */
/** @file
 *
 * Common header for XPCOM server and its module counterpart
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef MAIN_INCLUDED_SRC_src_server_xpcom_server_h
#define MAIN_INCLUDED_SRC_src_server_xpcom_server_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VPox/com/com.h>

#include <VPox/version.h>

/**
 * IPC name used to resolve the client ID of the server.
 */
#define VPOXSVC_IPC_NAME "VPoxSVC-" VPOX_VERSION_STRING


/**
 * Tag for the file descriptor passing for the daemonizing control.
 */
#define VPOXSVC_STARTUP_PIPE_NAME "vpoxsvc:startup-pipe"

#endif /* !MAIN_INCLUDED_SRC_src_server_xpcom_server_h */
