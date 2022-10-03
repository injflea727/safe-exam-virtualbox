/* $Id: VPoxGuestR0LibIdc-solaris.cpp $ */
/** @file
 * VPoxGuestLib - Ring-0 Support Library for VPoxGuest, IDC, Solaris specific.
 */

/*
 * Copyright (C) 2008-2020 Oracle Corporation
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
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <sys/conf.h>
#include <sys/sunldi.h>
#include <sys/file.h>
#undef u /* /usr/include/sys/user.h:249:1 is where this is defined to (curproc->p_user). very cool. */
#include "VPoxGuestR0LibInternal.h"
#include <VPox/err.h>


int VPOXCALL vbglR0IdcNativeOpen(PVBGLIDCHANDLE pHandle, PVBGLIOCIDCCONNECT pReq)
{
    ldi_handle_t hDev   = NULL;
    ldi_ident_t  hIdent = ldi_ident_from_anon();
    int rc = ldi_open_by_name((char *)VPOXGUEST_DEVICE_NAME, FREAD, kcred, &hDev, hIdent);
    ldi_ident_release(hIdent);
    if (rc == 0)
    {
        pHandle->s.hDev = hDev;
        rc = VbglR0IdcCallRaw(pHandle, VBGL_IOCTL_IDC_CONNECT, &pReq->Hdr, sizeof(*pReq));
        if (RT_SUCCESS(rc) && RT_SUCCESS(pReq->Hdr.rc))
            return VINF_SUCCESS;
        ldi_close(hDev, FREAD, kcred);
    }
    else
        rc = VERR_OPEN_FAILED;
    pHandle->s.hDev = NULL;
    return rc;
}


int VPOXCALL vbglR0IdcNativeClose(PVBGLIDCHANDLE pHandle, PVBGLIOCIDCDISCONNECT pReq)
{
    int rc = VbglR0IdcCallRaw(pHandle, VBGL_IOCTL_IDC_DISCONNECT, &pReq->Hdr, sizeof(*pReq));
    if (RT_SUCCESS(rc) && RT_SUCCESS(pReq->Hdr.rc))
    {
        ldi_close(pHandle->s.hDev, FREAD, kcred);
        pHandle->s.hDev = NULL;
    }
    return rc;
}


/**
 * Makes an IDC call, returning only the I/O control status code.
 *
 * @returns VPox status code (the I/O control failure status).
 * @param   pHandle             The IDC handle.
 * @param   uReq                The request number.
 * @param   pReqHdr             The request header.
 * @param   cbReq               The request size.
 */
DECLR0VBGL(int) VbglR0IdcCallRaw(PVBGLIDCHANDLE pHandle, uintptr_t uReq, PVBGLREQHDR pReqHdr, uint32_t cbReq)
{
#if 0
    return VPoxGuestIDC(pHandle->s.pvSession, uReq, pReqHdr, cbReq);
#else
    int iIgn;
    int rc = ldi_ioctl(pHandle->s.hDev, uReq, (intptr_t)pReqHdr, FKIOCTL | FNATIVE, kcred, &iIgn);
    if (rc == 0)
        return VINF_SUCCESS;
    return RTErrConvertFromErrno(rc);
#endif
}

