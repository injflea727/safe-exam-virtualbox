/* $Id: VPoxSCSI.cpp $ */
/** @file
 * VPox storage devices - Simple SCSI interface for BIOS access.
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
//#define DEBUG
#define LOG_GROUP LOG_GROUP_DEV_BUSLOGIC /** @todo Create extra group. */

#if defined(IN_R0) || defined(IN_RC)
# error This device has no R0 or RC components
#endif

#include <VPox/vmm/pdmdev.h>
#include <VPox/vmm/pgm.h>
#include <VPox/version.h>
#include <iprt/asm.h>
#include <iprt/mem.h>
#include <iprt/thread.h>
#include <iprt/string.h>

#include "VPoxSCSI.h"


/**
 * Resets the state.
 */
static void vpoxscsiReset(PVPOXSCSI pVPoxSCSI, bool fEverything)
{
    if (fEverything)
    {
        pVPoxSCSI->regIdentify = 0;
        pVPoxSCSI->fBusy       = false;
    }
    pVPoxSCSI->cbCDB         = 0;
    RT_ZERO(pVPoxSCSI->abCDB);
    pVPoxSCSI->iCDB          = 0;
    pVPoxSCSI->rcCompletion  = 0;
    pVPoxSCSI->uTargetDevice = 0;
    pVPoxSCSI->cbBuf         = 0;
    pVPoxSCSI->cbBufLeft     = 0;
    pVPoxSCSI->iBuf          = 0;
    if (pVPoxSCSI->pbBuf)
        RTMemFree(pVPoxSCSI->pbBuf);
    pVPoxSCSI->pbBuf         = NULL;
    pVPoxSCSI->enmState      = VPOXSCSISTATE_NO_COMMAND;
}

/**
 * Initializes the state for the SCSI interface.
 *
 * @returns VPox status code.
 * @param   pVPoxSCSI    Pointer to the unitialized SCSI state.
 */
int vpoxscsiInitialize(PVPOXSCSI pVPoxSCSI)
{
    int rc = RTCritSectInit(&pVPoxSCSI->CritSect);
    if (RT_SUCCESS(rc))
    {
        pVPoxSCSI->pbBuf = NULL;
        vpoxscsiReset(pVPoxSCSI, true /*fEverything*/);
    }

    return rc;
}

/**
 * Frees all allocated resources.
 *
 * @returns nothing.
 * @param   pVPoxSCSI    Pointer to the SCSI state,
 */
void vpoxscsiDestroy(PVPOXSCSI pVPoxSCSI)
{
    if (RTCritSectIsInitialized(&pVPoxSCSI->CritSect))
        RTCritSectDelete(&pVPoxSCSI->CritSect);
}

/**
 * Performs a hardware reset.
 *
 * @returns nothing.
 * @param   pVPoxSCSI    Pointer to the SCSI state,
 */
void vpoxscsiHwReset(PVPOXSCSI pVPoxSCSI)
{
    vpoxscsiReset(pVPoxSCSI, true /*fEverything*/);
}

/**
 * Reads a register value.
 *
 * @retval  VINF_SUCCESS
 * @param   pVPoxSCSI    Pointer to the SCSI state.
 * @param   iRegister    Index of the register to read.
 * @param   pu32Value    Where to store the content of the register.
 */
