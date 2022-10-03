/* $Id: DevVGA_VDMA.cpp $ */
/** @file
 * Video DMA (VDMA) support.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_VGA
#include <VPox/VMMDev.h>
#include <VPox/vmm/pdmdev.h>
#include <VPox/vmm/pgm.h>
#include <VPoxVideo.h>
#include <VPox/AssertGuest.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>
#include <iprt/mem.h>
#include <iprt/asm.h>
#include <iprt/list.h>
#include <iprt/param.h>

#include "DevVGA.h"
#include "HGSMI/SHGSMIHost.h"

#ifdef DEBUG_misha
# define VPOXVDBG_MEMCACHE_DISABLE
#endif

#ifndef VPOXVDBG_MEMCACHE_DISABLE
# include <iprt/memcache.h>
#endif


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#ifdef DEBUG_misha
# define WARN_BP() do { AssertFailed(); } while (0)
#else
# define WARN_BP() do { } while (0)
#endif
#define WARN(_msg) do { \
        LogRel(_msg); \
        WARN_BP(); \
    } while (0)

#define VPOXVDMATHREAD_STATE_TERMINATED             0
#define VPOXVDMATHREAD_STATE_CREATING               1
#define VPOXVDMATHREAD_STATE_CREATED                3
#define VPOXVDMATHREAD_STATE_TERMINATING            4


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
struct VPOXVDMATHREAD;

typedef DECLCALLBACKPTR(void, PFNVPOXVDMATHREAD_CHANGED,(struct VPOXVDMATHREAD *pThread, int rc, void *pvThreadContext, void *pvChangeContext));

typedef struct VPOXVDMATHREAD
{
    RTTHREAD hWorkerThread;
    RTSEMEVENT hEvent;
    volatile uint32_t u32State;
    PFNVPOXVDMATHREAD_CHANGED pfnChanged;
    void *pvChanged;
} VPOXVDMATHREAD, *PVPOXVDMATHREAD;


/* state transformations:
 *
 *   submitter   |    processor
 *
 *  LISTENING   --->  PROCESSING
 *
 *  */
#define VBVAEXHOSTCONTEXT_STATE_LISTENING      0
#define VBVAEXHOSTCONTEXT_STATE_PROCESSING     1

#define VBVAEXHOSTCONTEXT_ESTATE_DISABLED     -1
#define VBVAEXHOSTCONTEXT_ESTATE_PAUSED        0
#define VBVAEXHOSTCONTEXT_ESTATE_ENABLED       1

typedef struct VBVAEXHOSTCONTEXT
{
    VBVABUFFER RT_UNTRUSTED_VOLATILE_GUEST *pVBVA;
    /** Maximum number of data bytes addressible relative to pVBVA. */
    uint32_t                                cbMaxData;
    volatile int32_t i32State;
    volatile int32_t i32EnableState;
    volatile uint32_t u32cCtls;
    /* critical section for accessing ctl lists */
    RTCRITSECT CltCritSect;
    RTLISTANCHOR GuestCtlList;
    RTLISTANCHOR HostCtlList;
#ifndef VPOXVDBG_MEMCACHE_DISABLE
    RTMEMCACHE CtlCache;
#endif
} VBVAEXHOSTCONTEXT;

typedef enum
{
    VBVAEXHOSTCTL_TYPE_UNDEFINED = 0,
    VBVAEXHOSTCTL_TYPE_HH_INTERNAL_PAUSE,
    VBVAEXHOSTCTL_TYPE_HH_INTERNAL_RESUME,
    VBVAEXHOSTCTL_TYPE_HH_SAVESTATE,
    VBVAEXHOSTCTL_TYPE_HH_LOADSTATE,
    VBVAEXHOSTCTL_TYPE_HH_LOADSTATE_DONE,
    VBVAEXHOSTCTL_TYPE_HH_BE_OPAQUE,
    VBVAEXHOSTCTL_TYPE_HH_ON_HGCM_UNLOAD,
    VBVAEXHOSTCTL_TYPE_GHH_BE_OPAQUE,
    VBVAEXHOSTCTL_TYPE_GHH_ENABLE,
    VBVAEXHOSTCTL_TYPE_GHH_ENABLE_PAUSED,
    VBVAEXHOSTCTL_TYPE_GHH_DISABLE,
    VBVAEXHOSTCTL_TYPE_GHH_RESIZE
} VBVAEXHOSTCTL_TYPE;

