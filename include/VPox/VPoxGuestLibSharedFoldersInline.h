/* $Id: VPoxGuestLibSharedFoldersInline.h $ */
/** @file
 * VPoxGuestLib - Shared Folders Host Request Helpers (ring-0).
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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

#ifndef VPOX_INCLUDED_VPoxGuestLibSharedFoldersInline_h
#define VPOX_INCLUDED_VPoxGuestLibSharedFoldersInline_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/types.h>
#include <iprt/assert.h>
#include <VPox/VPoxGuest.h>
#include <VPox/VPoxGuestLib.h>
#include <VPox/VPoxGuestLibSharedFolders.h>
#include <VPox/VMMDev.h>
#include <VPox/shflsvc.h>
#include <iprt/err.h>


/** @defgroup grp_vpoxguest_lib_r0_sf_inline    Shared Folders Host Request Helpers
 * @ingroup grp_vpoxguest_lib_r0
 *
 * @note Using inline functions to avoid wasting precious ring-0 stack space on
 *       passing parameters that ends up in the structure @a pReq points to.  It
 *       is also safe to assume that it's faster too.  It's worth a few bytes
 *       larger code section in the resulting shared folders driver.
 *
 * @note This currently requires a C++ compiler or a C compiler capable of
 *       mixing code and variables (i.e. C99).
 *
 * @{
 */

/** VMMDEV_HVF_XXX (set during init). */
extern uint32_t g_fHostFeatures;
extern VBGLSFCLIENT g_SfClient; /**< Move this into the parameters? */

/** Request structure for VbglR0SfHostReqQueryFeatures. */
typedef struct VPOXSFQUERYFEATURES
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VPoxSFParmQueryFeatures Parms;
} VPOXSFQUERYFEATURES;

/**
 * SHFL_FN_QUERY_FEATURES request.
 */
DECLINLINE(int) VbglR0SfHostReqQueryFeatures(VPOXSFQUERYFEATURES *pReq)
{
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_QUERY_FEATURES, SHFL_CPARMS_QUERY_FEATURES, sizeof(*pReq));

    pReq->Parms.f64Features.type          = VMMDevHGCMParmType_64bit;
    pReq->Parms.f64Features.u.value64     = 0;

    pReq->Parms.u32LastFunction.type      = VMMDevHGCMParmType_32bit;
    pReq->Parms.u32LastFunction.u.value32 = 0;

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, sizeof(*pReq));
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;

    /*
     * Provide fallback values based on g_fHostFeatures to simplify
     * compatibility with older hosts and avoid duplicating this logic.
     */
    if (RT_FAILURE(vrc))
    {
        pReq->Parms.f64Features.u.value64     = 0;
        pReq->Parms.u32LastFunction.u.value32 = g_fHostFeatures & VMMDEV_HVF_HGCM_NO_BOUNCE_PAGE_LIST
                                              ?  SHFL_FN_SET_FILE_SIZE : SHFL_FN_SET_SYMLINKS;
        if (vrc == VERR_NOT_SUPPORTED)
            vrc = VINF_NOT_SUPPORTED;
    }
    return vrc;
}

/**
 * SHFL_FN_QUERY_FEATURES request, simplified version.
 */
DECLINLINE(int) VbglR0SfHostReqQueryFeaturesSimple(uint64_t *pfFeatures, uint32_t *puLastFunction)
{
    VPOXSFQUERYFEATURES *pReq = (VPOXSFQUERYFEATURES *)VbglR0PhysHeapAlloc(sizeof(*pReq));
    if (pReq)
    {
        int rc = VbglR0SfHostReqQueryFeatures(pReq);
        if (pfFeatures)
            *pfFeatures = pReq->Parms.f64Features.u.value64;
        if (puLastFunction)
            *puLastFunction = pReq->Parms.u32LastFunction.u.value32;

        VbglR0PhysHeapFree(pReq);
        return rc;
    }
    return VERR_NO_MEMORY;
}


/** Request structure for VbglR0SfHostReqSetUtf8 and VbglR0SfHostReqSetSymlink. */
typedef struct VPOXSFNOPARMS
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    /* no parameters */
} VPOXSFNOPARMS;

/**
 * Worker for request without any parameters.
 */
DECLINLINE(int) VbglR0SfHostReqNoParms(VPOXSFNOPARMS *pReq, uint32_t uFunction, uint32_t cParms)
{
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                uFunction, cParms, sizeof(*pReq));
    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, sizeof(*pReq));
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}

/**
 * Worker for request without any parameters, simplified.
 */
DECLINLINE(int) VbglR0SfHostReqNoParmsSimple(uint32_t uFunction, uint32_t cParms)
{
    VPOXSFNOPARMS *pReq = (VPOXSFNOPARMS *)VbglR0PhysHeapAlloc(sizeof(*pReq));
    if (pReq)
    {
        int vrc = VbglR0SfHostReqNoParms(pReq, uFunction, cParms);
        VbglR0PhysHeapFree(pReq);
        return vrc;
    }
    return VERR_NO_MEMORY;
}


/**
 * SHFL_F_SET_UTF8 request.
 */
DECLINLINE(int) VbglR0SfHostReqSetUtf8(VPOXSFNOPARMS *pReq)
{
    return VbglR0SfHostReqNoParms(pReq, SHFL_FN_SET_UTF8, SHFL_CPARMS_SET_UTF8);
}

/**
 * SHFL_F_SET_UTF8 request, simplified version.
 */
DECLINLINE(int) VbglR0SfHostReqSetUtf8Simple(void)
{
    return VbglR0SfHostReqNoParmsSimple(SHFL_FN_SET_UTF8, SHFL_CPARMS_SET_UTF8);
}


/**
 * SHFL_F_SET_SYMLINKS request.
 */
DECLINLINE(int) VbglR0SfHostReqSetSymlinks(VPOXSFNOPARMS *pReq)
{
    return VbglR0SfHostReqNoParms(pReq, SHFL_FN_SET_SYMLINKS, SHFL_CPARMS_SET_SYMLINKS);
}

/**
 * SHFL_F_SET_SYMLINKS request, simplified version.
 */
DECLINLINE(int) VbglR0SfHostReqSetSymlinksSimple(void)
{
    return VbglR0SfHostReqNoParmsSimple(SHFL_FN_SET_SYMLINKS, SHFL_CPARMS_SET_SYMLINKS);
}


/** Request structure for VbglR0SfHostReqSetErrorStyle.  */
typedef struct VPOXSFSETERRORSTYLE
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VPoxSFParmSetErrorStyle Parms;
} VPOXSFSETERRORSTYLE;

/**
 * SHFL_FN_QUERY_FEATURES request.
 */
DECLINLINE(int) VbglR0SfHostReqSetErrorStyle(VPOXSFSETERRORSTYLE *pReq, SHFLERRORSTYLE enmStyle)
{
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_SET_ERROR_STYLE, SHFL_CPARMS_SET_ERROR_STYLE, sizeof(*pReq));

    pReq->Parms.u32Style.type           = VMMDevHGCMParmType_32bit;
    pReq->Parms.u32Style.u.value32      = (uint32_t)enmStyle;

    pReq->Parms.u32Reserved.type        = VMMDevHGCMParmType_32bit;
    pReq->Parms.u32Reserved.u.value32   = 0;

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, sizeof(*pReq));
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}

/**
 * SHFL_FN_QUERY_FEATURES request, simplified version.
 */
DECLINLINE(int) VbglR0SfHostReqSetErrorStyleSimple(SHFLERRORSTYLE enmStyle)
{
    VPOXSFSETERRORSTYLE *pReq = (VPOXSFSETERRORSTYLE *)VbglR0PhysHeapAlloc(sizeof(*pReq));
    if (pReq)
    {
        int rc = VbglR0SfHostReqSetErrorStyle(pReq, enmStyle);
        VbglR0PhysHeapFree(pReq);
        return rc;
    }
    return VERR_NO_MEMORY;
}


/** Request structure for VbglR0SfHostReqMapFolderWithBuf.  */
typedef struct VPOXSFMAPFOLDERWITHBUFREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VPoxSFParmMapFolder     Parms;
    HGCMPageListInfo        PgLst;
} VPOXSFMAPFOLDERWITHBUFREQ;


/**
 * SHFL_FN_MAP_FOLDER request.
 */
DECLINLINE(int) VbglR0SfHostReqMapFolderWithContig(VPOXSFMAPFOLDERWITHBUFREQ *pReq, PSHFLSTRING pStrName, RTGCPHYS64 PhysStrName,
                                                   RTUTF16 wcDelimiter, bool fCaseSensitive)
{
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_MAP_FOLDER, SHFL_CPARMS_MAP_FOLDER, sizeof(*pReq));

    pReq->Parms.id32Root.type                   = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32              = SHFL_ROOT_NIL;

    pReq->Parms.uc32Delimiter.type              = VMMDevHGCMParmType_32bit;
    pReq->Parms.uc32Delimiter.u.value32         = wcDelimiter;

    pReq->Parms.fCaseSensitive.type             = VMMDevHGCMParmType_32bit;
    pReq->Parms.fCaseSensitive.u.value32        = fCaseSensitive;

    if (g_fHostFeatures & VMMDEV_HVF_HGCM_CONTIGUOUS_PAGE_LIST)
    {
        pReq->Parms.pStrName.type               = VMMDevHGCMParmType_PageList;
        pReq->Parms.pStrName.u.PageList.size    = SHFLSTRING_HEADER_SIZE + pStrName->u16Size;
        pReq->Parms.pStrName.u.PageList.offset  = RT_UOFFSETOF(VPOXSFMAPFOLDERWITHBUFREQ, PgLst) - sizeof(VBGLIOCIDCHGCMFASTCALL);
        pReq->PgLst.flags                       = VPOX_HGCM_F_PARM_DIRECTION_BOTH;
        pReq->PgLst.offFirstPage                = (uint16_t)PhysStrName & (uint16_t)(PAGE_OFFSET_MASK);
        pReq->PgLst.aPages[0]                   = PhysStrName & ~(RTGCPHYS64)PAGE_OFFSET_MASK;
        pReq->PgLst.cPages                      = 1;
    }
    else
    {
        pReq->Parms.pStrName.type               = VMMDevHGCMParmType_LinAddr_In;
        pReq->Parms.pStrName.u.LinAddr.cb       = SHFLSTRING_HEADER_SIZE + pStrName->u16Size;
        pReq->Parms.pStrName.u.LinAddr.uAddr    = (uintptr_t)pStrName;
    }

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, sizeof(*pReq));
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}

