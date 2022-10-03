/* $Id: VPoxMouse.cpp $ */
/** @file
 * VPoxMouse; input_server add-on - Haiku Guest Additions, implementation.
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
 *                    François Revol <revol@free.fr>
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <Clipboard.h>
#include <Debug.h>
#include <Message.h>
#include <String.h>

#include "VPoxMouse.h"
#include <VPox/VPoxGuest.h> /** @todo use the VbglR3 interface! */
#include <VPox/VPoxGuestLib.h>
#include <VPoxGuestInternal.h>
#include <VPox/VMMDev.h>
#include <VPox/log.h>
#include <iprt/errcore.h>

/* Export as global symbol with C linkage, RTDECL is necessary. */
RTDECL(BInputServerDevice *)
instantiate_input_device()
{
    return new VPoxMouse();
}


static inline int vpoxMouseAcquire()
{
    uint32_t fFeatures = 0;
    int rc = VbglR3GetMouseStatus(&fFeatures, NULL, NULL);
    if (RT_SUCCESS(rc))
    {
        rc = VbglR3SetMouseStatus(fFeatures | VMMDEV_MOUSE_GUEST_CAN_ABSOLUTE | VMMDEV_MOUSE_NEW_PROTOCOL);
        if (RT_FAILURE(rc))
            LogRel(("VbglR3SetMouseStatus failed. rc=%d\n", rc));
    }
    else
        LogRel(("VbglR3GetMouseStatus failed. rc=%d\n", rc));
    return rc;
}


static inline int vpoxMouseRelease()
{
    uint32_t fFeatures = 0;
    int rc = VbglR3GetMouseStatus(&fFeatures, NULL, NULL);
    if (RT_SUCCESS(rc))
    {
        rc = VbglR3SetMouseStatus(fFeatures & ~VMMDEV_MOUSE_GUEST_CAN_ABSOLUTE & ~VMMDEV_MOUSE_NEW_PROTOCOL);
        if (RT_FAILURE(rc))
            LogRel(("VbglR3SetMouseStatus failed. rc=%d\n", rc));
    }
    else
        LogRel(("VbglR3GetMouseStatus failed. rc=%d\n", rc));
    return rc;
}


VPoxMouse::VPoxMouse()
     : BInputServerDevice(),
       fDriverFD(-1),
       fServiceThreadID(-1),
       fExiting(false)
{
}


VPoxMouse::~VPoxMouse()
{
}


status_t VPoxMouse::InitCheck()
{
    int rc = VbglR3Init();
    if (!RT_SUCCESS(rc))
        return ENXIO;

    input_device_ref device = { (char *)"VPoxMouse", B_POINTING_DEVICE, (void *)this };
    input_device_ref *deviceList[2] = { &device, NULL };
    RegisterDevices(deviceList);

    return B_OK;
}


status_t VPoxMouse::SystemShuttingDown()
{
    VbglR3Term();

    return B_OK;
}


status_t VPoxMouse::Start(const char *device, void *cookie)
{
#if 0
    status_t err;
    int rc;
    uint32_t fFeatures = 0;
    Log(("VPoxMouse::%s()\n", __FUNCTION__));

    rc = VbglR3GetMouseStatus(&fFeatures, NULL, NULL);
    if (RT_SUCCESS(rc))
        rc = VbglR3SetMouseStatus(fFeatures
                                  | VMMDEV_MOUSE_GUEST_CAN_ABSOLUTE
                                  | VMMDEV_MOUSE_NEW_PROTOCOL);
    if (!RT_SUCCESS(rc))
    {
        LogRel(("VPoxMouse: Error switching guest mouse into absolute mode: %d\n", rc));
        return B_DEVICE_NOT_FOUND;
    }

    err = fServiceThreadID = spawn_thread(_ServiceThreadNub,
                                          "VPoxMouse", B_NORMAL_PRIORITY, this);
    if (err >= B_OK)
    {
        resume_thread(fServiceThreadID);
        return B_OK;
    }
    else
        LogRel(("VPoxMouse: Error starting service thread: 0x%08lx\n",
                err));

    // release the mouse
    rc = VbglR3GetMouseStatus(&fFeatures, NULL, NULL);
    if (RT_SUCCESS(rc))
        rc = VbglR3SetMouseStatus(fFeatures
                                  & ~VMMDEV_MOUSE_GUEST_CAN_ABSOLUTE
                                  & ~VMMDEV_MOUSE_NEW_PROTOCOL);

    return B_ERROR;
#endif

    status_t err = B_OK;
    int rc;
    uint32_t fFeatures = 0;
    LogFlowFunc(("device=%s cookie=%p\n", device, cookie));

    rc = vpoxMouseAcquire();
    if (RT_SUCCESS(rc))
    {
        err = fServiceThreadID = spawn_thread(_ServiceThreadNub, "VPoxMouse", B_NORMAL_PRIORITY, this);
        if (err >= B_OK)
        {
            resume_thread(fServiceThreadID);
            return B_OK;
        }
        else
            LogRel(("VPoxMouse::Start Error starting service thread: 0x%08lx\n", err));

        vpoxMouseRelease();
        err = B_ERROR;
    }
    else
    {
        LogRel(("VPoxMouse::Start vpoxMouseAcquire failed. rc=%d\n", rc));
        err = B_DEVICE_NOT_FOUND;
    }

    return err;
}