int vpoxscsiReadRegister(PVPOXSCSI pVPoxSCSI, uint8_t iRegister, uint32_t *pu32Value)
{
    uint8_t uVal = 0;

    RTCritSectEnter(&pVPoxSCSI->CritSect);
    switch (iRegister)
    {
        case 0:
        {
            if (ASMAtomicReadBool(&pVPoxSCSI->fBusy) == true)
            {
                uVal |= VPOX_SCSI_BUSY;
                /* There is an I/O operation in progress.
                 * Yield the execution thread to let the I/O thread make progress.
                 */
                RTThreadYield();
            }
            if (pVPoxSCSI->rcCompletion)
                uVal |= VPOX_SCSI_ERROR;
            break;
        }
        case 1:
        {
            /* If we're not in the 'command ready' state, there may not even be a buffer yet. */
            if (   pVPoxSCSI->enmState == VPOXSCSISTATE_COMMAND_READY
                && pVPoxSCSI->cbBufLeft > 0
                && pVPoxSCSI->pbBuf)
            {
                AssertMsg(pVPoxSCSI->pbBuf, ("pBuf is NULL\n"));
                Assert(!pVPoxSCSI->fBusy);
                uVal = pVPoxSCSI->pbBuf[pVPoxSCSI->iBuf];
                pVPoxSCSI->iBuf++;
                pVPoxSCSI->cbBufLeft--;

                /* When the guest reads the last byte from the data in buffer, clear
                   everything and reset command buffer. */
                if (pVPoxSCSI->cbBufLeft == 0)
                    vpoxscsiReset(pVPoxSCSI, false /*fEverything*/);
            }
            break;
        }
        case 2:
        {
            uVal = pVPoxSCSI->regIdentify;
            break;
        }
        case 3:
        {
            uVal = pVPoxSCSI->rcCompletion;
            break;
        }
        default:
            AssertMsgFailed(("Invalid register to read from %u\n", iRegister));
    }

    *pu32Value = uVal;
    RTCritSectLeave(&pVPoxSCSI->CritSect);

    return VINF_SUCCESS;
}

/**
 * Writes to a register.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_MORE_DATA if a command is ready to be sent to the SCSI driver.
 *
 * @param   pVPoxSCSI    Pointer to the SCSI state.
 * @param   iRegister    Index of the register to write to.
 * @param   uVal         Value to write.
 */