/**
 * SHFL_FN_MAP_FOLDER request.
 */
DECLINLINE(int) VbglR0SfHostReqMapFolderWithContigSimple(PSHFLSTRING pStrName, RTGCPHYS64 PhysStrName,
                                                         RTUTF16 wcDelimiter, bool fCaseSensitive, SHFLROOT *pidRoot)
{
    VPOXSFMAPFOLDERWITHBUFREQ *pReq = (VPOXSFMAPFOLDERWITHBUFREQ *)VbglR0PhysHeapAlloc(sizeof(*pReq));
    if (pReq)
    {
        int rc = VbglR0SfHostReqMapFolderWithContig(pReq, pStrName, PhysStrName, wcDelimiter, fCaseSensitive);
        *pidRoot = RT_SUCCESS(rc) ? pReq->Parms.id32Root.u.value32 : SHFL_ROOT_NIL;
        VbglR0PhysHeapFree(pReq);
        return rc;
    }
    *pidRoot = SHFL_ROOT_NIL;
    return VERR_NO_MEMORY;
}


/**
 * SHFL_FN_MAP_FOLDER request.
 */
DECLINLINE(int) VbglR0SfHostReqMapFolderWithBuf(VPOXSFMAPFOLDERWITHBUFREQ *pReq, PSHFLSTRING pStrName,
                                                RTUTF16 wcDelimiter, bool fCaseSensitive)
{
    return VbglR0SfHostReqMapFolderWithContig(pReq, pStrName, VbglR0PhysHeapGetPhysAddr(pStrName), wcDelimiter, fCaseSensitive);
}



/** Request structure used by vpoxSfOs2HostReqUnmapFolder.  */
typedef struct VPOXSFUNMAPFOLDERREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VPoxSFParmUnmapFolder   Parms;
} VPOXSFUNMAPFOLDERREQ;


/**
 * SHFL_FN_UNMAP_FOLDER request.
 */
DECLINLINE(int) VbglR0SfHostReqUnmapFolderSimple(uint32_t idRoot)
{
    VPOXSFUNMAPFOLDERREQ *pReq = (VPOXSFUNMAPFOLDERREQ *)VbglR0PhysHeapAlloc(sizeof(*pReq));
    if (pReq)
    {
        pReq->Parms.id32Root.type      = VMMDevHGCMParmType_32bit;
        pReq->Parms.id32Root.u.value32 = idRoot;

        VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                    SHFL_FN_UNMAP_FOLDER, SHFL_CPARMS_UNMAP_FOLDER, sizeof(*pReq));

        int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, sizeof(*pReq));
        if (RT_SUCCESS(vrc))
            vrc = pReq->Call.header.result;

        VbglR0PhysHeapFree(pReq);
        return vrc;
    }
    return VERR_NO_MEMORY;
}


/** Request structure for VbglR0SfHostReqCreate.  */
typedef struct VPOXSFCREATEREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VPoxSFParmCreate        Parms;
    SHFLCREATEPARMS         CreateParms;
    SHFLSTRING              StrPath;
} VPOXSFCREATEREQ;

/**
 * SHFL_FN_CREATE request.
 */
DECLINLINE(int) VbglR0SfHostReqCreate(SHFLROOT idRoot, VPOXSFCREATEREQ *pReq)
{
    uint32_t const cbReq = g_fHostFeatures & VMMDEV_HVF_HGCM_EMBEDDED_BUFFERS
                         ? RT_UOFFSETOF(VPOXSFCREATEREQ, StrPath.String) + pReq->StrPath.u16Size
                         : RT_UOFFSETOF(VPOXSFCREATEREQ, CreateParms);
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_CREATE, SHFL_CPARMS_CREATE, cbReq);

    pReq->Parms.id32Root.type                       = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32                  = idRoot;

    if (g_fHostFeatures & VMMDEV_HVF_HGCM_EMBEDDED_BUFFERS)
    {
        pReq->Parms.pStrPath.type                   = VMMDevHGCMParmType_Embedded;
        pReq->Parms.pStrPath.u.Embedded.cbData      = SHFLSTRING_HEADER_SIZE + pReq->StrPath.u16Size;
        pReq->Parms.pStrPath.u.Embedded.offData     = RT_UOFFSETOF(VPOXSFCREATEREQ, StrPath) - sizeof(VBGLIOCIDCHGCMFASTCALL);
        pReq->Parms.pStrPath.u.Embedded.fFlags      = VPOX_HGCM_F_PARM_DIRECTION_TO_HOST;

        pReq->Parms.pCreateParms.type               = VMMDevHGCMParmType_Embedded;
        pReq->Parms.pCreateParms.u.Embedded.cbData  = sizeof(pReq->CreateParms);
        pReq->Parms.pCreateParms.u.Embedded.offData = RT_UOFFSETOF(VPOXSFCREATEREQ, CreateParms) - sizeof(VBGLIOCIDCHGCMFASTCALL);
        pReq->Parms.pCreateParms.u.Embedded.fFlags  = VPOX_HGCM_F_PARM_DIRECTION_BOTH;
    }
    else
    {
        pReq->Parms.pStrPath.type                   = VMMDevHGCMParmType_LinAddr_In;
        pReq->Parms.pStrPath.u.LinAddr.cb           = SHFLSTRING_HEADER_SIZE + pReq->StrPath.u16Size;
        pReq->Parms.pStrPath.u.LinAddr.uAddr        = (uintptr_t)&pReq->StrPath;

        pReq->Parms.pCreateParms.type               = VMMDevHGCMParmType_LinAddr;
        pReq->Parms.pCreateParms.u.LinAddr.cb       = sizeof(pReq->CreateParms);
        pReq->Parms.pCreateParms.u.LinAddr.uAddr    = (uintptr_t)&pReq->CreateParms;
    }

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, cbReq);
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}


/** Request structure for VbglR0SfHostReqClose.  */
typedef struct VPOXSFCLOSEREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VPoxSFParmClose         Parms;
} VPOXSFCLOSEREQ;

/**
 * SHFL_FN_CLOSE request.
 */
DECLINLINE(int) VbglR0SfHostReqClose(SHFLROOT idRoot, VPOXSFCLOSEREQ *pReq, uint64_t hHostFile)
{
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_CLOSE, SHFL_CPARMS_CLOSE, sizeof(*pReq));

    pReq->Parms.id32Root.type       = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32  = idRoot;

    pReq->Parms.u64Handle.type      = VMMDevHGCMParmType_64bit;
    pReq->Parms.u64Handle.u.value64 = hHostFile;

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, sizeof(*pReq));
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}

/**
 * SHFL_FN_CLOSE request, allocate request buffer.
 */
DECLINLINE(int) VbglR0SfHostReqCloseSimple(SHFLROOT idRoot, uint64_t hHostFile)
{
    VPOXSFCLOSEREQ *pReq = (VPOXSFCLOSEREQ *)VbglR0PhysHeapAlloc(sizeof(*pReq));
    if (pReq)
    {
        int vrc = VbglR0SfHostReqClose(idRoot, pReq, hHostFile);
        VbglR0PhysHeapFree(pReq);
        return vrc;
    }
    return VERR_NO_MEMORY;
}


/** Request structure for VbglR0SfHostReqQueryVolInfo.  */
typedef struct VPOXSFVOLINFOREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VPoxSFParmInformation   Parms;
    SHFLVOLINFO             VolInfo;
} VPOXSFVOLINFOREQ;

/**
 * SHFL_FN_INFORMATION[SHFL_INFO_VOLUME | SHFL_INFO_GET] request.
 */
DECLINLINE(int) VbglR0SfHostReqQueryVolInfo(SHFLROOT idRoot, VPOXSFVOLINFOREQ *pReq, uint64_t hHostFile)
{
    uint32_t const cbReq = g_fHostFeatures & VMMDEV_HVF_HGCM_EMBEDDED_BUFFERS
                         ? sizeof(*pReq) : RT_UOFFSETOF(VPOXSFVOLINFOREQ, VolInfo);
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_INFORMATION, SHFL_CPARMS_INFORMATION, cbReq);

    pReq->Parms.id32Root.type                   = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32              = idRoot;

    pReq->Parms.u64Handle.type                  = VMMDevHGCMParmType_64bit;
    pReq->Parms.u64Handle.u.value64             = hHostFile;

    pReq->Parms.f32Flags.type                   = VMMDevHGCMParmType_32bit;
    pReq->Parms.f32Flags.u.value32              = SHFL_INFO_VOLUME | SHFL_INFO_GET;

    pReq->Parms.cb32.type                       = VMMDevHGCMParmType_32bit;
    pReq->Parms.cb32.u.value32                  = sizeof(pReq->VolInfo);

    if (g_fHostFeatures & VMMDEV_HVF_HGCM_EMBEDDED_BUFFERS)
    {
        pReq->Parms.pInfo.type                  = VMMDevHGCMParmType_Embedded;
        pReq->Parms.pInfo.u.Embedded.cbData     = sizeof(pReq->VolInfo);
        pReq->Parms.pInfo.u.Embedded.offData    = RT_UOFFSETOF(VPOXSFVOLINFOREQ, VolInfo) - sizeof(VBGLIOCIDCHGCMFASTCALL);
        pReq->Parms.pInfo.u.Embedded.fFlags     = VPOX_HGCM_F_PARM_DIRECTION_FROM_HOST;
    }
    else
    {
        pReq->Parms.pInfo.type                  = VMMDevHGCMParmType_LinAddr_Out;
        pReq->Parms.pInfo.u.LinAddr.cb          = sizeof(pReq->VolInfo);
        pReq->Parms.pInfo.u.LinAddr.uAddr       = (uintptr_t)&pReq->VolInfo;
    }

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, cbReq);
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}