status_t VPoxMouse::Stop(const char *device, void *cookie)
{
    status_t status;
    int rc;
    uint32_t fFeatures = 0;
    Log(("VPoxMouse::%s()\n", __FUNCTION__));

    fExiting = true;

    vpoxMouseRelease();

    close(fDriverFD);
    fDriverFD = -1;
    //XXX WTF ?
    suspend_thread(fServiceThreadID);
    resume_thread(fServiceThreadID);
    wait_for_thread(fServiceThreadID, &status);
    fServiceThreadID = -1;
    fExiting = false;
    return B_OK;
}


status_t VPoxMouse::Control(const char *device, void *cookie, uint32 code, BMessage *message)
{
    switch (code)
    {
        case B_MOUSE_SPEED_CHANGED:
        case B_CLICK_SPEED_CHANGED:
        case B_MOUSE_ACCELERATION_CHANGED:
        default:
            return BInputServerDevice::Control(device, cookie, code, message);
    }
    return B_OK;
}


status_t VPoxMouse::_ServiceThreadNub(void *_this)
{
    VPoxMouse *service = (VPoxMouse *)_this;
    return service->_ServiceThread();
}


status_t VPoxMouse::_ServiceThread()
{
    Log(("VPoxMouse::%s()\n", __FUNCTION__));

    fDriverFD = open(VPOXGUEST_DEVICE_NAME, O_RDWR);
    if (fDriverFD < 0)
        return ENXIO;

    /* The thread waits for incoming messages from the host. */
    while (!fExiting)
    {
        uint32_t cx, cy, fFeatures;
        int rc;

        fd_set readSet, writeSet, errorSet;
        FD_ZERO(&readSet);
        FD_ZERO(&writeSet);
        FD_ZERO(&errorSet);
        FD_SET(fDriverFD, &readSet);
        if (fDriverFD < 0)
            break;
        rc = select(fDriverFD + 1, &readSet, &writeSet, &errorSet, NULL);
        if (rc < 0)
        {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            break;
        }

        rc = VbglR3GetMouseStatus(&fFeatures, &cx, &cy);
        if (   RT_SUCCESS(rc)
            && (fFeatures & VMMDEV_MOUSE_HOST_WANTS_ABSOLUTE))
        {
            float x = cx * 1.0 / 65535;
            float y = cy * 1.0 / 65535;

            _debugPrintf("VPoxMouse: at %d,%d %f,%f\n", cx, cy, x, y);

            /* Send absolute movement */
            bigtime_t now = system_time();
            BMessage *event = new BMessage(B_MOUSE_MOVED);
            event->AddInt64("when", now);
            event->AddFloat("x", x);
            event->AddFloat("y", y);
            event->AddFloat("be:tablet_x", x);
            event->AddFloat("be:tablet_y", y);
            //event->PrintToStream();
            EnqueueMessage(event);

            //LogRelFlow(("processed host event rc = %d\n", rc));
        }
    }
    return 0;
}