int vpoxscsiWriteRegister(PVPOXSCSI pVPoxSCSI, uint8_t iRegister, uint8_t uVal)
{
    int rc = VINF_SUCCESS;

    RTCritSectEnter(&pVPoxSCSI->CritSect);
    switch (iRegister)
    {
        case 0:
        {
            if (pVPoxSCSI->enmState == VPOXSCSISTATE_NO_COMMAND)
            {
                pVPoxSCSI->enmState = VPOXSCSISTATE_READ_TXDIR;
                pVPoxSCSI->uTargetDevice = uVal;
            }
            else if (pVPoxSCSI->enmState == VPOXSCSISTATE_READ_TXDIR)
            {
                if (uVal != VPOXSCSI_TXDIR_FROM_DEVICE && uVal != VPOXSCSI_TXDIR_TO_DEVICE)
                    vpoxscsiReset(pVPoxSCSI, true /*fEverything*/);
                else
                {
                    pVPoxSCSI->enmState = VPOXSCSISTATE_READ_CDB_SIZE_BUFHI;
                    pVPoxSCSI->uTxDir = uVal;
                }
            }
            else if (pVPoxSCSI->enmState == VPOXSCSISTATE_READ_CDB_SIZE_BUFHI)
            {
                uint8_t cbCDB = uVal & 0x0F;

                if (cbCDB == 0)
                    cbCDB = 16;
                if (cbCDB > VPOXSCSI_CDB_SIZE_MAX)
                    vpoxscsiReset(pVPoxSCSI, true /*fEverything*/);
                else
                {
                    pVPoxSCSI->enmState = VPOXSCSISTATE_READ_BUFFER_SIZE_LSB;
                    pVPoxSCSI->cbCDB = cbCDB;
                    pVPoxSCSI->cbBuf = (uVal & 0xF0) << 12;     /* Bits 16-19 of buffer size. */
                }
            }
            else if (pVPoxSCSI->enmState == VPOXSCSISTATE_READ_BUFFER_SIZE_LSB)
            {
                pVPoxSCSI->enmState = VPOXSCSISTATE_READ_BUFFER_SIZE_MID;
                pVPoxSCSI->cbBuf |= uVal;                       /* Bits 0-7 of buffer size. */
            }
            else if (pVPoxSCSI->enmState == VPOXSCSISTATE_READ_BUFFER_SIZE_MID)
            {
                pVPoxSCSI->enmState = VPOXSCSISTATE_READ_COMMAND;
                pVPoxSCSI->cbBuf |= (((uint16_t)uVal) << 8);    /* Bits 8-15 of buffer size. */
            }
            else if (pVPoxSCSI->enmState == VPOXSCSISTATE_READ_COMMAND)
            {
                pVPoxSCSI->abCDB[pVPoxSCSI->iCDB] = uVal;
                pVPoxSCSI->iCDB++;

                /* Check if we have all necessary command data. */
                if (pVPoxSCSI->iCDB == pVPoxSCSI->cbCDB)
                {
                    Log(("%s: Command ready for processing\n", __FUNCTION__));
                    pVPoxSCSI->enmState = VPOXSCSISTATE_COMMAND_READY;
                    Assert(!pVPoxSCSI->cbBufLeft);
                    Assert(!pVPoxSCSI->pbBuf);
                    if (pVPoxSCSI->uTxDir == VPOXSCSI_TXDIR_TO_DEVICE)
                    {
                        /* This is a write allocate buffer. */
                        pVPoxSCSI->pbBuf = (uint8_t *)RTMemAllocZ(pVPoxSCSI->cbBuf);
                        if (!pVPoxSCSI->pbBuf)
                            return VERR_NO_MEMORY;
                        pVPoxSCSI->cbBufLeft = pVPoxSCSI->cbBuf;
                    }
                    else
                    {
                        /* This is a read from the device. */
                        pVPoxSCSI->cbBufLeft = pVPoxSCSI->cbBuf;
                        ASMAtomicXchgBool(&pVPoxSCSI->fBusy, true);
                        rc = VERR_MORE_DATA; /** @todo Better return value to indicate ready command? */
                    }
                }
            }
            else
                AssertMsgFailed(("Invalid state %d\n", pVPoxSCSI->enmState));
            break;
        }

        case 1:
        {
            if (   pVPoxSCSI->enmState != VPOXSCSISTATE_COMMAND_READY
                || pVPoxSCSI->uTxDir != VPOXSCSI_TXDIR_TO_DEVICE)
            {
                /* Reset the state */
                vpoxscsiReset(pVPoxSCSI, true /*fEverything*/);
            }
            else if (pVPoxSCSI->cbBufLeft > 0)
            {
                pVPoxSCSI->pbBuf[pVPoxSCSI->iBuf++] = uVal;
                pVPoxSCSI->cbBufLeft--;
                if (pVPoxSCSI->cbBufLeft == 0)
                {
                    rc = VERR_MORE_DATA;
                    ASMAtomicXchgBool(&pVPoxSCSI->fBusy, true);
                }
            }
            /* else: Ignore extra data, request pending or something. */
            break;
        }

        case 2:
        {
            pVPoxSCSI->regIdentify = uVal;
            break;
        }

        case 3:
        {
            /* Reset */
            vpoxscsiReset(pVPoxSCSI, true /*fEverything*/);
            break;
        }

        default:
            AssertMsgFailed(("Invalid register to write to %u\n", iRegister));
    }

    RTCritSectLeave(&pVPoxSCSI->CritSect);
    return rc;
}

/**
 * Sets up a SCSI request which the owning SCSI device can process.
 *
 * @returns VPox status code.
 * @param   pVPoxSCSI      Pointer to the SCSI state.
 * @param   puLun          Where to store the LUN on success.
 * @param   ppbCdb         Where to store the pointer to the CDB on success.
 * @param   pcbCdb         Where to store the size of the CDB on success.
 * @param   pcbBuf         Where to store th size of the data buffer on success.
 * @param   puTargetDevice Where to store the target device ID.
 */
int vpoxscsiSetupRequest(PVPOXSCSI pVPoxSCSI, uint32_t *puLun, uint8_t **ppbCdb,
                         size_t *pcbCdb, size_t *pcbBuf, uint32_t *puTargetDevice)
{
    int rc = VINF_SUCCESS;

    LogFlowFunc(("pVPoxSCSI=%#p puTargetDevice=%#p\n", pVPoxSCSI, puTargetDevice));

    RTCritSectEnter(&pVPoxSCSI->CritSect);
    AssertMsg(pVPoxSCSI->enmState == VPOXSCSISTATE_COMMAND_READY, ("Invalid state %u\n", pVPoxSCSI->enmState));

    /* Clear any errors from a previous request. */
    pVPoxSCSI->rcCompletion = 0;

    if (pVPoxSCSI->uTxDir == VPOXSCSI_TXDIR_FROM_DEVICE)
    {
        if (pVPoxSCSI->pbBuf)
            RTMemFree(pVPoxSCSI->pbBuf);

        pVPoxSCSI->pbBuf = (uint8_t *)RTMemAllocZ(pVPoxSCSI->cbBuf);
        if (!pVPoxSCSI->pbBuf)
            return VERR_NO_MEMORY;
    }

    *puLun = 0;
    *ppbCdb = &pVPoxSCSI->abCDB[0];
    *pcbCdb = pVPoxSCSI->cbCDB;
    *pcbBuf = pVPoxSCSI->cbBuf;
    *puTargetDevice = pVPoxSCSI->uTargetDevice;
    RTCritSectLeave(&pVPoxSCSI->CritSect);

    return rc;
}