struct VBVAEXHOSTCTL;

typedef DECLCALLBACK(void) FNVBVAEXHOSTCTL_COMPLETE(VBVAEXHOSTCONTEXT *pVbva, struct VBVAEXHOSTCTL *pCtl, int rc, void *pvComplete);
typedef FNVBVAEXHOSTCTL_COMPLETE *PFNVBVAEXHOSTCTL_COMPLETE;

typedef struct VBVAEXHOSTCTL
{
    RTLISTNODE Node;
    VBVAEXHOSTCTL_TYPE enmType;
    union
    {
        struct
        {
            void RT_UNTRUSTED_VOLATILE_GUEST *pvCmd;
            uint32_t cbCmd;
        } cmd;

        struct
        {
            PSSMHANDLE pSSM;
            uint32_t u32Version;
        } state;
    } u;
    PFNVBVAEXHOSTCTL_COMPLETE pfnComplete;
    void *pvComplete;
} VBVAEXHOSTCTL;

/* VPoxVBVAExHP**, i.e. processor functions, can NOT be called concurrently with each other,
 * but can be called with other VPoxVBVAExS** (submitter) functions except Init/Start/Term aparently.
 * Can only be called be the processor, i.e. the entity that acquired the processor state by direct or indirect call to the VPoxVBVAExHSCheckCommands
 * see mor edetailed comments in headers for function definitions */
typedef enum
{
    VBVAEXHOST_DATA_TYPE_NO_DATA = 0,
    VBVAEXHOST_DATA_TYPE_CMD,
    VBVAEXHOST_DATA_TYPE_HOSTCTL,
    VBVAEXHOST_DATA_TYPE_GUESTCTL
} VBVAEXHOST_DATA_TYPE;


typedef struct VPOXVDMAHOST
{
    PHGSMIINSTANCE pHgsmi; /**< Same as VGASTATE::pHgsmi. */
    PVGASTATE pThis;
} VPOXVDMAHOST, *PVPOXVDMAHOST;


/**
 * List selector for VPoxVBVAExHCtlSubmit(), vdmaVBVACtlSubmit().
 */
typedef enum
{
    VBVAEXHOSTCTL_SOURCE_GUEST = 0,
    VBVAEXHOSTCTL_SOURCE_HOST
} VBVAEXHOSTCTL_SOURCE;




/**
 * Called by vgaR3Construct() to initialize the state.
 *
 * @returns VPox status code.
 */