/** Request structure for VbglR0SfHostReqSetObjInfo & VbglR0SfHostReqQueryObjInfo. */
typedef struct VPOXSFOBJINFOREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VPoxSFParmInformation   Parms;
    SHFLFSOBJINFO           ObjInfo;
} VPOXSFOBJINFOREQ;

/**
 * SHFL_FN_INFORMATION[SHFL_INFO_GET | SHFL_INFO_FILE] request.
 */
DECLINLINE(int) VbglR0SfHostReqQueryObjInfo(SHFLROOT idRoot, VPOXSFOBJINFOREQ *pReq, uint64_t hHostFile)
{
    uint32_t const cbReq = g_fHostFeatures & VMMDEV_HVF_HGCM_EMBEDDED_BUFFERS
                         ? sizeof(*pReq) : RT_UOFFSETOF(VPOXSFOBJINFOREQ, ObjInfo);
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_INFORMATION, SHFL_CPARMS_INFORMATION, cbReq);

    pReq->Parms.id32Root.type                   = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32              = idRoot;

    pReq->Parms.u64Handle.type                  = VMMDevHGCMParmType_64bit;
    pReq->Parms.u64Handle.u.value64             = hHostFile;

    pReq->Parms.f32Flags.type                   = VMMDevHGCMParmType_32bit;
    pReq->Parms.f32Flags.u.value32              = SHFL_INFO_GET | SHFL_INFO_FILE;

    pReq->Parms.cb32.type                       = VMMDevHGCMParmType_32bit;
    pReq->Parms.cb32.u.value32                  = sizeof(pReq->ObjInfo);

    if (g_fHostFeatures & VMMDEV_HVF_HGCM_EMBEDDED_BUFFERS)
    {
        pReq->Parms.pInfo.type                  = VMMDevHGCMParmType_Embedded;
        pReq->Parms.pInfo.u.Embedded.cbData     = sizeof(pReq->ObjInfo);
        pReq->Parms.pInfo.u.Embedded.offData    = RT_UOFFSETOF(VPOXSFOBJINFOREQ, ObjInfo) - sizeof(VBGLIOCIDCHGCMFASTCALL);
        pReq->Parms.pInfo.u.Embedded.fFlags     = VPOX_HGCM_F_PARM_DIRECTION_FROM_HOST;
    }
    else
    {
        pReq->Parms.pInfo.type                  = VMMDevHGCMParmType_LinAddr_Out;
        pReq->Parms.pInfo.u.LinAddr.cb          = sizeof(pReq->ObjInfo);
        pReq->Parms.pInfo.u.LinAddr.uAddr       = (uintptr_t)&pReq->ObjInfo;
    }

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, cbReq);
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}


/**
 * SHFL_FN_INFORMATION[SHFL_INFO_SET | SHFL_INFO_FILE] request.
 */
DECLINLINE(int) VbglR0SfHostReqSetObjInfo(SHFLROOT idRoot, VPOXSFOBJINFOREQ *pReq, uint64_t hHostFile)
{
    uint32_t const cbReq = g_fHostFeatures & VMMDEV_HVF_HGCM_EMBEDDED_BUFFERS
                         ? sizeof(*pReq) : RT_UOFFSETOF(VPOXSFOBJINFOREQ, ObjInfo);
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_INFORMATION, SHFL_CPARMS_INFORMATION, cbReq);

    pReq->Parms.id32Root.type                   = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32              = idRoot;

    pReq->Parms.u64Handle.type                  = VMMDevHGCMParmType_64bit;
    pReq->Parms.u64Handle.u.value64             = hHostFile;

    pReq->Parms.f32Flags.type                   = VMMDevHGCMParmType_32bit;
    pReq->Parms.f32Flags.u.value32              = SHFL_INFO_SET | SHFL_INFO_FILE;

    pReq->Parms.cb32.type                       = VMMDevHGCMParmType_32bit;
    pReq->Parms.cb32.u.value32                  = sizeof(pReq->ObjInfo);

    if (g_fHostFeatures & VMMDEV_HVF_HGCM_EMBEDDED_BUFFERS)
    {
        pReq->Parms.pInfo.type                  = VMMDevHGCMParmType_Embedded;
        pReq->Parms.pInfo.u.Embedded.cbData     = sizeof(pReq->ObjInfo);
        pReq->Parms.pInfo.u.Embedded.offData    = RT_UOFFSETOF(VPOXSFOBJINFOREQ, ObjInfo) - sizeof(VBGLIOCIDCHGCMFASTCALL);
        pReq->Parms.pInfo.u.Embedded.fFlags     = VPOX_HGCM_F_PARM_DIRECTION_BOTH;
    }
    else
    {
        pReq->Parms.pInfo.type                  = VMMDevHGCMParmType_LinAddr;
        pReq->Parms.pInfo.u.LinAddr.cb          = sizeof(pReq->ObjInfo);
        pReq->Parms.pInfo.u.LinAddr.uAddr       = (uintptr_t)&pReq->ObjInfo;
    }

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, cbReq);
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}


/**
 * SHFL_FN_INFORMATION[SHFL_INFO_SET | SHFL_INFO_SIZE] request.
 */
DECLINLINE(int) VbglR0SfHostReqSetFileSizeOld(SHFLROOT idRoot, VPOXSFOBJINFOREQ *pReq, uint64_t hHostFile)
{
    uint32_t const cbReq = g_fHostFeatures & VMMDEV_HVF_HGCM_EMBEDDED_BUFFERS
                         ? sizeof(*pReq) : RT_UOFFSETOF(VPOXSFOBJINFOREQ, ObjInfo);
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_INFORMATION, SHFL_CPARMS_INFORMATION, cbReq);

    pReq->Parms.id32Root.type                   = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32              = idRoot;

    pReq->Parms.u64Handle.type                  = VMMDevHGCMParmType_64bit;
    pReq->Parms.u64Handle.u.value64             = hHostFile;

    pReq->Parms.f32Flags.type                   = VMMDevHGCMParmType_32bit;
    pReq->Parms.f32Flags.u.value32              = SHFL_INFO_SET | SHFL_INFO_SIZE;

    pReq->Parms.cb32.type                       = VMMDevHGCMParmType_32bit;
    pReq->Parms.cb32.u.value32                  = sizeof(pReq->ObjInfo);

    if (g_fHostFeatures & VMMDEV_HVF_HGCM_EMBEDDED_BUFFERS)
    {
        pReq->Parms.pInfo.type                  = VMMDevHGCMParmType_Embedded;
        pReq->Parms.pInfo.u.Embedded.cbData     = sizeof(pReq->ObjInfo);
        pReq->Parms.pInfo.u.Embedded.offData    = RT_UOFFSETOF(VPOXSFOBJINFOREQ, ObjInfo) - sizeof(VBGLIOCIDCHGCMFASTCALL);
        pReq->Parms.pInfo.u.Embedded.fFlags     = VPOX_HGCM_F_PARM_DIRECTION_BOTH;
    }
    else
    {
        pReq->Parms.pInfo.type                  = VMMDevHGCMParmType_LinAddr;
        pReq->Parms.pInfo.u.LinAddr.cb          = sizeof(pReq->ObjInfo);
        pReq->Parms.pInfo.u.LinAddr.uAddr       = (uintptr_t)&pReq->ObjInfo;
    }

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, cbReq);
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}


/** Request structure for VbglR0SfHostReqSetObjInfo.  */
typedef struct VPOXSFOBJINFOWITHBUFREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VPoxSFParmInformation   Parms;
    HGCMPageListInfo        PgLst;
} VPOXSFOBJINFOWITHBUFREQ;

/**
 * SHFL_FN_INFORMATION[SHFL_INFO_SET | SHFL_INFO_FILE] request, with separate
 * buffer (on the physical heap).
 */
