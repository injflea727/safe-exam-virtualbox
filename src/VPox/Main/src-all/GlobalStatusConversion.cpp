/* $Id: GlobalStatusConversion.cpp $ */
/** @file
 * VirtualPox COM global definitions - status code conversion.
 *
 * NOTE: This file is part of both VPoxC.dll and VPoxSVC.exe.
 */

/*
 * Copyright (C) 2008-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "Global.h"

#include <iprt/assert.h>
#include <VPox/err.h>


/*static*/ int
Global::vpoxStatusCodeFromCOM(HRESULT aComStatus)
{
    switch (aComStatus)
    {
        case S_OK:                              return VINF_SUCCESS;

        /* Standard COM status codes. See also RTErrConvertFromDarwinCOM */
        case E_UNEXPECTED:                      return VERR_COM_UNEXPECTED;
        case E_NOTIMPL:                         return VERR_NOT_IMPLEMENTED;
        case E_OUTOFMEMORY:                     return VERR_NO_MEMORY;
        case E_INVALIDARG:                      return VERR_INVALID_PARAMETER;
        case E_NOINTERFACE:                     return VERR_NOT_SUPPORTED;
        case E_POINTER:                         return VERR_INVALID_POINTER;
#ifdef E_HANDLE
        case E_HANDLE:                          return VERR_INVALID_HANDLE;
#endif
        case E_ABORT:                           return VERR_CANCELLED;
        case E_FAIL:                            return VERR_GENERAL_FAILURE;
        case E_ACCESSDENIED:                    return VERR_ACCESS_DENIED;

        /* VirtualPox status codes */
        case VPOX_E_OBJECT_NOT_FOUND:           return VERR_COM_OBJECT_NOT_FOUND;
        case VPOX_E_INVALID_VM_STATE:           return VERR_COM_INVALID_VM_STATE;
        case VPOX_E_VM_ERROR:                   return VERR_COM_VM_ERROR;
        case VPOX_E_FILE_ERROR:                 return VERR_COM_FILE_ERROR;
        case VPOX_E_IPRT_ERROR:                 return VERR_COM_IPRT_ERROR;
        case VPOX_E_PDM_ERROR:                  return VERR_COM_PDM_ERROR;
        case VPOX_E_INVALID_OBJECT_STATE:       return VERR_COM_INVALID_OBJECT_STATE;
        case VPOX_E_HOST_ERROR:                 return VERR_COM_HOST_ERROR;
        case VPOX_E_NOT_SUPPORTED:              return VERR_COM_NOT_SUPPORTED;
        case VPOX_E_XML_ERROR:                  return VERR_COM_XML_ERROR;
        case VPOX_E_INVALID_SESSION_STATE:      return VERR_COM_INVALID_SESSION_STATE;
        case VPOX_E_OBJECT_IN_USE:              return VERR_COM_OBJECT_IN_USE;

        default:
            if (SUCCEEDED(aComStatus))
                return VINF_SUCCESS;
            /** @todo Check for the win32 facility and use the
             *        RTErrConvertFromWin32 function on windows. */
            return VERR_UNRESOLVED_ERROR;
    }
}


/*static*/ HRESULT
Global::vpoxStatusCodeToCOM(int aVPoxStatus)
{
    switch (aVPoxStatus)
    {
        case VINF_SUCCESS:                      return S_OK;

        /* Standard COM status codes. */
        case VERR_COM_UNEXPECTED:               return E_UNEXPECTED;
        case VERR_NOT_IMPLEMENTED:              return E_NOTIMPL;
        case VERR_NO_MEMORY:                    return E_OUTOFMEMORY;
        case VERR_INVALID_PARAMETER:            return E_INVALIDARG;
        case VERR_NOT_SUPPORTED:                return E_NOINTERFACE;
        case VERR_INVALID_POINTER:              return E_POINTER;
#ifdef E_HANDLE
        case VERR_INVALID_HANDLE:               return E_HANDLE;
#endif
        case VERR_CANCELLED:                    return E_ABORT;
        case VERR_GENERAL_FAILURE:              return E_FAIL;
        case VERR_ACCESS_DENIED:                return E_ACCESSDENIED;

        /* VirtualPox COM status codes. */
        case VERR_COM_OBJECT_NOT_FOUND:         return VPOX_E_OBJECT_NOT_FOUND;
        case VERR_COM_INVALID_VM_STATE:         return VPOX_E_INVALID_VM_STATE;
        case VERR_COM_VM_ERROR:                 return VPOX_E_VM_ERROR;
        case VERR_COM_FILE_ERROR:               return VPOX_E_FILE_ERROR;
        case VERR_COM_IPRT_ERROR:               return VPOX_E_IPRT_ERROR;
        case VERR_COM_PDM_ERROR:                return VPOX_E_PDM_ERROR;
        case VERR_COM_INVALID_OBJECT_STATE:     return VPOX_E_INVALID_OBJECT_STATE;
        case VERR_COM_HOST_ERROR:               return VPOX_E_HOST_ERROR;
        case VERR_COM_NOT_SUPPORTED:            return VPOX_E_NOT_SUPPORTED;
        case VERR_COM_XML_ERROR:                return VPOX_E_XML_ERROR;
        case VERR_COM_INVALID_SESSION_STATE:    return VPOX_E_INVALID_SESSION_STATE;
        case VERR_COM_OBJECT_IN_USE:            return VPOX_E_OBJECT_IN_USE;

        /* Other errors. */
        case VERR_UNRESOLVED_ERROR:             return E_FAIL;
        case VERR_NOT_EQUAL:                    return VPOX_E_FILE_ERROR;
        case VERR_FILE_NOT_FOUND:               return VPOX_E_OBJECT_NOT_FOUND;

        /* Guest Control errors. */
        case VERR_GSTCTL_MAX_CID_OBJECTS_REACHED: return VPOX_E_MAXIMUM_REACHED;
        case VERR_GSTCTL_GUEST_ERROR:             return VPOX_E_GSTCTL_GUEST_ERROR;

        default:
            AssertMsgFailed(("%Rrc\n", aVPoxStatus));
            if (RT_SUCCESS(aVPoxStatus))
                return S_OK;

            /* try categorize it */
            if (   aVPoxStatus < 0
                && (   aVPoxStatus > -1000
                    || (aVPoxStatus < -22000 && aVPoxStatus > -32766) )
               )
                return VPOX_E_IPRT_ERROR;
            if (    aVPoxStatus <  VERR_PDM_NO_SUCH_LUN / 100 * 10
                &&  aVPoxStatus >  VERR_PDM_NO_SUCH_LUN / 100 * 10 - 100)
                return VPOX_E_PDM_ERROR;
            if (    aVPoxStatus <= -1000
                &&  aVPoxStatus >  -5000 /* wrong, but so what... */)
                return VPOX_E_VM_ERROR;

            return E_FAIL;
    }
}

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