/**
 * Notifies the device that a request finished and the incoming data
 * is ready at the incoming data port.
 */
int vpoxscsiRequestFinished(PVPOXSCSI pVPoxSCSI, int rcCompletion)
{
    LogFlowFunc(("pVPoxSCSI=%#p\n", pVPoxSCSI));

    RTCritSectEnter(&pVPoxSCSI->CritSect);
    if (pVPoxSCSI->uTxDir == VPOXSCSI_TXDIR_TO_DEVICE)
        vpoxscsiReset(pVPoxSCSI, false /*fEverything*/);

    pVPoxSCSI->rcCompletion = rcCompletion;

    ASMAtomicXchgBool(&pVPoxSCSI->fBusy, false);
    RTCritSectLeave(&pVPoxSCSI->CritSect);

    return VINF_SUCCESS;
}

size_t vpoxscsiCopyToBuf(PVPOXSCSI pVPoxSCSI, PRTSGBUF pSgBuf, size_t cbSkip, size_t cbCopy)
{
    AssertPtrReturn(pVPoxSCSI->pbBuf, 0);
    AssertReturn(cbSkip + cbCopy <= pVPoxSCSI->cbBuf, 0);

    RTCritSectEnter(&pVPoxSCSI->CritSect);
    void *pvBuf = pVPoxSCSI->pbBuf + cbSkip;
    size_t cbCopied = RTSgBufCopyToBuf(pSgBuf, pvBuf, cbCopy);
    RTCritSectLeave(&pVPoxSCSI->CritSect);

    return cbCopied;
}

size_t vpoxscsiCopyFromBuf(PVPOXSCSI pVPoxSCSI, PRTSGBUF pSgBuf, size_t cbSkip, size_t cbCopy)
{
    AssertPtrReturn(pVPoxSCSI->pbBuf, 0);
    AssertReturn(cbSkip + cbCopy <= pVPoxSCSI->cbBuf, 0);

    RTCritSectEnter(&pVPoxSCSI->CritSect);
    void *pvBuf = pVPoxSCSI->pbBuf + cbSkip;
    size_t cbCopied = RTSgBufCopyFromBuf(pSgBuf, pvBuf, cbCopy);
    RTCritSectLeave(&pVPoxSCSI->CritSect);

    return cbCopied;
}

/**
 * @retval VINF_SUCCESS
 */