DECLINLINE(int) VbglR0SfHostReqSetObjInfoWithBuf(SHFLROOT idRoot, VPOXSFOBJINFOWITHBUFREQ *pReq, uint64_t hHostFile,
                                                 PSHFLFSOBJINFO pObjInfo, uint32_t offObjInfoInAlloc)
{
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_INFORMATION, SHFL_CPARMS_INFORMATION, sizeof(*pReq));

    pReq->Parms.id32Root.type               = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32          = idRoot;

    pReq->Parms.u64Handle.type              = VMMDevHGCMParmType_64bit;
    pReq->Parms.u64Handle.u.value64         = hHostFile;

    pReq->Parms.f32Flags.type               = VMMDevHGCMParmType_32bit;
    pReq->Parms.f32Flags.u.value32          = SHFL_INFO_SET | SHFL_INFO_FILE;

    pReq->Parms.cb32.type                   = VMMDevHGCMParmType_32bit;
    pReq->Parms.cb32.u.value32              = sizeof(*pObjInfo);

    if (g_fHostFeatures & VMMDEV_HVF_HGCM_CONTIGUOUS_PAGE_LIST)
    {
        pReq->Parms.pInfo.type              = VMMDevHGCMParmType_ContiguousPageList;
        pReq->Parms.pInfo.u.PageList.size   = sizeof(*pObjInfo);
        pReq->Parms.pInfo.u.PageList.offset = RT_UOFFSETOF(VPOXSFOBJINFOREQ, ObjInfo) - sizeof(VBGLIOCIDCHGCMFASTCALL);
        pReq->PgLst.flags                   = VPOX_HGCM_F_PARM_DIRECTION_BOTH;
        pReq->PgLst.aPages[0]               = VbglR0PhysHeapGetPhysAddr((uint8_t *)pObjInfo - offObjInfoInAlloc) + offObjInfoInAlloc;
        pReq->PgLst.offFirstPage            = (uint16_t)(pReq->PgLst.aPages[0] & PAGE_OFFSET_MASK);
        pReq->PgLst.aPages[0]              &= ~(RTGCPHYS)PAGE_OFFSET_MASK;
        pReq->PgLst.cPages                  = 1;
    }
    else
    {
        pReq->Parms.pInfo.type              = VMMDevHGCMParmType_LinAddr;
        pReq->Parms.pInfo.u.LinAddr.cb      = sizeof(*pObjInfo);
        pReq->Parms.pInfo.u.LinAddr.uAddr   = (uintptr_t)pObjInfo;
    }

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, sizeof(*pReq));
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}


/** Request structure for VbglR0SfHostReqRemove.  */
typedef struct VPOXSFREMOVEREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VPoxSFParmRemove        Parms;
    SHFLSTRING              StrPath;
} VPOXSFREMOVEREQ;

/**
 * SHFL_FN_REMOVE request.
 */
DECLINLINE(int) VbglR0SfHostReqRemove(SHFLROOT idRoot, VPOXSFREMOVEREQ *pReq, uint32_t fFlags)
{
    uint32_t const cbReq = RT_UOFFSETOF(VPOXSFREMOVEREQ, StrPath.String)
                         + (g_fHostFeatures & VMMDEV_HVF_HGCM_EMBEDDED_BUFFERS ? pReq->StrPath.u16Size : 0);
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_REMOVE, SHFL_CPARMS_REMOVE, cbReq);

    pReq->Parms.id32Root.type                   = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32              = idRoot;

    if (g_fHostFeatures & VMMDEV_HVF_HGCM_EMBEDDED_BUFFERS)
    {
        pReq->Parms.pStrPath.type               = VMMDevHGCMParmType_Embedded;
        pReq->Parms.pStrPath.u.Embedded.cbData  = SHFLSTRING_HEADER_SIZE + pReq->StrPath.u16Size;
        pReq->Parms.pStrPath.u.Embedded.offData = RT_UOFFSETOF(VPOXSFREMOVEREQ, StrPath) - sizeof(VBGLIOCIDCHGCMFASTCALL);
        pReq->Parms.pStrPath.u.Embedded.fFlags  = VPOX_HGCM_F_PARM_DIRECTION_TO_HOST;
    }
    else
    {
        pReq->Parms.pStrPath.type               = VMMDevHGCMParmType_LinAddr_In;
        pReq->Parms.pStrPath.u.LinAddr.cb       = SHFLSTRING_HEADER_SIZE + pReq->StrPath.u16Size;
        pReq->Parms.pStrPath.u.LinAddr.uAddr    = (uintptr_t)&pReq->StrPath;
    }

    pReq->Parms.f32Flags.type                   = VMMDevHGCMParmType_32bit;
    pReq->Parms.f32Flags.u.value32              = fFlags;

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, cbReq);
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}


/** Request structure for VbglR0SfHostReqCloseAndRemove.  */
typedef struct VPOXSFCLOSEANDREMOVEREQ
{
    VBGLIOCIDCHGCMFASTCALL      Hdr;
    VMMDevHGCMCall              Call;
    VPoxSFParmCloseAndRemove    Parms;
    SHFLSTRING                  StrPath;
} VPOXSFCLOSEANDREMOVEREQ;

/**
 * SHFL_FN_CLOSE_AND_REMOVE request.
 */
DECLINLINE(int) VbglR0SfHostReqCloseAndRemove(SHFLROOT idRoot, VPOXSFCLOSEANDREMOVEREQ *pReq, uint32_t fFlags, SHFLHANDLE hToClose)
{
    uint32_t const cbReq = RT_UOFFSETOF(VPOXSFCLOSEANDREMOVEREQ, StrPath.String)
                         + (g_fHostFeatures & VMMDEV_HVF_HGCM_EMBEDDED_BUFFERS ? pReq->StrPath.u16Size : 0);
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_CLOSE_AND_REMOVE, SHFL_CPARMS_CLOSE_AND_REMOVE, cbReq);

    pReq->Parms.id32Root.type                   = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32              = idRoot;

    if (g_fHostFeatures & VMMDEV_HVF_HGCM_EMBEDDED_BUFFERS)
    {
        pReq->Parms.pStrPath.type               = VMMDevHGCMParmType_Embedded;
        pReq->Parms.pStrPath.u.Embedded.cbData  = SHFLSTRING_HEADER_SIZE + pReq->StrPath.u16Size;
        pReq->Parms.pStrPath.u.Embedded.offData = RT_UOFFSETOF(VPOXSFCLOSEANDREMOVEREQ, StrPath) - sizeof(VBGLIOCIDCHGCMFASTCALL);
        pReq->Parms.pStrPath.u.Embedded.fFlags  = VPOX_HGCM_F_PARM_DIRECTION_TO_HOST;
    }
    else
    {
        pReq->Parms.pStrPath.type               = VMMDevHGCMParmType_LinAddr_In;
        pReq->Parms.pStrPath.u.LinAddr.cb       = SHFLSTRING_HEADER_SIZE + pReq->StrPath.u16Size;
        pReq->Parms.pStrPath.u.LinAddr.uAddr    = (uintptr_t)&pReq->StrPath;
    }

    pReq->Parms.f32Flags.type                   = VMMDevHGCMParmType_32bit;
    pReq->Parms.f32Flags.u.value32              = fFlags;

    pReq->Parms.u64Handle.type                  = VMMDevHGCMParmType_64bit;
    pReq->Parms.u64Handle.u.value64             = hToClose;

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, cbReq);
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}


/** Request structure for VbglR0SfHostReqRenameWithSrcContig and
 *  VbglR0SfHostReqRenameWithSrcBuf. */
typedef struct VPOXSFRENAMEWITHSRCBUFREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VPoxSFParmRename        Parms;
    HGCMPageListInfo        PgLst;
    SHFLSTRING              StrDstPath;
} VPOXSFRENAMEWITHSRCBUFREQ;


/**
 * SHFL_FN_REMOVE request.
 */
DECLINLINE(int) VbglR0SfHostReqRenameWithSrcContig(SHFLROOT idRoot, VPOXSFRENAMEWITHSRCBUFREQ *pReq,
                                                   PSHFLSTRING pSrcStr, RTGCPHYS64 PhysSrcStr, uint32_t fFlags)
{
    uint32_t const cbReq = RT_UOFFSETOF(VPOXSFRENAMEWITHSRCBUFREQ, StrDstPath.String)
                         + (g_fHostFeatures & VMMDEV_HVF_HGCM_EMBEDDED_BUFFERS ? pReq->StrDstPath.u16Size : 0);
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_RENAME, SHFL_CPARMS_RENAME, cbReq);

    pReq->Parms.id32Root.type                       = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32                  = idRoot;

    if (g_fHostFeatures & VMMDEV_HVF_HGCM_CONTIGUOUS_PAGE_LIST)
    {
        pReq->Parms.pStrSrcPath.type                = VMMDevHGCMParmType_ContiguousPageList;
        pReq->Parms.pStrSrcPath.u.PageList.size     = SHFLSTRING_HEADER_SIZE + pSrcStr->u16Size;
        pReq->Parms.pStrSrcPath.u.PageList.offset   = RT_UOFFSETOF(VPOXSFRENAMEWITHSRCBUFREQ, PgLst)
                                                    - sizeof(VBGLIOCIDCHGCMFASTCALL);
        pReq->PgLst.flags                           = VPOX_HGCM_F_PARM_DIRECTION_TO_HOST;
        pReq->PgLst.offFirstPage                    = (uint16_t)PhysSrcStr & (uint16_t)(PAGE_OFFSET_MASK);
        pReq->PgLst.aPages[0]                       = PhysSrcStr & ~(RTGCPHYS64)PAGE_OFFSET_MASK;
        pReq->PgLst.cPages                          = 1;
    }
    else
    {
        pReq->Parms.pStrSrcPath.type                = VMMDevHGCMParmType_LinAddr_In;
        pReq->Parms.pStrSrcPath.u.LinAddr.cb        = SHFLSTRING_HEADER_SIZE + pSrcStr->u16Size;
        pReq->Parms.pStrSrcPath.u.LinAddr.uAddr     = (uintptr_t)pSrcStr;
    }

    if (g_fHostFeatures & VMMDEV_HVF_HGCM_EMBEDDED_BUFFERS)
    {
        pReq->Parms.pStrDstPath.type                = VMMDevHGCMParmType_Embedded;
        pReq->Parms.pStrDstPath.u.Embedded.cbData   = SHFLSTRING_HEADER_SIZE + pReq->StrDstPath.u16Size;
        pReq->Parms.pStrDstPath.u.Embedded.offData  = RT_UOFFSETOF(VPOXSFRENAMEWITHSRCBUFREQ, StrDstPath)
                                                    - sizeof(VBGLIOCIDCHGCMFASTCALL);
        pReq->Parms.pStrDstPath.u.Embedded.fFlags   = VPOX_HGCM_F_PARM_DIRECTION_TO_HOST;
    }
    else
    {
        pReq->Parms.pStrDstPath.type                = VMMDevHGCMParmType_LinAddr_In;
        pReq->Parms.pStrDstPath.u.LinAddr.cb        = SHFLSTRING_HEADER_SIZE + pReq->StrDstPath.u16Size;
        pReq->Parms.pStrDstPath.u.LinAddr.uAddr     = (uintptr_t)&pReq->StrDstPath;
    }

    pReq->Parms.f32Flags.type                       = VMMDevHGCMParmType_32bit;
    pReq->Parms.f32Flags.u.value32                  = fFlags;

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, cbReq);
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}


