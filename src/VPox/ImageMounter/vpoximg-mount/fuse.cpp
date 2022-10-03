/* $Id: fuse.cpp $ */
/** @file
 *
 * Module to dynamically load libfuse and load all symbols
 * which are needed by vpoximg-mount.
 */

/*
 * Copyright (C) 2019-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define LOG_GROUP RTLOGGROUP_DEFAULT
#include "fuse.h"

/* Declarations of the functions that we need from libfuse */
#define VPOX_FUSE_GENERATE_BODY
#include "fuse-calls.h"