int vpoxscsiReadString(PPDMDEVINS pDevIns, PVPOXSCSI pVPoxSCSI, uint8_t iRegister,
                       uint8_t *pbDst, uint32_t *pcTransfers, unsigned cb)
{
    RT_NOREF(pDevIns);
    LogFlowFunc(("pDevIns=%#p pVPoxSCSI=%#p iRegister=%d cTransfers=%u cb=%u\n",
                 pDevIns, pVPoxSCSI, iRegister, *pcTransfers, cb));

    /*
     * Check preconditions, fall back to non-string I/O handler.
     */
    Assert(*pcTransfers > 0);

    /* Read string only valid for data in register. */
    AssertMsgReturn(iRegister == 1, ("Hey! Only register 1 can be read from with string!\n"), VINF_SUCCESS);

    /* Accesses without a valid buffer will be ignored. */
    AssertReturn(pVPoxSCSI->pbBuf, VINF_SUCCESS);

    /* Check state. */
    AssertReturn(pVPoxSCSI->enmState == VPOXSCSISTATE_COMMAND_READY, VINF_SUCCESS);
    Assert(!pVPoxSCSI->fBusy);

    RTCritSectEnter(&pVPoxSCSI->CritSect);
    /*
     * Also ignore attempts to read more data than is available.
     */
    uint32_t cbTransfer = *pcTransfers * cb;
    if (pVPoxSCSI->cbBufLeft > 0)
    {
        Assert(cbTransfer <= pVPoxSCSI->cbBufLeft);
        if (cbTransfer > pVPoxSCSI->cbBufLeft)
        {
            memset(pbDst + pVPoxSCSI->cbBufLeft, 0xff, cbTransfer - pVPoxSCSI->cbBufLeft);
            cbTransfer = pVPoxSCSI->cbBufLeft;  /* Ignore excess data (not supposed to happen). */
        }

        /* Copy the data and adance the buffer position. */
        memcpy(pbDst, pVPoxSCSI->pbBuf + pVPoxSCSI->iBuf, cbTransfer);

        /* Advance current buffer position. */
        pVPoxSCSI->iBuf      += cbTransfer;
        pVPoxSCSI->cbBufLeft -= cbTransfer;

        /* When the guest reads the last byte from the data in buffer, clear
           everything and reset command buffer. */
        if (pVPoxSCSI->cbBufLeft == 0)
            vpoxscsiReset(pVPoxSCSI, false /*fEverything*/);
    }
    else
    {
        AssertFailed();
        memset(pbDst, 0, cbTransfer);
    }
    *pcTransfers = 0;
    RTCritSectLeave(&pVPoxSCSI->CritSect);

    return VINF_SUCCESS;
}

/**
 * @retval VINF_SUCCESS
 * @retval VERR_MORE_DATA
 */
int vpoxscsiWriteString(PPDMDEVINS pDevIns, PVPOXSCSI pVPoxSCSI, uint8_t iRegister,
                        uint8_t const *pbSrc, uint32_t *pcTransfers, unsigned cb)
{
    RT_NOREF(pDevIns);

    /*
     * Check preconditions, fall back to non-string I/O handler.
     */
    Assert(*pcTransfers > 0);
    /* Write string only valid for data in/out register. */
    AssertMsgReturn(iRegister == 1, ("Hey! Only register 1 can be written to with string!\n"), VINF_SUCCESS);

    /* Accesses without a valid buffer will be ignored. */
    AssertReturn(pVPoxSCSI->pbBuf, VINF_SUCCESS);

    /* State machine assumptions. */
    AssertReturn(pVPoxSCSI->enmState == VPOXSCSISTATE_COMMAND_READY, VINF_SUCCESS);
    AssertReturn(pVPoxSCSI->uTxDir == VPOXSCSI_TXDIR_TO_DEVICE, VINF_SUCCESS);

    RTCritSectEnter(&pVPoxSCSI->CritSect);
    /*
     * Ignore excess data (not supposed to happen).
     */
    int rc = VINF_SUCCESS;
    if (pVPoxSCSI->cbBufLeft > 0)
    {
        uint32_t cbTransfer = RT_MIN(*pcTransfers * cb, pVPoxSCSI->cbBufLeft);

        /* Copy the data and adance the buffer position. */
        memcpy(pVPoxSCSI->pbBuf + pVPoxSCSI->iBuf, pbSrc, cbTransfer);
        pVPoxSCSI->iBuf      += cbTransfer;
        pVPoxSCSI->cbBufLeft -= cbTransfer;

        /* If we've reached the end, tell the caller to submit the command. */
        if (pVPoxSCSI->cbBufLeft == 0)
        {
            ASMAtomicXchgBool(&pVPoxSCSI->fBusy, true);
            rc = VERR_MORE_DATA;
        }
    }
    else
        AssertFailed();
    *pcTransfers = 0;
    RTCritSectLeave(&pVPoxSCSI->CritSect);

    return rc;
}

void vpoxscsiSetRequestRedo(PVPOXSCSI pVPoxSCSI)
{
    AssertMsg(pVPoxSCSI->fBusy, ("No request to redo\n"));

    if (pVPoxSCSI->uTxDir == VPOXSCSI_TXDIR_FROM_DEVICE)
    {
        AssertPtr(pVPoxSCSI->pbBuf);
    }
}