/**
 * SHFL_FN_REMOVE request.
 */
DECLINLINE(int) VbglR0SfHostReqRenameWithSrcBuf(SHFLROOT idRoot, VPOXSFRENAMEWITHSRCBUFREQ *pReq,
                                                PSHFLSTRING pSrcStr, uint32_t fFlags)
{
    return VbglR0SfHostReqRenameWithSrcContig(idRoot, pReq, pSrcStr, VbglR0PhysHeapGetPhysAddr(pSrcStr), fFlags);
}


/** Request structure for VbglR0SfHostReqFlush.  */
typedef struct VPOXSFFLUSHREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VPoxSFParmFlush         Parms;
} VPOXSFFLUSHREQ;

/**
 * SHFL_FN_FLUSH request.
 */
DECLINLINE(int) VbglR0SfHostReqFlush(SHFLROOT idRoot, VPOXSFFLUSHREQ *pReq, uint64_t hHostFile)
{
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_FLUSH, SHFL_CPARMS_FLUSH, sizeof(*pReq));

    pReq->Parms.id32Root.type       = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32  = idRoot;

    pReq->Parms.u64Handle.type      = VMMDevHGCMParmType_64bit;
    pReq->Parms.u64Handle.u.value64 = hHostFile;

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, sizeof(*pReq));
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}

/**
 * SHFL_FN_FLUSH request, allocate request buffer.
 */
DECLINLINE(int) VbglR0SfHostReqFlushSimple(SHFLROOT idRoot, uint64_t hHostFile)
{
    VPOXSFFLUSHREQ *pReq = (VPOXSFFLUSHREQ *)VbglR0PhysHeapAlloc(sizeof(*pReq));
    if (pReq)
    {
        int vrc = VbglR0SfHostReqFlush(idRoot, pReq, hHostFile);
        VbglR0PhysHeapFree(pReq);
        return vrc;
    }
    return VERR_NO_MEMORY;
}


/** Request structure for VbglR0SfHostReqSetFileSize.  */
typedef struct VPOXSFSETFILESIZEREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VPoxSFParmSetFileSize   Parms;
} VPOXSFSETFILESIZEREQ;

/**
 * SHFL_FN_SET_FILE_SIZE request.
 */
DECLINLINE(int) VbglR0SfHostReqSetFileSize(SHFLROOT idRoot, VPOXSFSETFILESIZEREQ *pReq, uint64_t hHostFile, uint64_t cbNewSize)
{
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_SET_FILE_SIZE, SHFL_CPARMS_SET_FILE_SIZE, sizeof(*pReq));

    pReq->Parms.id32Root.type           = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32      = idRoot;

    pReq->Parms.u64Handle.type          = VMMDevHGCMParmType_64bit;
    pReq->Parms.u64Handle.u.value64     = hHostFile;

    pReq->Parms.cb64NewSize.type        = VMMDevHGCMParmType_64bit;
    pReq->Parms.cb64NewSize.u.value64   = cbNewSize;

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, sizeof(*pReq));
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}

/**
 * SHFL_FN_SET_FILE_SIZE request, allocate request buffer.
 */
DECLINLINE(int) VbglR0SfHostReqSetFileSizeSimple(SHFLROOT idRoot, uint64_t hHostFile, uint64_t cbNewSize)
{
    VPOXSFSETFILESIZEREQ *pReq = (VPOXSFSETFILESIZEREQ *)VbglR0PhysHeapAlloc(sizeof(*pReq));
    if (pReq)
    {
        int vrc = VbglR0SfHostReqSetFileSize(idRoot, pReq, hHostFile, cbNewSize);
        VbglR0PhysHeapFree(pReq);
        return vrc;
    }
    return VERR_NO_MEMORY;
}


/** Request structure for VbglR0SfHostReqReadEmbedded. */
typedef struct VPOXSFREADEMBEDDEDREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VPoxSFParmRead          Parms;
    RT_FLEXIBLE_ARRAY_EXTENSION
    uint8_t                 abData[RT_FLEXIBLE_ARRAY];
} VPOXSFREADEMBEDDEDREQ;

/**
 * SHFL_FN_READ request using embedded data buffer.
 */
DECLINLINE(int) VbglR0SfHostReqReadEmbedded(SHFLROOT idRoot, VPOXSFREADEMBEDDEDREQ *pReq, uint64_t hHostFile,
                                            uint64_t offRead, uint32_t cbToRead)
{
    uint32_t const cbReq = RT_UOFFSETOF(VPOXSFREADEMBEDDEDREQ, abData[0])
                         + (g_fHostFeatures & VMMDEV_HVF_HGCM_EMBEDDED_BUFFERS ? cbToRead : 0);
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_READ, SHFL_CPARMS_READ, cbReq);

    pReq->Parms.id32Root.type               = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32          = idRoot;

    pReq->Parms.u64Handle.type              = VMMDevHGCMParmType_64bit;
    pReq->Parms.u64Handle.u.value64         = hHostFile;

    pReq->Parms.off64Read.type              = VMMDevHGCMParmType_64bit;
    pReq->Parms.off64Read.u.value64         = offRead;

    pReq->Parms.cb32Read.type               = VMMDevHGCMParmType_32bit;
    pReq->Parms.cb32Read.u.value32          = cbToRead;

    if (g_fHostFeatures & VMMDEV_HVF_HGCM_EMBEDDED_BUFFERS)
    {
        pReq->Parms.pBuf.type               = VMMDevHGCMParmType_Embedded;
        pReq->Parms.pBuf.u.Embedded.cbData  = cbToRead;
        pReq->Parms.pBuf.u.Embedded.offData = RT_UOFFSETOF(VPOXSFREADEMBEDDEDREQ, abData[0]) - sizeof(VBGLIOCIDCHGCMFASTCALL);
        pReq->Parms.pBuf.u.Embedded.fFlags  = VPOX_HGCM_F_PARM_DIRECTION_FROM_HOST;
    }
    else
    {
        pReq->Parms.pBuf.type               = VMMDevHGCMParmType_LinAddr_Out;
        pReq->Parms.pBuf.u.LinAddr.cb       = cbToRead;
        pReq->Parms.pBuf.u.LinAddr.uAddr    = (uintptr_t)&pReq->abData[0];
    }

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, cbReq);
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}


/** Request structure for vpoxSfOs2HostReqRead & VbglR0SfHostReqReadContig. */
typedef struct VPOXSFREADPGLSTREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VPoxSFParmRead          Parms;
    HGCMPageListInfo        PgLst;
} VPOXSFREADPGLSTREQ;

/**
 * SHFL_FN_READ request using page list for data buffer (caller populated).
 */
DECLINLINE(int) VbglR0SfHostReqReadPgLst(SHFLROOT idRoot, VPOXSFREADPGLSTREQ *pReq, uint64_t hHostFile,
                                         uint64_t offRead, uint32_t cbToRead, uint32_t cPages)
{
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_READ, SHFL_CPARMS_READ,
                                RT_UOFFSETOF_DYN(VPOXSFREADPGLSTREQ, PgLst.aPages[cPages]));

    pReq->Parms.id32Root.type               = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32          = idRoot;

    pReq->Parms.u64Handle.type              = VMMDevHGCMParmType_64bit;
    pReq->Parms.u64Handle.u.value64         = hHostFile;

    pReq->Parms.off64Read.type              = VMMDevHGCMParmType_64bit;
    pReq->Parms.off64Read.u.value64         = offRead;

    pReq->Parms.cb32Read.type               = VMMDevHGCMParmType_32bit;
    pReq->Parms.cb32Read.u.value32          = cbToRead;

    pReq->Parms.pBuf.type                   = g_fHostFeatures & VMMDEV_HVF_HGCM_NO_BOUNCE_PAGE_LIST
                                            ? VMMDevHGCMParmType_NoBouncePageList : VMMDevHGCMParmType_PageList;
    pReq->Parms.pBuf.u.PageList.size        = cbToRead;
    pReq->Parms.pBuf.u.PageList.offset      = RT_UOFFSETOF(VPOXSFREADPGLSTREQ, PgLst) - sizeof(VBGLIOCIDCHGCMFASTCALL);
    pReq->PgLst.flags                       = VPOX_HGCM_F_PARM_DIRECTION_FROM_HOST;
    pReq->PgLst.cPages                      = (uint16_t)cPages;
    AssertReturn(cPages <= UINT16_MAX, VERR_OUT_OF_RANGE);
    /* caller sets offset */

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr,
                                 RT_UOFFSETOF_DYN(VPOXSFREADPGLSTREQ, PgLst.aPages[cPages]));
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}


/**
 * SHFL_FN_READ request using a physically contiguous buffer.
 */
