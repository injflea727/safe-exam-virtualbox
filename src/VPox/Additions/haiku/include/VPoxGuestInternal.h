/* $$ */
/** @file
 * VPoxGuestInternal - Private Haiku additions declarations.
 */

/*
 * Copyright (C) 2012-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*
 * This code is based on:
 *
 * VirtualPox Guest Additions for Haiku.
 * Copyright (c) 2011 Mike Smith <mike@scgtrp.net>
 *                    Fran�ois Revol <revol@free.fr>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef GA_INCLUDED_HAIKU_VPoxGuestInternal_h
#define GA_INCLUDED_HAIKU_VPoxGuestInternal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/** The MIME signature of the VPoxGuest application. */
#define VPOX_GUEST_APP_SIG                       "application/x-vnd.Oracle-VPoxGuest"

/** The code used for messages sent by and to the system tray. */
#define VPOX_GUEST_CLIPBOARD_HOST_MSG_READ_DATA  'vbC2'
#define VPOX_GUEST_CLIPBOARD_HOST_MSG_FORMATS    'vbC3'

/** The code used for messages sent by and to the system tray. */
#define VPOX_GUEST_APP_ACTION                    'vpox'

#endif /* !GA_INCLUDED_HAIKU_VPoxGuestInternal_h */