DECLHIDDEN(int) vpoxscsiR3LoadExec(PCPDMDEVHLPR3 pHlp, PVPOXSCSI pVPoxSCSI, PSSMHANDLE pSSM)
{
    SSMR3GetU8  (pSSM, &pVPoxSCSI->regIdentify);
    SSMR3GetU8  (pSSM, &pVPoxSCSI->uTargetDevice);
    SSMR3GetU8  (pSSM, &pVPoxSCSI->uTxDir);
    SSMR3GetU8  (pSSM, &pVPoxSCSI->cbCDB);

    /*
     * The CDB buffer was increased with r104155 in trunk (backported to 5.0
     * in r104311) without bumping the SSM state versions which leaves us
     * with broken saved state restoring for older VirtualPox releases
     * (up to 5.0.10).
     */
    if (   (   SSMR3HandleRevision(pSSM) < 104311
            && SSMR3HandleVersion(pSSM)  < VPOX_FULL_VERSION_MAKE(5, 0, 12))
        || (   SSMR3HandleRevision(pSSM) < 104155
            && SSMR3HandleVersion(pSSM)  >= VPOX_FULL_VERSION_MAKE(5, 0, 51)))
    {
        memset(&pVPoxSCSI->abCDB[0], 0, sizeof(pVPoxSCSI->abCDB));
        SSMR3GetMem (pSSM, &pVPoxSCSI->abCDB[0], 12);
    }
    else
        SSMR3GetMem (pSSM, &pVPoxSCSI->abCDB[0], sizeof(pVPoxSCSI->abCDB));

    SSMR3GetU8  (pSSM, &pVPoxSCSI->iCDB);
    SSMR3GetU32 (pSSM, &pVPoxSCSI->cbBufLeft);
    SSMR3GetU32 (pSSM, &pVPoxSCSI->iBuf);
    SSMR3GetBoolV(pSSM, &pVPoxSCSI->fBusy);
    PDMDEVHLP_SSM_GET_ENUM8_RET(pHlp, pSSM, pVPoxSCSI->enmState, VPOXSCSISTATE);

    /*
     * Old saved states only save the size of the buffer left to read/write.
     * To avoid changing the saved state version we can just calculate the original
     * buffer size from the offset and remaining size.
     */
    pVPoxSCSI->cbBuf = pVPoxSCSI->cbBufLeft + pVPoxSCSI->iBuf;

    if (pVPoxSCSI->cbBuf)
    {
        pVPoxSCSI->pbBuf = (uint8_t *)RTMemAllocZ(pVPoxSCSI->cbBuf);
        if (!pVPoxSCSI->pbBuf)
            return VERR_NO_MEMORY;

        SSMR3GetMem(pSSM, pVPoxSCSI->pbBuf, pVPoxSCSI->cbBuf);
    }

    return VINF_SUCCESS;
}

DECLHIDDEN(int) vpoxscsiR3SaveExec(PCPDMDEVHLPR3 pHlp, PVPOXSCSI pVPoxSCSI, PSSMHANDLE pSSM)
{
    RT_NOREF(pHlp);
    SSMR3PutU8    (pSSM, pVPoxSCSI->regIdentify);
    SSMR3PutU8    (pSSM, pVPoxSCSI->uTargetDevice);
    SSMR3PutU8    (pSSM, pVPoxSCSI->uTxDir);
    SSMR3PutU8    (pSSM, pVPoxSCSI->cbCDB);
    SSMR3PutMem   (pSSM, pVPoxSCSI->abCDB, sizeof(pVPoxSCSI->abCDB));
    SSMR3PutU8    (pSSM, pVPoxSCSI->iCDB);
    SSMR3PutU32   (pSSM, pVPoxSCSI->cbBufLeft);
    SSMR3PutU32   (pSSM, pVPoxSCSI->iBuf);
    SSMR3PutBool  (pSSM, pVPoxSCSI->fBusy);
    SSMR3PutU8    (pSSM, pVPoxSCSI->enmState);

    if (pVPoxSCSI->cbBuf)
        SSMR3PutMem(pSSM, pVPoxSCSI->pbBuf, pVPoxSCSI->cbBuf);

    return VINF_SUCCESS;
}