DECLINLINE(int) VbglR0SfHostReqReadContig(SHFLROOT idRoot, VPOXSFREADPGLSTREQ *pReq, uint64_t hHostFile,
                                          uint64_t offRead, uint32_t cbToRead, void *pvBuffer, RTGCPHYS64 PhysBuffer)
{
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_READ, SHFL_CPARMS_READ, RT_UOFFSETOF_DYN(VPOXSFREADPGLSTREQ, PgLst.aPages[1]));

    pReq->Parms.id32Root.type               = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32          = idRoot;

    pReq->Parms.u64Handle.type              = VMMDevHGCMParmType_64bit;
    pReq->Parms.u64Handle.u.value64         = hHostFile;

    pReq->Parms.off64Read.type              = VMMDevHGCMParmType_64bit;
    pReq->Parms.off64Read.u.value64         = offRead;

    pReq->Parms.cb32Read.type               = VMMDevHGCMParmType_32bit;
    pReq->Parms.cb32Read.u.value32          = cbToRead;

    if (g_fHostFeatures & VMMDEV_HVF_HGCM_CONTIGUOUS_PAGE_LIST)
    {
        pReq->Parms.pBuf.type               = VMMDevHGCMParmType_ContiguousPageList;
        pReq->Parms.pBuf.u.PageList.size    = cbToRead;
        pReq->Parms.pBuf.u.PageList.offset  = RT_UOFFSETOF(VPOXSFREADPGLSTREQ, PgLst) - sizeof(VBGLIOCIDCHGCMFASTCALL);
        pReq->PgLst.flags                   = VPOX_HGCM_F_PARM_DIRECTION_FROM_HOST;
        pReq->PgLst.offFirstPage            = (uint16_t)(PhysBuffer & PAGE_OFFSET_MASK);
        pReq->PgLst.cPages                  = 1;
        pReq->PgLst.aPages[0]               = PhysBuffer & ~(RTGCPHYS64)PAGE_OFFSET_MASK;
    }
    else
    {
        pReq->Parms.pBuf.type               = VMMDevHGCMParmType_LinAddr_Out;
        pReq->Parms.pBuf.u.LinAddr.cb       = cbToRead;
        pReq->Parms.pBuf.u.LinAddr.uAddr    = (uintptr_t)pvBuffer;
    }

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, RT_UOFFSETOF_DYN(VPOXSFREADPGLSTREQ, PgLst.aPages[1]));
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}



/** Request structure for VbglR0SfHostReqWriteEmbedded. */
typedef struct VPOXSFWRITEEMBEDDEDREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VPoxSFParmWrite         Parms;
    RT_FLEXIBLE_ARRAY_EXTENSION
    uint8_t                 abData[RT_FLEXIBLE_ARRAY];
} VPOXSFWRITEEMBEDDEDREQ;

/**
 * SHFL_FN_WRITE request using embedded data buffer.
 */
DECLINLINE(int) VbglR0SfHostReqWriteEmbedded(SHFLROOT idRoot, VPOXSFWRITEEMBEDDEDREQ *pReq, uint64_t hHostFile,
                                             uint64_t offWrite, uint32_t cbToWrite)
{
    uint32_t const cbReq = RT_UOFFSETOF(VPOXSFWRITEEMBEDDEDREQ, abData[0])
                         + (g_fHostFeatures & VMMDEV_HVF_HGCM_EMBEDDED_BUFFERS ? cbToWrite : 0);
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_WRITE, SHFL_CPARMS_WRITE, cbReq);

    pReq->Parms.id32Root.type               = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32          = idRoot;

    pReq->Parms.u64Handle.type              = VMMDevHGCMParmType_64bit;
    pReq->Parms.u64Handle.u.value64         = hHostFile;

    pReq->Parms.off64Write.type             = VMMDevHGCMParmType_64bit;
    pReq->Parms.off64Write.u.value64        = offWrite;

    pReq->Parms.cb32Write.type              = VMMDevHGCMParmType_32bit;
    pReq->Parms.cb32Write.u.value32         = cbToWrite;

    if (g_fHostFeatures & VMMDEV_HVF_HGCM_EMBEDDED_BUFFERS)
    {
        pReq->Parms.pBuf.type               = VMMDevHGCMParmType_Embedded;
        pReq->Parms.pBuf.u.Embedded.cbData  = cbToWrite;
        pReq->Parms.pBuf.u.Embedded.offData = RT_UOFFSETOF(VPOXSFWRITEEMBEDDEDREQ, abData[0]) - sizeof(VBGLIOCIDCHGCMFASTCALL);
        pReq->Parms.pBuf.u.Embedded.fFlags  = VPOX_HGCM_F_PARM_DIRECTION_TO_HOST;
    }
    else
    {
        pReq->Parms.pBuf.type               = VMMDevHGCMParmType_LinAddr_In;
        pReq->Parms.pBuf.u.LinAddr.cb       = cbToWrite;
        pReq->Parms.pBuf.u.LinAddr.uAddr    = (uintptr_t)&pReq->abData[0];
    }

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, cbReq);
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}


/** Request structure for vpoxSfOs2HostReqWrite and VbglR0SfHostReqWriteContig. */
typedef struct VPOXSFWRITEPGLSTREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VPoxSFParmWrite         Parms;
    HGCMPageListInfo        PgLst;
} VPOXSFWRITEPGLSTREQ;

/**
 * SHFL_FN_WRITE request using page list for data buffer (caller populated).
 */
DECLINLINE(int) VbglR0SfHostReqWritePgLst(SHFLROOT idRoot, VPOXSFWRITEPGLSTREQ *pReq, uint64_t hHostFile,
                                          uint64_t offWrite, uint32_t cbToWrite, uint32_t cPages)
{
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_WRITE, SHFL_CPARMS_WRITE,
                                RT_UOFFSETOF_DYN(VPOXSFWRITEPGLSTREQ, PgLst.aPages[cPages]));

    pReq->Parms.id32Root.type               = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32          = idRoot;

    pReq->Parms.u64Handle.type              = VMMDevHGCMParmType_64bit;
    pReq->Parms.u64Handle.u.value64         = hHostFile;

    pReq->Parms.off64Write.type             = VMMDevHGCMParmType_64bit;
    pReq->Parms.off64Write.u.value64        = offWrite;

    pReq->Parms.cb32Write.type              = VMMDevHGCMParmType_32bit;
    pReq->Parms.cb32Write.u.value32         = cbToWrite;

    pReq->Parms.pBuf.type                   = g_fHostFeatures & VMMDEV_HVF_HGCM_NO_BOUNCE_PAGE_LIST
                                            ? VMMDevHGCMParmType_NoBouncePageList : VMMDevHGCMParmType_PageList;;
    pReq->Parms.pBuf.u.PageList.size        = cbToWrite;
    pReq->Parms.pBuf.u.PageList.offset      = RT_UOFFSETOF(VPOXSFWRITEPGLSTREQ, PgLst) - sizeof(VBGLIOCIDCHGCMFASTCALL);
    pReq->PgLst.flags                       = VPOX_HGCM_F_PARM_DIRECTION_TO_HOST;
    pReq->PgLst.cPages                      = (uint16_t)cPages;
    AssertReturn(cPages <= UINT16_MAX, VERR_OUT_OF_RANGE);
    /* caller sets offset */

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr,
                                 RT_UOFFSETOF_DYN(VPOXSFWRITEPGLSTREQ, PgLst.aPages[cPages]));
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}


/**
 * SHFL_FN_WRITE request using a physically contiguous buffer.
 */
DECLINLINE(int) VbglR0SfHostReqWriteContig(SHFLROOT idRoot, VPOXSFWRITEPGLSTREQ *pReq, uint64_t hHostFile,
                                           uint64_t offWrite, uint32_t cbToWrite, void const *pvBuffer, RTGCPHYS64 PhysBuffer)
{
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_WRITE, SHFL_CPARMS_WRITE, RT_UOFFSETOF_DYN(VPOXSFWRITEPGLSTREQ, PgLst.aPages[1]));

    pReq->Parms.id32Root.type               = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32          = idRoot;

    pReq->Parms.u64Handle.type              = VMMDevHGCMParmType_64bit;
    pReq->Parms.u64Handle.u.value64         = hHostFile;

    pReq->Parms.off64Write.type             = VMMDevHGCMParmType_64bit;
    pReq->Parms.off64Write.u.value64        = offWrite;

    pReq->Parms.cb32Write.type              = VMMDevHGCMParmType_32bit;
    pReq->Parms.cb32Write.u.value32         = cbToWrite;

    if (g_fHostFeatures & VMMDEV_HVF_HGCM_CONTIGUOUS_PAGE_LIST)
    {
        pReq->Parms.pBuf.type               = VMMDevHGCMParmType_ContiguousPageList;
        pReq->Parms.pBuf.u.PageList.size    = cbToWrite;
        pReq->Parms.pBuf.u.PageList.offset  = RT_UOFFSETOF(VPOXSFWRITEPGLSTREQ, PgLst) - sizeof(VBGLIOCIDCHGCMFASTCALL);
        pReq->PgLst.flags                   = VPOX_HGCM_F_PARM_DIRECTION_TO_HOST;
        pReq->PgLst.offFirstPage            = (uint16_t)(PhysBuffer & PAGE_OFFSET_MASK);
        pReq->PgLst.cPages                  = 1;
        pReq->PgLst.aPages[0]               = PhysBuffer & ~(RTGCPHYS64)PAGE_OFFSET_MASK;
    }
    else
    {
        pReq->Parms.pBuf.type               = VMMDevHGCMParmType_LinAddr_In;
        pReq->Parms.pBuf.u.LinAddr.cb       = cbToWrite;
        pReq->Parms.pBuf.u.LinAddr.uAddr    = (uintptr_t)pvBuffer;
    }

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, RT_UOFFSETOF_DYN(VPOXSFWRITEPGLSTREQ, PgLst.aPages[1]));
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}