int vpoxVDMAConstruct(PVGASTATE pThis, PVGASTATECC pThisCC, uint32_t cPipeElements)
{
    RT_NOREF(cPipeElements);
    PVPOXVDMAHOST pVdma = (PVPOXVDMAHOST)RTMemAllocZ(sizeof(*pVdma));
    Assert(pVdma);
    if (pVdma)
    {
        pVdma->pHgsmi = pThisCC->pHGSMI;
        pVdma->pThis  = pThis;

        pThisCC->pVdma = pVdma;
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}

/**
 * Called by vgaR3Reset() to do reset.
 */
void  vpoxVDMAReset(struct VPOXVDMAHOST *pVdma)
{
    RT_NOREF(pVdma);
}

/**
 * Called by vgaR3Destruct() to do cleanup.
 */
void vpoxVDMADestruct(struct VPOXVDMAHOST *pVdma)
{
    if (!pVdma)
        return;
    RTMemFree(pVdma);
}

/**
 * Handle VBVA_VDMA_CTL, see vbvaChannelHandler
 *
 * @param   pVdma   The VDMA channel.
 * @param   pCmd    The control command to handle.  Considered volatile.
 * @param   cbCmd   The size of the command.  At least sizeof(VPOXVDMA_CTL).
 */
void vpoxVDMAControl(struct VPOXVDMAHOST *pVdma, VPOXVDMA_CTL RT_UNTRUSTED_VOLATILE_GUEST *pCmd, uint32_t cbCmd)
{
    RT_NOREF(cbCmd);
    PHGSMIINSTANCE pIns = pVdma->pHgsmi;

    VPOXVDMA_CTL_TYPE enmCtl = pCmd->enmCtl;
    RT_UNTRUSTED_NONVOLATILE_COPY_FENCE();

    int rc;
    if (enmCtl < VPOXVDMA_CTL_TYPE_END)
    {
        RT_UNTRUSTED_VALIDATED_FENCE();

        switch (enmCtl)
        {
            case VPOXVDMA_CTL_TYPE_ENABLE:
                rc = VINF_SUCCESS;
                break;
            case VPOXVDMA_CTL_TYPE_DISABLE:
                rc = VINF_SUCCESS;
                break;
            case VPOXVDMA_CTL_TYPE_FLUSH:
                rc = VINF_SUCCESS;
                break;
            case VPOXVDMA_CTL_TYPE_WATCHDOG:
                rc = VERR_NOT_SUPPORTED;
                break;
            default:
                AssertFailedBreakStmt(rc = VERR_IPE_NOT_REACHED_DEFAULT_CASE);
        }
    }
    else
    {
        RT_UNTRUSTED_VALIDATED_FENCE();
        ASSERT_GUEST_FAILED();
        rc = VERR_NOT_SUPPORTED;
    }

    pCmd->i32Result = rc;
    rc = VPoxSHGSMICommandComplete(pIns, pCmd);
    AssertRC(rc);
}

/**
 * Handle VBVA_VDMA_CMD, see vbvaChannelHandler().
 *
 * @param   pVdma   The VDMA channel.
 * @param   pCmd    The command to handle.  Considered volatile.
 * @param   cbCmd   The size of the command.  At least sizeof(VPOXVDMACBUF_DR).
 * @thread  EMT
 */
void vpoxVDMACommand(struct VPOXVDMAHOST *pVdma, VPOXVDMACBUF_DR RT_UNTRUSTED_VOLATILE_GUEST *pCmd, uint32_t cbCmd)
{
    /*
     * Process the command.
     */
    bool fAsyncCmd = false;
    RT_NOREF(cbCmd);
    int rc = VERR_NOT_IMPLEMENTED;

    /*
     * Complete the command unless it's asynchronous (e.g. chromium).
     */
    if (!fAsyncCmd)
    {
        pCmd->rc = rc;
        int rc2 = VPoxSHGSMICommandComplete(pVdma->pHgsmi, pCmd);
        AssertRC(rc2);
    }
}



/*
 *
 *
 * Saved state.
 * Saved state.
 * Saved state.
 *
 *
 */

int vpoxVDMASaveStateExecPrep(struct VPOXVDMAHOST *pVdma)
{
    RT_NOREF(pVdma);
    return VINF_SUCCESS;
}

int vpoxVDMASaveStateExecDone(struct VPOXVDMAHOST *pVdma)
{
    RT_NOREF(pVdma);
    return VINF_SUCCESS;
}

int vpoxVDMASaveStateExecPerform(PCPDMDEVHLPR3 pHlp, struct VPOXVDMAHOST *pVdma, PSSMHANDLE pSSM)
{
    int rc;
    RT_NOREF(pVdma);

    rc = pHlp->pfnSSMPutU32(pSSM, UINT32_MAX);
    AssertRCReturn(rc, rc);
    return VINF_SUCCESS;
}

int vpoxVDMASaveLoadExecPerform(PCPDMDEVHLPR3 pHlp, struct VPOXVDMAHOST *pVdma, PSSMHANDLE pSSM, uint32_t u32Version)
{
    uint32_t u32;
    int rc = pHlp->pfnSSMGetU32(pSSM, &u32);
    AssertLogRelRCReturn(rc, rc);

    if (u32 != UINT32_MAX)
    {
        RT_NOREF(pVdma, u32Version);
        WARN(("Unsupported VBVACtl info!\n"));
        return VERR_VERSION_MISMATCH;
    }

    return VINF_SUCCESS;
}

int vpoxVDMASaveLoadDone(struct VPOXVDMAHOST *pVdma)
{
    RT_NOREF(pVdma);
    return VINF_SUCCESS;
}