/** Request structure for VbglR0SfHostReqCopyFilePart.  */
typedef struct VPOXSFCOPYFILEPARTREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VPoxSFParmCopyFilePart  Parms;
} VPOXSFCOPYFILEPARTREQ;

/**
 * SHFL_FN_CREATE request.
 */
DECLINLINE(int) VbglR0SfHostReqCopyFilePart(SHFLROOT idRootSrc, SHFLHANDLE hHostFileSrc, uint64_t offSrc,
                                            SHFLROOT idRootDst, SHFLHANDLE hHostFileDst, uint64_t offDst,
                                            uint64_t cbToCopy, uint32_t fFlags, VPOXSFCOPYFILEPARTREQ *pReq)
{
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_COPY_FILE_PART, SHFL_CPARMS_COPY_FILE_PART, sizeof(*pReq));

    pReq->Parms.id32RootSrc.type        = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32RootSrc.u.value32   = idRootSrc;

    pReq->Parms.u64HandleSrc.type       = VMMDevHGCMParmType_64bit;
    pReq->Parms.u64HandleSrc.u.value64  = hHostFileSrc;

    pReq->Parms.off64Src.type           = VMMDevHGCMParmType_64bit;
    pReq->Parms.off64Src.u.value64      = offSrc;

    pReq->Parms.id32RootDst.type        = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32RootDst.u.value32   = idRootDst;

    pReq->Parms.u64HandleDst.type       = VMMDevHGCMParmType_64bit;
    pReq->Parms.u64HandleDst.u.value64  = hHostFileDst;

    pReq->Parms.off64Dst.type           = VMMDevHGCMParmType_64bit;
    pReq->Parms.off64Dst.u.value64      = offDst;

    pReq->Parms.cb64ToCopy.type         = VMMDevHGCMParmType_64bit;
    pReq->Parms.cb64ToCopy.u.value64    = cbToCopy;

    pReq->Parms.f32Flags.type           = VMMDevHGCMParmType_32bit;
    pReq->Parms.f32Flags.u.value32      = fFlags;

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, sizeof(*pReq));
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}



/** Request structure for VbglR0SfHostReqListDirContig2x() and
 *  VbglR0SfHostReqListDir(). */
typedef struct VPOXSFLISTDIRREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VPoxSFParmList          Parms;
    HGCMPageListInfo        StrPgLst;
    HGCMPageListInfo        BufPgLst;
} VPOXSFLISTDIRREQ;

/**
 * SHFL_FN_LIST request with separate string buffer and buffers for entries,
 * both physically contiguous allocations.
 */
DECLINLINE(int) VbglR0SfHostReqListDirContig2x(SHFLROOT idRoot, VPOXSFLISTDIRREQ *pReq, uint64_t hHostDir,
                                               PSHFLSTRING pFilter, RTGCPHYS64 PhysFilter, uint32_t fFlags,
                                               PSHFLDIRINFO pBuffer, RTGCPHYS64 PhysBuffer, uint32_t cbBuffer)
{
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_LIST, SHFL_CPARMS_LIST, sizeof(*pReq));

    pReq->Parms.id32Root.type                       = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32                  = idRoot;

    pReq->Parms.u64Handle.type                      = VMMDevHGCMParmType_64bit;
    pReq->Parms.u64Handle.u.value64                 = hHostDir;

    pReq->Parms.f32Flags.type                       = VMMDevHGCMParmType_32bit;
    pReq->Parms.f32Flags.u.value32                  = fFlags;

    pReq->Parms.cb32Buffer.type                     = VMMDevHGCMParmType_32bit;
    pReq->Parms.cb32Buffer.u.value32                = cbBuffer;

    if (g_fHostFeatures & VMMDEV_HVF_HGCM_CONTIGUOUS_PAGE_LIST)
    {
        pReq->Parms.pStrFilter.type                 = VMMDevHGCMParmType_ContiguousPageList;
        pReq->Parms.pStrFilter.u.PageList.offset    = RT_UOFFSETOF(VPOXSFLISTDIRREQ, StrPgLst) - sizeof(VBGLIOCIDCHGCMFASTCALL);
        pReq->StrPgLst.flags                        = VPOX_HGCM_F_PARM_DIRECTION_TO_HOST;
        pReq->StrPgLst.cPages                       = 1;
        if (pFilter)
        {
            pReq->Parms.pStrFilter.u.PageList.size  = SHFLSTRING_HEADER_SIZE + pFilter->u16Size;
            uint32_t const offFirstPage = (uint32_t)PhysFilter & PAGE_OFFSET_MASK;
            pReq->StrPgLst.offFirstPage             = (uint16_t)offFirstPage;
            pReq->StrPgLst.aPages[0]                = PhysFilter - offFirstPage;
        }
        else
        {
            pReq->Parms.pStrFilter.u.PageList.size  = 0;
            pReq->StrPgLst.offFirstPage             = 0;
            pReq->StrPgLst.aPages[0]                = NIL_RTGCPHYS64;
        }

        pReq->Parms.pBuffer.type                    = VMMDevHGCMParmType_ContiguousPageList;
        pReq->Parms.pBuffer.u.PageList.offset       = RT_UOFFSETOF(VPOXSFLISTDIRREQ, BufPgLst) - sizeof(VBGLIOCIDCHGCMFASTCALL);
        pReq->Parms.pBuffer.u.PageList.size         = cbBuffer;
        pReq->BufPgLst.flags                        = VPOX_HGCM_F_PARM_DIRECTION_FROM_HOST;
        pReq->BufPgLst.cPages                       = 1;
        uint32_t const offFirstPage = (uint32_t)PhysBuffer & PAGE_OFFSET_MASK;
        pReq->BufPgLst.offFirstPage                 = (uint16_t)offFirstPage;
        pReq->BufPgLst.aPages[0]                    = PhysBuffer - offFirstPage;
    }
    else
    {
        pReq->Parms.pStrFilter.type                 = VMMDevHGCMParmType_LinAddr_In;
        pReq->Parms.pStrFilter.u.LinAddr.cb         = pFilter ? SHFLSTRING_HEADER_SIZE + pFilter->u16Size : 0;
        pReq->Parms.pStrFilter.u.LinAddr.uAddr      = (uintptr_t)pFilter;

        pReq->Parms.pBuffer.type                    = VMMDevHGCMParmType_LinAddr_Out;
        pReq->Parms.pBuffer.u.LinAddr.cb            = cbBuffer;
        pReq->Parms.pBuffer.u.LinAddr.uAddr         = (uintptr_t)pBuffer;
    }

    pReq->Parms.f32More.type                        = VMMDevHGCMParmType_32bit;
    pReq->Parms.f32More.u.value32                   = 0;

    pReq->Parms.c32Entries.type                     = VMMDevHGCMParmType_32bit;
    pReq->Parms.c32Entries.u.value32                = 0;

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, sizeof(*pReq));
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}

/**
 * SHFL_FN_LIST request with separate string buffer and buffers for entries,
 * both allocated on the physical heap.
 */
DECLINLINE(int) VbglR0SfHostReqListDir(SHFLROOT idRoot, VPOXSFLISTDIRREQ *pReq, uint64_t hHostDir,
                                       PSHFLSTRING pFilter, uint32_t fFlags, PSHFLDIRINFO pBuffer, uint32_t cbBuffer)
{
    return VbglR0SfHostReqListDirContig2x(idRoot,
                                          pReq,
                                          hHostDir,
                                          pFilter,
                                          pFilter ? VbglR0PhysHeapGetPhysAddr(pFilter) : NIL_RTGCPHYS64,
                                          fFlags,
                                          pBuffer,
                                          VbglR0PhysHeapGetPhysAddr(pBuffer),
                                          cbBuffer);
}


/** Request structure for VbglR0SfHostReqReadLink.  */
typedef struct VPOXSFREADLINKREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VPoxSFParmReadLink      Parms;
    HGCMPageListInfo        PgLst;
    SHFLSTRING              StrPath;
} VPOXSFREADLINKREQ;

/**
 * SHFL_FN_READLINK request.
 *
 * @note Buffer contains UTF-8 characters on success, regardless of the
 *       UTF-8/UTF-16 setting of the connection.
 */
DECLINLINE(int) VbglR0SfHostReqReadLinkContig(SHFLROOT idRoot, void *pvBuffer, RTGCPHYS64 PhysBuffer, uint32_t cbBuffer,
                                              VPOXSFREADLINKREQ *pReq)
{
    uint32_t const cbReq = g_fHostFeatures & VMMDEV_HVF_HGCM_EMBEDDED_BUFFERS
                         ? RT_UOFFSETOF(VPOXSFREADLINKREQ, StrPath.String) + pReq->StrPath.u16Size
                         :    cbBuffer <= PAGE_SIZE - (PhysBuffer & PAGE_OFFSET_MASK)
                           || (g_fHostFeatures & VMMDEV_HVF_HGCM_CONTIGUOUS_PAGE_LIST)
                         ? RT_UOFFSETOF(VPOXSFREADLINKREQ, StrPath.String)
                         : RT_UOFFSETOF(VPOXSFREADLINKREQ, PgLst);
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_READLINK, SHFL_CPARMS_READLINK, cbReq);

    pReq->Parms.id32Root.type                   = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32              = idRoot;

    if (g_fHostFeatures & VMMDEV_HVF_HGCM_EMBEDDED_BUFFERS)
    {
        pReq->Parms.pStrPath.type               = VMMDevHGCMParmType_Embedded;
        pReq->Parms.pStrPath.u.Embedded.cbData  = SHFLSTRING_HEADER_SIZE + pReq->StrPath.u16Size;
        pReq->Parms.pStrPath.u.Embedded.offData = RT_UOFFSETOF(VPOXSFREADLINKREQ, StrPath)
                                                - sizeof(VBGLIOCIDCHGCMFASTCALL);
        pReq->Parms.pStrPath.u.Embedded.fFlags  = VPOX_HGCM_F_PARM_DIRECTION_TO_HOST;
    }
    else
    {
        pReq->Parms.pStrPath.type               = VMMDevHGCMParmType_LinAddr_In;
        pReq->Parms.pStrPath.u.LinAddr.cb       = SHFLSTRING_HEADER_SIZE + pReq->StrPath.u16Size;
        pReq->Parms.pStrPath.u.LinAddr.uAddr    = (uintptr_t)&pReq->StrPath;
    }

    if (   cbBuffer <= PAGE_SIZE - (PhysBuffer & PAGE_OFFSET_MASK)
        || (g_fHostFeatures & VMMDEV_HVF_HGCM_CONTIGUOUS_PAGE_LIST))
    {
        pReq->Parms.pBuffer.type                = cbBuffer <= PAGE_SIZE - (PhysBuffer & PAGE_OFFSET_MASK)
                                                ? VMMDevHGCMParmType_PageList
                                                : VMMDevHGCMParmType_ContiguousPageList;
        pReq->Parms.pBuffer.u.PageList.size     = cbBuffer;
        pReq->Parms.pBuffer.u.PageList.offset   = RT_UOFFSETOF(VPOXSFREADLINKREQ, PgLst)
                                                - sizeof(VBGLIOCIDCHGCMFASTCALL);
        pReq->PgLst.flags                       = VPOX_HGCM_F_PARM_DIRECTION_FROM_HOST;
        pReq->PgLst.offFirstPage                = (uint16_t)PhysBuffer & (uint16_t)(PAGE_OFFSET_MASK);
        pReq->PgLst.aPages[0]                   = PhysBuffer & ~(RTGCPHYS64)PAGE_OFFSET_MASK;
        pReq->PgLst.cPages                      = 1;
    }
    else
    {
        pReq->Parms.pBuffer.type                = VMMDevHGCMParmType_LinAddr_Out;
        pReq->Parms.pBuffer.u.LinAddr.cb        = cbBuffer;
        pReq->Parms.pBuffer.u.LinAddr.uAddr     = (uintptr_t)pvBuffer;
    }

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, cbReq);
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}

/**
 * SHFL_FN_READLINK request, simplified version.
 *
 *
 * @note Buffer contains UTF-8 characters on success, regardless of the
 *       UTF-8/UTF-16 setting of the connection.
 */
DECLINLINE(int) VbglR0SfHostReqReadLinkContigSimple(SHFLROOT idRoot, const char *pszPath, size_t cchPath, void *pvBuf,
                                                    RTGCPHYS64 PhysBuffer, uint32_t cbBuffer)
{
    if (cchPath < _64K - 1)
    {
        VPOXSFREADLINKREQ *pReq = (VPOXSFREADLINKREQ *)VbglR0PhysHeapAlloc(RT_UOFFSETOF(VPOXSFREADLINKREQ, StrPath.String)
                                                                           + SHFLSTRING_HEADER_SIZE + (uint32_t)cchPath);
        if (pReq)
        {
            pReq->StrPath.u16Length = (uint16_t)cchPath;
            pReq->StrPath.u16Size   = (uint16_t)cchPath + 1;
            memcpy(pReq->StrPath.String.ach, pszPath, cchPath);
            pReq->StrPath.String.ach[cchPath] = '\0';

            {
                int vrc = VbglR0SfHostReqReadLinkContig(idRoot, pvBuf, PhysBuffer, cbBuffer, pReq);
                VbglR0PhysHeapFree(pReq);
                return vrc;
            }
        }
        return VERR_NO_MEMORY;
    }
    return VERR_FILENAME_TOO_LONG;
}


/** Request structure for VbglR0SfHostReqCreateSymlink.  */
typedef struct VPOXSFCREATESYMLINKREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VPoxSFParmCreateSymlink Parms;
    HGCMPageListInfo        PgLstTarget;
    SHFLFSOBJINFO           ObjInfo;
    SHFLSTRING              StrSymlinkPath;
} VPOXSFCREATESYMLINKREQ;

/**
 * SHFL_FN_SYMLINK request.
 *
 * Caller fills in the symlink string and supplies a physical contiguous
 * target string
 */
DECLINLINE(int) VbglR0SfHostReqCreateSymlinkContig(SHFLROOT idRoot, PCSHFLSTRING pStrTarget, RTGCPHYS64 PhysTarget,
                                                   VPOXSFCREATESYMLINKREQ *pReq)
{
    uint32_t const cbTarget = SHFLSTRING_HEADER_SIZE + pStrTarget->u16Size;
    uint32_t const cbReq    = g_fHostFeatures & VMMDEV_HVF_HGCM_EMBEDDED_BUFFERS
                            ? RT_UOFFSETOF(VPOXSFCREATESYMLINKREQ, StrSymlinkPath.String) + pReq->StrSymlinkPath.u16Size
                            : RT_UOFFSETOF(VPOXSFCREATESYMLINKREQ, ObjInfo) /*simplified*/;
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_SYMLINK, SHFL_CPARMS_SYMLINK, cbReq);

    pReq->Parms.id32Root.type                          = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32                     = idRoot;

    if (g_fHostFeatures & VMMDEV_HVF_HGCM_EMBEDDED_BUFFERS)
    {
        pReq->Parms.pStrSymlink.type               = VMMDevHGCMParmType_Embedded;
        pReq->Parms.pStrSymlink.u.Embedded.cbData  = SHFLSTRING_HEADER_SIZE + pReq->StrSymlinkPath.u16Size;
        pReq->Parms.pStrSymlink.u.Embedded.offData = RT_UOFFSETOF(VPOXSFCREATESYMLINKREQ, StrSymlinkPath)
                                                   - sizeof(VBGLIOCIDCHGCMFASTCALL);
        pReq->Parms.pStrSymlink.u.Embedded.fFlags  = VPOX_HGCM_F_PARM_DIRECTION_TO_HOST;
    }
    else
    {
        pReq->Parms.pStrSymlink.type               = VMMDevHGCMParmType_LinAddr_In;
        pReq->Parms.pStrSymlink.u.LinAddr.cb       = SHFLSTRING_HEADER_SIZE + pReq->StrSymlinkPath.u16Size;
        pReq->Parms.pStrSymlink.u.LinAddr.uAddr    = (uintptr_t)&pReq->StrSymlinkPath;
    }

    if (   cbTarget <= PAGE_SIZE - (PhysTarget & PAGE_OFFSET_MASK)
        || (g_fHostFeatures & VMMDEV_HVF_HGCM_CONTIGUOUS_PAGE_LIST))
    {
        pReq->Parms.pStrTarget.type                = cbTarget <= PAGE_SIZE - (PhysTarget & PAGE_OFFSET_MASK)
                                                   ? VMMDevHGCMParmType_PageList
                                                   : VMMDevHGCMParmType_ContiguousPageList;
        pReq->Parms.pStrTarget.u.PageList.size     = cbTarget;
        pReq->Parms.pStrTarget.u.PageList.offset   = RT_UOFFSETOF(VPOXSFCREATESYMLINKREQ, PgLstTarget)
                                                   - sizeof(VBGLIOCIDCHGCMFASTCALL);
        pReq->PgLstTarget.flags                    = VPOX_HGCM_F_PARM_DIRECTION_TO_HOST;
        pReq->PgLstTarget.offFirstPage             = (uint16_t)PhysTarget & (uint16_t)(PAGE_OFFSET_MASK);
        pReq->PgLstTarget.aPages[0]                = PhysTarget & ~(RTGCPHYS64)PAGE_OFFSET_MASK;
        pReq->PgLstTarget.cPages                   = 1;
    }
    else
    {
        pReq->Parms.pStrTarget.type                = VMMDevHGCMParmType_LinAddr_In;
        pReq->Parms.pStrTarget.u.LinAddr.cb        = cbTarget;
        pReq->Parms.pStrTarget.u.LinAddr.uAddr     = (uintptr_t)pStrTarget;
    }

    if (g_fHostFeatures & VMMDEV_HVF_HGCM_EMBEDDED_BUFFERS)
    {
        pReq->Parms.pInfo.type                     = VMMDevHGCMParmType_Embedded;
        pReq->Parms.pInfo.u.Embedded.cbData        = sizeof(pReq->ObjInfo);
        pReq->Parms.pInfo.u.Embedded.offData       = RT_UOFFSETOF(VPOXSFCREATESYMLINKREQ, ObjInfo)
                                                   - sizeof(VBGLIOCIDCHGCMFASTCALL);
        pReq->Parms.pInfo.u.Embedded.fFlags        = VPOX_HGCM_F_PARM_DIRECTION_FROM_HOST;
    }
    else
    {
        pReq->Parms.pInfo.type                     = VMMDevHGCMParmType_LinAddr_Out;
        pReq->Parms.pInfo.u.LinAddr.cb             = sizeof(pReq->ObjInfo);
        pReq->Parms.pInfo.u.LinAddr.uAddr          = (uintptr_t)&pReq->ObjInfo;
    }

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, cbReq);
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}

/** @} */

#endif /* !VPOX_INCLUDED_VPoxGuestLibSharedFoldersInline_h */

