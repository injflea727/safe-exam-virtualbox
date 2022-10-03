/* $Id: VPoxMPMisc.cpp $ */
/** @file
 * VPox WDDM Miniport driver
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

#include "VPoxMPWddm.h"
#include <VPoxVideoVBE.h>
#include <iprt/param.h>
#include <stdio.h>

/* simple handle -> value table API */
NTSTATUS vpoxWddmHTableCreate(PVPOXWDDM_HTABLE pTbl, uint32_t cSize)
{
    memset(pTbl, 0, sizeof (*pTbl));
    pTbl->paData = (PVOID*)vpoxWddmMemAllocZero(sizeof (pTbl->paData[0]) * cSize);
    if (pTbl->paData)
    {
        pTbl->cSize = cSize;
        return STATUS_SUCCESS;
    }
    return STATUS_NO_MEMORY;
}

VOID vpoxWddmHTableDestroy(PVPOXWDDM_HTABLE pTbl)
{
    if (!pTbl->paData)
        return;

    vpoxWddmMemFree(pTbl->paData);
}

DECLINLINE(VPOXWDDM_HANDLE) vpoxWddmHTableIndex2Handle(uint32_t iIndex)
{
    return iIndex+1;
}

DECLINLINE(uint32_t) vpoxWddmHTableHandle2Index(VPOXWDDM_HANDLE hHandle)
{
    return hHandle-1;
}

NTSTATUS vpoxWddmHTableRealloc(PVPOXWDDM_HTABLE pTbl, uint32_t cNewSize)
{
    Assert(cNewSize > pTbl->cSize);
    if (cNewSize > pTbl->cSize)
    {
        PVOID *pvNewData = (PVOID*)vpoxWddmMemAllocZero(sizeof (pTbl->paData[0]) * cNewSize);
        if (!pvNewData)
        {
            WARN(("vpoxWddmMemAllocZero failed for size (%d)", sizeof (pTbl->paData[0]) * cNewSize));
            return STATUS_NO_MEMORY;
        }
        memcpy(pvNewData, pTbl->paData, sizeof (pTbl->paData[0]) * pTbl->cSize);
        vpoxWddmMemFree(pTbl->paData);
        pTbl->iNext2Search = pTbl->cSize;
        pTbl->cSize = cNewSize;
        pTbl->paData = pvNewData;
        return STATUS_SUCCESS;
    }
    if (cNewSize >= pTbl->cData)
    {
        AssertFailed();
        return STATUS_NOT_IMPLEMENTED;
    }
    return STATUS_INVALID_PARAMETER;

}
VPOXWDDM_HANDLE vpoxWddmHTablePut(PVPOXWDDM_HTABLE pTbl, PVOID pvData)
{
    if (pTbl->cSize == pTbl->cData)
    {
        NTSTATUS Status = vpoxWddmHTableRealloc(pTbl, pTbl->cSize + RT_MAX(10, pTbl->cSize/4));
        AssertNtStatusSuccess(Status);
        if (Status != STATUS_SUCCESS)
            return VPOXWDDM_HANDLE_INVALID;
    }
    for (UINT i = pTbl->iNext2Search; ; i = (i + 1) % pTbl->cSize)
    {
        Assert(i < pTbl->cSize);
        if (!pTbl->paData[i])
        {
            pTbl->paData[i] = pvData;
            ++pTbl->cData;
            Assert(pTbl->cData <= pTbl->cSize);
            ++pTbl->iNext2Search;
            pTbl->iNext2Search %= pTbl->cSize;
            return vpoxWddmHTableIndex2Handle(i);
        }
    }
    /* not reached */
}

PVOID vpoxWddmHTableRemove(PVPOXWDDM_HTABLE pTbl, VPOXWDDM_HANDLE hHandle)
{
    uint32_t iIndex = vpoxWddmHTableHandle2Index(hHandle);
    Assert(iIndex < pTbl->cSize);
    if (iIndex < pTbl->cSize)
    {
        PVOID pvData = pTbl->paData[iIndex];
        pTbl->paData[iIndex] = NULL;
        --pTbl->cData;
        Assert(pTbl->cData <= pTbl->cSize);
        pTbl->iNext2Search = iIndex;
        return pvData;
    }
    return NULL;
}

PVOID vpoxWddmHTableGet(PVPOXWDDM_HTABLE pTbl, VPOXWDDM_HANDLE hHandle)
{
    uint32_t iIndex = vpoxWddmHTableHandle2Index(hHandle);
    Assert(iIndex < pTbl->cSize);
    if (iIndex < pTbl->cSize)
        return pTbl->paData[iIndex];
    return NULL;
}

VOID vpoxWddmHTableIterInit(PVPOXWDDM_HTABLE pTbl, PVPOXWDDM_HTABLE_ITERATOR pIter)
{
    pIter->pTbl = pTbl;
    pIter->iCur = ~0UL;
    pIter->cLeft = pTbl->cData;
}

BOOL vpoxWddmHTableIterHasNext(PVPOXWDDM_HTABLE_ITERATOR pIter)
{
    return pIter->cLeft;
}


PVOID vpoxWddmHTableIterNext(PVPOXWDDM_HTABLE_ITERATOR pIter, VPOXWDDM_HANDLE *phHandle)
{
    if (vpoxWddmHTableIterHasNext(pIter))
    {
        for (uint32_t i = pIter->iCur+1; i < pIter->pTbl->cSize ; ++i)
        {
            if (pIter->pTbl->paData[i])
            {
                pIter->iCur = i;
                --pIter->cLeft;
                VPOXWDDM_HANDLE hHandle = vpoxWddmHTableIndex2Handle(i);
                Assert(hHandle);
                if (phHandle)
                    *phHandle = hHandle;
                return pIter->pTbl->paData[i];
            }
        }
    }

    Assert(!vpoxWddmHTableIterHasNext(pIter));
    if (phHandle)
        *phHandle = VPOXWDDM_HANDLE_INVALID;
    return NULL;
}


PVOID vpoxWddmHTableIterRemoveCur(PVPOXWDDM_HTABLE_ITERATOR pIter)
{
    VPOXWDDM_HANDLE hHandle = vpoxWddmHTableIndex2Handle(pIter->iCur);
    Assert(hHandle);
    if (hHandle)
    {
        PVOID pRet = vpoxWddmHTableRemove(pIter->pTbl, hHandle);
        Assert(pRet);
        return pRet;
    }
    return NULL;
}

NTSTATUS vpoxWddmRegQueryDrvKeyName(PVPOXMP_DEVEXT pDevExt, ULONG cbBuf, PWCHAR pBuf, PULONG pcbResult)
{
    WCHAR fallBackBuf[2];
    PWCHAR pSuffix;
    bool bFallback = false;

    if (cbBuf > sizeof(VPOXWDDM_REG_DRVKEY_PREFIX))
    {
        memcpy(pBuf, VPOXWDDM_REG_DRVKEY_PREFIX, sizeof (VPOXWDDM_REG_DRVKEY_PREFIX));
        pSuffix = pBuf + (sizeof (VPOXWDDM_REG_DRVKEY_PREFIX)-2)/2;
        cbBuf -= sizeof (VPOXWDDM_REG_DRVKEY_PREFIX)-2;
    }
    else
    {
        pSuffix = fallBackBuf;
        cbBuf = sizeof (fallBackBuf);
        bFallback = true;
    }

    NTSTATUS Status = IoGetDeviceProperty (pDevExt->pPDO,
                                  DevicePropertyDriverKeyName,
                                  cbBuf,
                                  pSuffix,
                                  &cbBuf);
    if (Status == STATUS_SUCCESS && bFallback)
        Status = STATUS_BUFFER_TOO_SMALL;
    if (Status == STATUS_BUFFER_TOO_SMALL)
        *pcbResult = cbBuf + sizeof (VPOXWDDM_REG_DRVKEY_PREFIX)-2;

    return Status;
}

NTSTATUS vpoxWddmRegQueryDisplaySettingsKeyName(PVPOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId,
        ULONG cbBuf, PWCHAR pBuf, PULONG pcbResult)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PWCHAR pSuffix;
    const WCHAR* pKeyPrefix;
    UINT cbKeyPrefix;
    UNICODE_STRING* pVGuid = vpoxWddmVGuidGet(pDevExt);
    Assert(pVGuid);
    if (!pVGuid)
        return STATUS_UNSUCCESSFUL;

    uint32_t build;
    vpoxWinVersion_t ver = VPoxQueryWinVersion(&build);
    if (ver == WINVERSION_VISTA)
    {
        pKeyPrefix = VPOXWDDM_REG_DISPLAYSETTINGSKEY_PREFIX_VISTA;
        cbKeyPrefix = sizeof (VPOXWDDM_REG_DISPLAYSETTINGSKEY_PREFIX_VISTA);
    }
    else if (ver >= WINVERSION_10 && build >= 17763)
    {
        pKeyPrefix = VPOXWDDM_REG_DISPLAYSETTINGSKEY_PREFIX_WIN10_17763;
        cbKeyPrefix = sizeof (VPOXWDDM_REG_DISPLAYSETTINGSKEY_PREFIX_WIN10_17763);
    }
    else
    {
        Assert(ver > WINVERSION_VISTA);
        pKeyPrefix = VPOXWDDM_REG_DISPLAYSETTINGSKEY_PREFIX_WIN7;
        cbKeyPrefix = sizeof (VPOXWDDM_REG_DISPLAYSETTINGSKEY_PREFIX_WIN7);
    }

    ULONG cbResult = cbKeyPrefix + pVGuid->Length + 2 + 8; // L"\\" + "XXXX"
    if (cbBuf >= cbResult)
    {
        wcscpy(pBuf, pKeyPrefix);
        pSuffix = pBuf + (cbKeyPrefix-2)/2;
        memcpy(pSuffix, pVGuid->Buffer, pVGuid->Length);
        pSuffix += pVGuid->Length/2;
        pSuffix[0] = L'\\';
        pSuffix += 1;
        swprintf(pSuffix, L"%04d", VidPnSourceId);
    }
    else
    {
        Status = STATUS_BUFFER_TOO_SMALL;
    }

    *pcbResult = cbResult;

    return Status;
}

NTSTATUS vpoxWddmRegQueryVideoGuidString(PVPOXMP_DEVEXT pDevExt, ULONG cbBuf, PWCHAR pBuf, PULONG pcbResult)
{
    BOOLEAN fNewMethodSucceeded = FALSE;
    HANDLE hKey = NULL;
    NTSTATUS Status = IoOpenDeviceRegistryKey(pDevExt->pPDO, PLUGPLAY_REGKEY_DEVICE, GENERIC_READ, &hKey);
    if (NT_SUCCESS(Status))
    {
        struct
        {
            KEY_VALUE_PARTIAL_INFORMATION Info;
            UCHAR Buf[1024]; /* should be enough */
        } KeyData;
        ULONG cbResult;
        UNICODE_STRING RtlStr;
        RtlInitUnicodeString(&RtlStr, L"VideoID");
        Status = ZwQueryValueKey(hKey,
                    &RtlStr,
                    KeyValuePartialInformation,
                    &KeyData.Info,
                    sizeof(KeyData),
                    &cbResult);
        if (NT_SUCCESS(Status))
        {
            if (KeyData.Info.Type == REG_SZ)
            {
                fNewMethodSucceeded = TRUE;
                *pcbResult = KeyData.Info.DataLength + 2;
                if (cbBuf >= KeyData.Info.DataLength)
                {
                    memcpy(pBuf, KeyData.Info.Data, KeyData.Info.DataLength + 2);
                    Status = STATUS_SUCCESS;
                }
                else
                    Status = STATUS_BUFFER_TOO_SMALL;
            }
        }
        else
        {
            WARN(("ZwQueryValueKey failed, Status 0x%x", Status));
        }

        NTSTATUS rcNt2 = ZwClose(hKey);
        AssertNtStatusSuccess(rcNt2);
    }
    else
    {
        WARN(("IoOpenDeviceRegistryKey failed Status 0x%x", Status));
    }

    if (fNewMethodSucceeded)
        return Status;
    else
        WARN(("failed to acquire the VideoID, falling back to the old impl"));

    Status = vpoxWddmRegOpenKey(&hKey, VPOXWDDM_REG_DISPLAYSETTINGSVIDEOKEY, GENERIC_READ);
    //AssertNtStatusSuccess(Status);
    if (Status == STATUS_SUCCESS)
    {
        struct
        {
            KEY_BASIC_INFORMATION Name;
            WCHAR Buf[256];
        } Buf;
        WCHAR KeyBuf[sizeof (VPOXWDDM_REG_DISPLAYSETTINGSVIDEOKEY)/2 + 256 + 64];
        wcscpy(KeyBuf, VPOXWDDM_REG_DISPLAYSETTINGSVIDEOKEY);
        ULONG ResultLength;
        BOOL bFound = FALSE;
        for (ULONG i = 0; !bFound; ++i)
        {
            RtlZeroMemory(&Buf, sizeof (Buf));
            Status = ZwEnumerateKey(hKey, i, KeyBasicInformation, &Buf, sizeof (Buf), &ResultLength);
            AssertNtStatusSuccess(Status);
            /* we should not encounter STATUS_NO_MORE_ENTRIES here since this would mean we did not find our entry */
            if (Status != STATUS_SUCCESS)
                break;

            HANDLE hSubKey;
            PWCHAR pSubBuf = KeyBuf + (sizeof (VPOXWDDM_REG_DISPLAYSETTINGSVIDEOKEY) - 2)/2;
            memcpy(pSubBuf, Buf.Name.Name, Buf.Name.NameLength);
            pSubBuf += Buf.Name.NameLength/2;
            memcpy(pSubBuf, VPOXWDDM_REG_DISPLAYSETTINGSVIDEOKEY_SUBKEY, sizeof (VPOXWDDM_REG_DISPLAYSETTINGSVIDEOKEY_SUBKEY));
            Status = vpoxWddmRegOpenKey(&hSubKey, KeyBuf, GENERIC_READ);
            //AssertNtStatusSuccess(Status);
            if (Status == STATUS_SUCCESS)
            {
                struct
                {
                    KEY_VALUE_PARTIAL_INFORMATION Info;
                    UCHAR Buf[sizeof (VPOX_WDDM_DRIVERNAME)]; /* should be enough */
                } KeyData;
                ULONG cbResult;
                UNICODE_STRING RtlStr;
                RtlInitUnicodeString(&RtlStr, L"Service");
                Status = ZwQueryValueKey(hSubKey,
                            &RtlStr,
                            KeyValuePartialInformation,
                            &KeyData.Info,
                            sizeof(KeyData),
                            &cbResult);
                Assert(Status == STATUS_SUCCESS || STATUS_BUFFER_TOO_SMALL || STATUS_BUFFER_OVERFLOW);
                if (Status == STATUS_SUCCESS)
                {
                    if (KeyData.Info.Type == REG_SZ)
                    {
                        if (KeyData.Info.DataLength == sizeof (VPOX_WDDM_DRIVERNAME))
                        {
                            if (!wcscmp(VPOX_WDDM_DRIVERNAME, (PWCHAR)KeyData.Info.Data))
                            {
                                bFound = TRUE;
                                *pcbResult = Buf.Name.NameLength + 2;
                                if (cbBuf >= Buf.Name.NameLength + 2)
                                {
                                    memcpy(pBuf, Buf.Name.Name, Buf.Name.NameLength + 2);
                                }
                                else
                                {
                                    Status = STATUS_BUFFER_TOO_SMALL;
                                }
                            }
                        }
                    }
                }

                NTSTATUS rcNt2 = ZwClose(hSubKey);
                AssertNtStatusSuccess(rcNt2);
            }
            else
                break;
        }
        NTSTATUS rcNt2 = ZwClose(hKey);
        AssertNtStatusSuccess(rcNt2);
    }

    return Status;
}

NTSTATUS vpoxWddmRegOpenKeyEx(OUT PHANDLE phKey, IN HANDLE hRootKey, IN PWCHAR pName, IN ACCESS_MASK fAccess)
{
    OBJECT_ATTRIBUTES ObjAttr;
    UNICODE_STRING RtlStr;

    RtlInitUnicodeString(&RtlStr, pName);
    InitializeObjectAttributes(&ObjAttr, &RtlStr, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, hRootKey, NULL);

    return ZwOpenKey(phKey, fAccess, &ObjAttr);
}

NTSTATUS vpoxWddmRegOpenKey(OUT PHANDLE phKey, IN PWCHAR pName, IN ACCESS_MASK fAccess)
{
    return vpoxWddmRegOpenKeyEx(phKey, NULL, pName, fAccess);
}

NTSTATUS vpoxWddmRegOpenDisplaySettingsKey(IN PVPOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId,
                                           OUT PHANDLE phKey)
{
    WCHAR Buf[512];
    ULONG cbBuf = sizeof(Buf);
    NTSTATUS Status = vpoxWddmRegQueryDisplaySettingsKeyName(pDevExt, VidPnSourceId, cbBuf, Buf, &cbBuf);
    AssertNtStatusSuccess(Status);
    if (Status == STATUS_SUCCESS)
    {
        Status = vpoxWddmRegOpenKey(phKey, Buf, GENERIC_READ);
        AssertNtStatusSuccess(Status);
        if(Status == STATUS_SUCCESS)
            return STATUS_SUCCESS;
    }

    /* fall-back to make the subsequent VPoxVideoCmnRegXxx calls treat the fail accordingly
     * basically needed to make as less modifications to the current XPDM code as possible */
    *phKey = NULL;

    return Status;
}

NTSTATUS vpoxWddmRegDisplaySettingsQueryRelX(HANDLE hKey, int * pResult)
{
    DWORD dwVal;
    NTSTATUS Status = vpoxWddmRegQueryValueDword(hKey, VPOXWDDM_REG_DISPLAYSETTINGS_ATTACH_RELX, &dwVal);
    AssertNtStatusSuccess(Status);
    if (Status == STATUS_SUCCESS)
    {
        *pResult = (int)dwVal;
    }

    return Status;
}

NTSTATUS vpoxWddmRegDisplaySettingsQueryRelY(HANDLE hKey, int * pResult)
{
    DWORD dwVal;
    NTSTATUS Status = vpoxWddmRegQueryValueDword(hKey, VPOXWDDM_REG_DISPLAYSETTINGS_ATTACH_RELY, &dwVal);
    AssertNtStatusSuccess(Status);
    if (Status == STATUS_SUCCESS)
    {
        *pResult = (int)dwVal;
    }

    return Status;
}

NTSTATUS vpoxWddmDisplaySettingsQueryPos(IN PVPOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, POINT * pPos)
{
    Assert(KeGetCurrentIrql() == PASSIVE_LEVEL);
    HANDLE hKey;
    NTSTATUS Status = vpoxWddmRegOpenDisplaySettingsKey(pDevExt, VidPnSourceId, &hKey);
    //AssertNtStatusSuccess(Status);
    if (Status == STATUS_SUCCESS)
    {
        int x, y;
        Status = vpoxWddmRegDisplaySettingsQueryRelX(hKey, &x);
        AssertNtStatusSuccess(Status);
        if (Status == STATUS_SUCCESS)
        {
            Status = vpoxWddmRegDisplaySettingsQueryRelY(hKey, &y);
            AssertNtStatusSuccess(Status);
            if (Status == STATUS_SUCCESS)
            {
                pPos->x = x;
                pPos->y = y;
            }
        }
        NTSTATUS rcNt2 = ZwClose(hKey);
        AssertNtStatusSuccess(rcNt2);
    }

    return Status;
}

void vpoxWddmDisplaySettingsCheckPos(IN PVPOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId)
{
    POINT Pos = {0};
    NTSTATUS Status = vpoxWddmDisplaySettingsQueryPos(pDevExt, VidPnSourceId, &Pos);
    if (!NT_SUCCESS(Status))
    {
        Log(("vpoxWddmDisplaySettingsQueryPos failed %#x", Status));
        return;
    }

    PVPOXWDDM_SOURCE pSource = &pDevExt->aSources[VidPnSourceId];

    if (!memcmp(&pSource->VScreenPos, &Pos, sizeof (Pos)))
        return;

    pSource->VScreenPos = Pos;
    pSource->u8SyncState &= ~VPOXWDDM_HGSYNC_F_SYNCED_DIMENSIONS;

    vpoxWddmGhDisplayCheckSetInfoFromSource(pDevExt, pSource);
}

NTSTATUS vpoxWddmRegDrvFlagsSet(PVPOXMP_DEVEXT pDevExt, DWORD fVal)
{
    HANDLE hKey = NULL;
    NTSTATUS Status = IoOpenDeviceRegistryKey(pDevExt->pPDO, PLUGPLAY_REGKEY_DRIVER, GENERIC_WRITE, &hKey);
    if (!NT_SUCCESS(Status))
    {
        WARN(("IoOpenDeviceRegistryKey failed, Status = 0x%x", Status));
        return Status;
    }

    Status = vpoxWddmRegSetValueDword(hKey, VPOXWDDM_REG_DRV_FLAGS_NAME, fVal);
    if (!NT_SUCCESS(Status))
        WARN(("vpoxWddmRegSetValueDword failed, Status = 0x%x", Status));

    NTSTATUS rcNt2 = ZwClose(hKey);
    AssertNtStatusSuccess(rcNt2);

    return Status;
}

DWORD vpoxWddmRegDrvFlagsGet(PVPOXMP_DEVEXT pDevExt, DWORD fDefault)
{
    HANDLE hKey = NULL;
    NTSTATUS Status = IoOpenDeviceRegistryKey(pDevExt->pPDO, PLUGPLAY_REGKEY_DRIVER, GENERIC_READ, &hKey);
    if (!NT_SUCCESS(Status))
    {
        WARN(("IoOpenDeviceRegistryKey failed, Status = 0x%x", Status));
        return fDefault;
    }

    DWORD dwVal = 0;
    Status = vpoxWddmRegQueryValueDword(hKey, VPOXWDDM_REG_DRV_FLAGS_NAME, &dwVal);
    if (!NT_SUCCESS(Status))
    {
        WARN(("vpoxWddmRegQueryValueDword failed, Status = 0x%x", Status));
        dwVal = fDefault;
    }

    NTSTATUS rcNt2 = ZwClose(hKey);
    AssertNtStatusSuccess(rcNt2);

    return dwVal;
}

NTSTATUS vpoxWddmRegQueryValueDword(IN HANDLE hKey, IN PWCHAR pName, OUT PDWORD pDword)
{
    struct
    {
        KEY_VALUE_PARTIAL_INFORMATION Info;
        UCHAR Buf[32]; /* should be enough */
    } Buf;
    ULONG cbBuf;
    UNICODE_STRING RtlStr;
    RtlInitUnicodeString(&RtlStr, pName);
    NTSTATUS Status = ZwQueryValueKey(hKey,
                &RtlStr,
                KeyValuePartialInformation,
                &Buf.Info,
                sizeof(Buf),
                &cbBuf);
    if (Status == STATUS_SUCCESS)
    {
        if (Buf.Info.Type == REG_DWORD)
        {
            Assert(Buf.Info.DataLength == 4);
            *pDword = *((PULONG)Buf.Info.Data);
            return STATUS_SUCCESS;
        }
    }

    return STATUS_INVALID_PARAMETER;
}

NTSTATUS vpoxWddmRegSetValueDword(IN HANDLE hKey, IN PWCHAR pName, IN DWORD val)
{
    UNICODE_STRING RtlStr;
    RtlInitUnicodeString(&RtlStr, pName);
    return ZwSetValueKey(hKey, &RtlStr,
            NULL, /* IN ULONG  TitleIndex  OPTIONAL, reserved */
            REG_DWORD,
            &val,
            sizeof(val));
}

UNICODE_STRING* vpoxWddmVGuidGet(PVPOXMP_DEVEXT pDevExt)
{
    if (pDevExt->VideoGuid.Buffer)
        return &pDevExt->VideoGuid;

    Assert(KeGetCurrentIrql() == PASSIVE_LEVEL);
    WCHAR VideoGuidBuf[512];
    ULONG cbVideoGuidBuf = sizeof (VideoGuidBuf);
    NTSTATUS Status = vpoxWddmRegQueryVideoGuidString(pDevExt ,cbVideoGuidBuf, VideoGuidBuf, &cbVideoGuidBuf);
    AssertNtStatusSuccess(Status);
    if (Status == STATUS_SUCCESS)
    {
        PWCHAR pBuf = (PWCHAR)vpoxWddmMemAllocZero(cbVideoGuidBuf);
        Assert(pBuf);
        if (pBuf)
        {
            memcpy(pBuf, VideoGuidBuf, cbVideoGuidBuf);
            RtlInitUnicodeString(&pDevExt->VideoGuid, pBuf);
            return &pDevExt->VideoGuid;
        }
    }

    return NULL;
}

VOID vpoxWddmVGuidFree(PVPOXMP_DEVEXT pDevExt)
{
    if (pDevExt->VideoGuid.Buffer)
    {
        vpoxWddmMemFree(pDevExt->VideoGuid.Buffer);
        pDevExt->VideoGuid.Buffer = NULL;
    }
}

/* mm */

NTSTATUS vpoxMmInit(PVPOXWDDM_MM pMm, UINT cPages)
{
    UINT cbBuffer = VPOXWDDM_ROUNDBOUND(cPages, 8) >> 3;
    cbBuffer = VPOXWDDM_ROUNDBOUND(cbBuffer, 4);
    PULONG pBuf = (PULONG)vpoxWddmMemAllocZero(cbBuffer);
    if (!pBuf)
    {
        Assert(0);
        return STATUS_NO_MEMORY;
    }
    RtlInitializeBitMap(&pMm->BitMap, pBuf, cPages);
    pMm->cPages = cPages;
    pMm->cAllocs = 0;
    pMm->pBuffer = pBuf;
    return STATUS_SUCCESS;
}

ULONG vpoxMmAlloc(PVPOXWDDM_MM pMm, UINT cPages)
{
    ULONG iPage = RtlFindClearBitsAndSet(&pMm->BitMap, cPages, 0);
    if (iPage == 0xFFFFFFFF)
    {
        Assert(0);
        return VPOXWDDM_MM_VOID;
    }

    ++pMm->cAllocs;
    return iPage;
}

VOID vpoxMmFree(PVPOXWDDM_MM pMm, UINT iPage, UINT cPages)
{
    Assert(RtlAreBitsSet(&pMm->BitMap, iPage, cPages));
    RtlClearBits(&pMm->BitMap, iPage, cPages);
    --pMm->cAllocs;
    Assert(pMm->cAllocs < UINT32_MAX);
}

NTSTATUS vpoxMmTerm(PVPOXWDDM_MM pMm)
{
    Assert(!pMm->cAllocs);
    vpoxWddmMemFree(pMm->pBuffer);
    pMm->pBuffer = NULL;
    return STATUS_SUCCESS;
}



typedef struct VPOXVIDEOCM_ALLOC
{
    VPOXWDDM_HANDLE hGlobalHandle;
    uint32_t offData;
    uint32_t cbData;
} VPOXVIDEOCM_ALLOC, *PVPOXVIDEOCM_ALLOC;

typedef struct VPOXVIDEOCM_ALLOC_REF
{
    PVPOXVIDEOCM_ALLOC_CONTEXT pContext;
    VPOXWDDM_HANDLE hSessionHandle;
    PVPOXVIDEOCM_ALLOC pAlloc;
    PKEVENT pSynchEvent;
    VPOXUHGSMI_BUFFER_TYPE_FLAGS fUhgsmiType;
    volatile uint32_t cRefs;
    PVOID pvUm;
    MDL Mdl;
} VPOXVIDEOCM_ALLOC_REF, *PVPOXVIDEOCM_ALLOC_REF;


NTSTATUS vpoxVideoCmAllocAlloc(PVPOXVIDEOCM_ALLOC_MGR pMgr, PVPOXVIDEOCM_ALLOC pAlloc)
{
    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    UINT cbSize = pAlloc->cbData;
    UINT cPages = BYTES_TO_PAGES(cbSize);
    ExAcquireFastMutex(&pMgr->Mutex);
    UINT iPage = vpoxMmAlloc(&pMgr->Mm, cPages);
    if (iPage != VPOXWDDM_MM_VOID)
    {
        uint32_t offData = pMgr->offData + (iPage << PAGE_SHIFT);
        Assert(offData + cbSize <= pMgr->offData + pMgr->cbData);
        pAlloc->offData = offData;
        pAlloc->hGlobalHandle = vpoxWddmHTablePut(&pMgr->AllocTable, pAlloc);
        ExReleaseFastMutex(&pMgr->Mutex);
        if (VPOXWDDM_HANDLE_INVALID != pAlloc->hGlobalHandle)
            return STATUS_SUCCESS;

        Assert(0);
        Status = STATUS_NO_MEMORY;
        vpoxMmFree(&pMgr->Mm, iPage, cPages);
    }
    else
    {
        Assert(0);
        ExReleaseFastMutex(&pMgr->Mutex);
        Status = STATUS_INSUFFICIENT_RESOURCES;
    }
    return Status;
}

VOID vpoxVideoCmAllocDealloc(PVPOXVIDEOCM_ALLOC_MGR pMgr, PVPOXVIDEOCM_ALLOC pAlloc)
{
    UINT cbSize = pAlloc->cbData;
    UINT cPages = BYTES_TO_PAGES(cbSize);
    UINT iPage = BYTES_TO_PAGES(pAlloc->offData - pMgr->offData);
    ExAcquireFastMutex(&pMgr->Mutex);
    vpoxWddmHTableRemove(&pMgr->AllocTable, pAlloc->hGlobalHandle);
    vpoxMmFree(&pMgr->Mm, iPage, cPages);
    ExReleaseFastMutex(&pMgr->Mutex);
}


NTSTATUS vpoxVideoAMgrAllocCreate(PVPOXVIDEOCM_ALLOC_MGR pMgr, UINT cbSize, PVPOXVIDEOCM_ALLOC *ppAlloc)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PVPOXVIDEOCM_ALLOC pAlloc = (PVPOXVIDEOCM_ALLOC)vpoxWddmMemAllocZero(sizeof (*pAlloc));
    if (pAlloc)
    {
        pAlloc->cbData = cbSize;
        Status = vpoxVideoCmAllocAlloc(pMgr, pAlloc);
        if (Status == STATUS_SUCCESS)
        {
            *ppAlloc = pAlloc;
            return STATUS_SUCCESS;
        }

        Assert(0);
        vpoxWddmMemFree(pAlloc);
    }
    else
    {
        Assert(0);
        Status = STATUS_NO_MEMORY;
    }

    return Status;
}

VOID vpoxVideoAMgrAllocDestroy(PVPOXVIDEOCM_ALLOC_MGR pMgr, PVPOXVIDEOCM_ALLOC pAlloc)
{
    vpoxVideoCmAllocDealloc(pMgr, pAlloc);
    vpoxWddmMemFree(pAlloc);
}

NTSTATUS vpoxVideoAMgrCtxAllocMap(PVPOXVIDEOCM_ALLOC_CONTEXT pContext, PVPOXVIDEOCM_ALLOC pAlloc, PVPOXVIDEOCM_UM_ALLOC pUmAlloc)
{
    PVPOXVIDEOCM_ALLOC_MGR pMgr = pContext->pMgr;
    NTSTATUS Status = STATUS_SUCCESS;
    PKEVENT pSynchEvent = NULL;

    if (pUmAlloc->hSynch)
    {
        Status = ObReferenceObjectByHandle((HANDLE)pUmAlloc->hSynch, EVENT_MODIFY_STATE, *ExEventObjectType, UserMode,
                (PVOID*)&pSynchEvent,
                NULL);
        AssertNtStatusSuccess(Status);
        Assert(pSynchEvent);
    }

    if (Status == STATUS_SUCCESS)
    {
        PVOID BaseVa = pMgr->pvData + pAlloc->offData - pMgr->offData;
        SIZE_T cbLength = pAlloc->cbData;

        PVPOXVIDEOCM_ALLOC_REF pAllocRef;
        pAllocRef = (PVPOXVIDEOCM_ALLOC_REF)vpoxWddmMemAllocZero(  sizeof(*pAllocRef)
                                                                 +   sizeof(PFN_NUMBER)
                                                                   * ADDRESS_AND_SIZE_TO_SPAN_PAGES(BaseVa, cbLength));
        if (pAllocRef)
        {
            pAllocRef->cRefs = 1;
            MmInitializeMdl(&pAllocRef->Mdl, BaseVa, cbLength);
            __try
            {
                MmProbeAndLockPages(&pAllocRef->Mdl, KernelMode, IoWriteAccess);
            }
            __except(EXCEPTION_EXECUTE_HANDLER)
            {
                Assert(0);
                Status = STATUS_UNSUCCESSFUL;
            }

            if (Status == STATUS_SUCCESS)
            {
                PVOID pvUm = MmMapLockedPagesSpecifyCache(&pAllocRef->Mdl, UserMode, MmNonCached,
                          NULL, /* PVOID BaseAddress */
                          FALSE, /* ULONG BugCheckOnFailure */
                          NormalPagePriority);
                if (pvUm)
                {
                    pAllocRef->pvUm = pvUm;
                    pAllocRef->pContext = pContext;
                    pAllocRef->pAlloc = pAlloc;
                    pAllocRef->fUhgsmiType = pUmAlloc->fUhgsmiType;
                    pAllocRef->pSynchEvent = pSynchEvent;
                    ExAcquireFastMutex(&pContext->Mutex);
                    pAllocRef->hSessionHandle = vpoxWddmHTablePut(&pContext->AllocTable, pAllocRef);
                    ExReleaseFastMutex(&pContext->Mutex);
                    if (VPOXWDDM_HANDLE_INVALID != pAllocRef->hSessionHandle)
                    {
                        pUmAlloc->hAlloc = pAllocRef->hSessionHandle;
                        pUmAlloc->cbData = pAlloc->cbData;
                        pUmAlloc->pvData = (uintptr_t)pvUm;
                        return STATUS_SUCCESS;
                    }

                    MmUnmapLockedPages(pvUm, &pAllocRef->Mdl);
                }
                else
                {
                    Assert(0);
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                }

                MmUnlockPages(&pAllocRef->Mdl);
            }

            vpoxWddmMemFree(pAllocRef);
        }
        else
        {
            Assert(0);
            Status = STATUS_NO_MEMORY;
        }

        if (pSynchEvent)
            ObDereferenceObject(pSynchEvent);
    }
    else
    {
        Assert(0);
    }


    return Status;
}

NTSTATUS vpoxVideoAMgrCtxAllocUnmap(PVPOXVIDEOCM_ALLOC_CONTEXT pContext, VPOXDISP_KMHANDLE hSesionHandle,
                                    PVPOXVIDEOCM_ALLOC *ppAlloc)
{
    NTSTATUS Status = STATUS_SUCCESS;
    ExAcquireFastMutex(&pContext->Mutex);
    PVPOXVIDEOCM_ALLOC_REF pAllocRef = (PVPOXVIDEOCM_ALLOC_REF)vpoxWddmHTableRemove(&pContext->AllocTable, hSesionHandle);
    ExReleaseFastMutex(&pContext->Mutex);
    if (pAllocRef)
    {
        /* wait for the dereference, i.e. for all commands involving this allocation to complete */
        vpoxWddmCounterU32Wait(&pAllocRef->cRefs, 1);

        MmUnmapLockedPages(pAllocRef->pvUm, &pAllocRef->Mdl);

        MmUnlockPages(&pAllocRef->Mdl);
        *ppAlloc = pAllocRef->pAlloc;
        if (pAllocRef->pSynchEvent)
            ObDereferenceObject(pAllocRef->pSynchEvent);
        vpoxWddmMemFree(pAllocRef);
    }
    else
    {
        Assert(0);
        Status = STATUS_INVALID_PARAMETER;
    }

    return Status;
}

static PVPOXVIDEOCM_ALLOC_REF vpoxVideoAMgrCtxAllocRefAcquire(PVPOXVIDEOCM_ALLOC_CONTEXT pContext,
                                                              VPOXDISP_KMHANDLE hSesionHandle)
{
    ExAcquireFastMutex(&pContext->Mutex);
    PVPOXVIDEOCM_ALLOC_REF pAllocRef = (PVPOXVIDEOCM_ALLOC_REF)vpoxWddmHTableGet(&pContext->AllocTable, hSesionHandle);
    if (pAllocRef)
        ASMAtomicIncU32(&pAllocRef->cRefs);
    ExReleaseFastMutex(&pContext->Mutex);
    return pAllocRef;
}

static VOID vpoxVideoAMgrCtxAllocRefRelease(PVPOXVIDEOCM_ALLOC_REF pRef)
{
    uint32_t cRefs = ASMAtomicDecU32(&pRef->cRefs);
    Assert(cRefs < UINT32_MAX/2);
    Assert(cRefs >= 1); /* we do not do cleanup-on-zero here, instead we wait for the cRefs to reach 1 in
                           vpoxVideoAMgrCtxAllocUnmap before unmapping */
    NOREF(cRefs);
}



NTSTATUS vpoxVideoAMgrCtxAllocCreate(PVPOXVIDEOCM_ALLOC_CONTEXT pContext, PVPOXVIDEOCM_UM_ALLOC pUmAlloc)
{
    PVPOXVIDEOCM_ALLOC pAlloc;
    PVPOXVIDEOCM_ALLOC_MGR pMgr = pContext->pMgr;
    NTSTATUS Status = vpoxVideoAMgrAllocCreate(pMgr, pUmAlloc->cbData, &pAlloc);
    if (Status == STATUS_SUCCESS)
    {
        Status = vpoxVideoAMgrCtxAllocMap(pContext, pAlloc, pUmAlloc);
        if (Status == STATUS_SUCCESS)
            return STATUS_SUCCESS;
        else
        {
            Assert(0);
        }
        vpoxVideoAMgrAllocDestroy(pMgr, pAlloc);
    }
    else
    {
        Assert(0);
    }
    return Status;
}

NTSTATUS vpoxVideoAMgrCtxAllocDestroy(PVPOXVIDEOCM_ALLOC_CONTEXT pContext, VPOXDISP_KMHANDLE hSesionHandle)
{
    PVPOXVIDEOCM_ALLOC pAlloc;
    PVPOXVIDEOCM_ALLOC_MGR pMgr = pContext->pMgr;
    NTSTATUS Status = vpoxVideoAMgrCtxAllocUnmap(pContext, hSesionHandle, &pAlloc);
    if (Status == STATUS_SUCCESS)
    {
        vpoxVideoAMgrAllocDestroy(pMgr, pAlloc);
    }
    else
    {
        Assert(0);
    }
    return Status;
}

NTSTATUS vpoxVideoAMgrCreate(PVPOXMP_DEVEXT pDevExt, PVPOXVIDEOCM_ALLOC_MGR pMgr, uint32_t offData, uint32_t cbData)
{
    Assert(!(offData & (PAGE_SIZE -1)));
    Assert(!(cbData & (PAGE_SIZE -1)));
    offData = VPOXWDDM_ROUNDBOUND(offData, PAGE_SIZE);
    cbData &= (~(PAGE_SIZE -1));
    Assert(cbData);
    if (!cbData)
        return STATUS_INVALID_PARAMETER;

    ExInitializeFastMutex(&pMgr->Mutex);
    NTSTATUS Status = vpoxWddmHTableCreate(&pMgr->AllocTable, 64);
    AssertNtStatusSuccess(Status);
    if (Status == STATUS_SUCCESS)
    {
        Status = vpoxMmInit(&pMgr->Mm, BYTES_TO_PAGES(cbData));
        AssertNtStatusSuccess(Status);
        if (Status == STATUS_SUCCESS)
        {
            PHYSICAL_ADDRESS PhysicalAddress = {0};
            PhysicalAddress.QuadPart = VPoxCommonFromDeviceExt(pDevExt)->phVRAM.QuadPart + offData;
            pMgr->pvData = (uint8_t*)MmMapIoSpace(PhysicalAddress, cbData, MmNonCached);
            Assert(pMgr->pvData);
            if (pMgr->pvData)
            {
                pMgr->offData = offData;
                pMgr->cbData = cbData;
                return STATUS_SUCCESS;
            }
            else
            {
                Status = STATUS_UNSUCCESSFUL;
            }
            vpoxMmTerm(&pMgr->Mm);
        }
        vpoxWddmHTableDestroy(&pMgr->AllocTable);
    }

    return Status;
}

NTSTATUS vpoxVideoAMgrDestroy(PVPOXMP_DEVEXT pDevExt, PVPOXVIDEOCM_ALLOC_MGR pMgr)
{
    RT_NOREF(pDevExt);
    MmUnmapIoSpace(pMgr->pvData, pMgr->cbData);
    vpoxMmTerm(&pMgr->Mm);
    vpoxWddmHTableDestroy(&pMgr->AllocTable);
    return STATUS_SUCCESS;
}

NTSTATUS vpoxVideoAMgrCtxCreate(PVPOXVIDEOCM_ALLOC_MGR pMgr, PVPOXVIDEOCM_ALLOC_CONTEXT pCtx)
{
    NTSTATUS Status = STATUS_NOT_SUPPORTED;
    if (pMgr->pvData)
    {
        ExInitializeFastMutex(&pCtx->Mutex);
        Status = vpoxWddmHTableCreate(&pCtx->AllocTable, 32);
        AssertNtStatusSuccess(Status);
        if (Status == STATUS_SUCCESS)
        {
            pCtx->pMgr = pMgr;
            return STATUS_SUCCESS;
        }
    }
    return Status;
}

NTSTATUS vpoxVideoAMgrCtxDestroy(PVPOXVIDEOCM_ALLOC_CONTEXT pCtx)
{
    if (!pCtx->pMgr)
        return STATUS_SUCCESS;

    VPOXWDDM_HTABLE_ITERATOR Iter;
    NTSTATUS Status = STATUS_SUCCESS;

    vpoxWddmHTableIterInit(&pCtx->AllocTable, &Iter);
    do
    {
        PVPOXVIDEOCM_ALLOC_REF pRef = (PVPOXVIDEOCM_ALLOC_REF)vpoxWddmHTableIterNext(&Iter, NULL);
        if (!pRef)
            break;

        Assert(0);

        Status = vpoxVideoAMgrCtxAllocDestroy(pCtx, pRef->hSessionHandle);
        AssertNtStatusSuccess(Status);
        if (Status != STATUS_SUCCESS)
            break;
        //        vpoxWddmHTableIterRemoveCur(&Iter);
    } while (1);

    if (Status == STATUS_SUCCESS)
    {
        vpoxWddmHTableDestroy(&pCtx->AllocTable);
    }

    return Status;
}


VOID vpoxWddmSleep(uint32_t u32Val)
{
    RT_NOREF(u32Val);
    LARGE_INTEGER Interval;
    Interval.QuadPart = -(int64_t) 2 /* ms */ * 10000;

    KeDelayExecutionThread(KernelMode, FALSE, &Interval);
}

VOID vpoxWddmCounterU32Wait(uint32_t volatile * pu32, uint32_t u32Val)
{
    LARGE_INTEGER Interval;
    Interval.QuadPart = -(int64_t) 2 /* ms */ * 10000;
    uint32_t u32CurVal;

    Assert(KeGetCurrentIrql() < DISPATCH_LEVEL);

    while ((u32CurVal = ASMAtomicReadU32(pu32)) != u32Val)
    {
        Assert(u32CurVal >= u32Val);
        Assert(u32CurVal < UINT32_MAX/2);

        KeDelayExecutionThread(KernelMode, FALSE, &Interval);
    }
}

/* dump user-mode driver debug info */
static char    g_aVPoxUmdD3DCAPS9[304];
static VPOXDISPIFESCAPE_DBGDUMPBUF_FLAGS g_VPoxUmdD3DCAPS9Flags;
static BOOLEAN g_bVPoxUmdD3DCAPS9IsInited = FALSE;

static void vpoxUmdDumpDword(DWORD *pvData, DWORD cData)
{
    char aBuf[16*4];
    DWORD dw1, dw2, dw3, dw4;
    for (UINT i = 0; i < (cData & (~3)); i+=4)
    {
        dw1 = *pvData++;
        dw2 = *pvData++;
        dw3 = *pvData++;
        dw4 = *pvData++;
        sprintf(aBuf, "0x%08x, 0x%08x, 0x%08x, 0x%08x,\n", dw1, dw2, dw3, dw4);
        LOGREL(("%s", aBuf));
    }

    cData = cData % 4;
    switch (cData)
    {
        case 3:
            dw1 = *pvData++;
            dw2 = *pvData++;
            dw3 = *pvData++;
            sprintf(aBuf, "0x%08x, 0x%08x, 0x%08x\n", dw1, dw2, dw3);
            LOGREL(("%s", aBuf));
            break;
        case 2:
            dw1 = *pvData++;
            dw2 = *pvData++;
            sprintf(aBuf, "0x%08x, 0x%08x\n", dw1, dw2);
            LOGREL(("%s", aBuf));
            break;
        case 1:
            dw1 = *pvData++;
            sprintf(aBuf, "0x%8x\n", dw1);
            LOGREL(("%s", aBuf));
            break;
        default:
            break;
    }
}

static void vpoxUmdDumpD3DCAPS9(void *pvData, PVPOXDISPIFESCAPE_DBGDUMPBUF_FLAGS pFlags)
{
    AssertCompile(!(sizeof (g_aVPoxUmdD3DCAPS9) % sizeof (DWORD)));
    LOGREL(("*****Start Dumping D3DCAPS9:*******"));
    LOGREL(("WoW64 flag(%d)", (UINT)pFlags->WoW64));
    vpoxUmdDumpDword((DWORD*)pvData, sizeof (g_aVPoxUmdD3DCAPS9) / sizeof (DWORD));
    LOGREL(("*****End Dumping D3DCAPS9**********"));
}

NTSTATUS vpoxUmdDumpBuf(PVPOXDISPIFESCAPE_DBGDUMPBUF pBuf, uint32_t cbBuffer)
{
    if (cbBuffer < RT_UOFFSETOF(VPOXDISPIFESCAPE_DBGDUMPBUF, aBuf[0]))
    {
        WARN(("Buffer too small"));
        return STATUS_BUFFER_TOO_SMALL;
    }

    NTSTATUS Status = STATUS_SUCCESS;
    uint32_t cbString = cbBuffer - RT_UOFFSETOF(VPOXDISPIFESCAPE_DBGDUMPBUF, aBuf[0]);
    switch (pBuf->enmType)
    {
        case VPOXDISPIFESCAPE_DBGDUMPBUF_TYPE_D3DCAPS9:
        {
            if (cbString != sizeof (g_aVPoxUmdD3DCAPS9))
            {
                WARN(("wrong caps size, expected %d, but was %d", sizeof (g_aVPoxUmdD3DCAPS9), cbString));
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            if (g_bVPoxUmdD3DCAPS9IsInited)
            {
                if (!memcmp(g_aVPoxUmdD3DCAPS9, pBuf->aBuf, sizeof (g_aVPoxUmdD3DCAPS9)))
                    break;

                WARN(("caps do not match!"));
                vpoxUmdDumpD3DCAPS9(pBuf->aBuf, &pBuf->Flags);
                break;
            }

            memcpy(g_aVPoxUmdD3DCAPS9, pBuf->aBuf, sizeof (g_aVPoxUmdD3DCAPS9));
            g_VPoxUmdD3DCAPS9Flags = pBuf->Flags;
            g_bVPoxUmdD3DCAPS9IsInited = TRUE;
            vpoxUmdDumpD3DCAPS9(pBuf->aBuf, &pBuf->Flags);
        }
        default: break; /* Shuts up MSC. */
    }

    return Status;
}

#if 0
VOID vpoxShRcTreeInit(PVPOXMP_DEVEXT pDevExt)
{
    ExInitializeFastMutex(&pDevExt->ShRcTreeMutex);
    pDevExt->ShRcTree = NULL;
}

VOID vpoxShRcTreeTerm(PVPOXMP_DEVEXT pDevExt)
{
    Assert(!pDevExt->ShRcTree);
    pDevExt->ShRcTree = NULL;
}

BOOLEAN vpoxShRcTreePut(PVPOXMP_DEVEXT pDevExt, PVPOXWDDM_ALLOCATION pAlloc)
{
    HANDLE hSharedRc = pAlloc->hSharedHandle;
    if (!hSharedRc)
    {
        WARN(("invalid call with zero shared handle!"));
        return FALSE;
    }
    pAlloc->ShRcTreeEntry.Key = (AVLPVKEY)hSharedRc;
    ExAcquireFastMutex(&pDevExt->ShRcTreeMutex);
    bool bRc = RTAvlPVInsert(&pDevExt->ShRcTree, &pAlloc->ShRcTreeEntry);
    ExReleaseFastMutex(&pDevExt->ShRcTreeMutex);
    Assert(bRc);
    return (BOOLEAN)bRc;
}

#define PVPOXWDDM_ALLOCATION_FROM_SHRCTREENODE(_p) \
    ((PVPOXWDDM_ALLOCATION)(((uint8_t*)(_p)) - RT_OFFSETOF(VPOXWDDM_ALLOCATION, ShRcTreeEntry)))

PVPOXWDDM_ALLOCATION vpoxShRcTreeGet(PVPOXMP_DEVEXT pDevExt, HANDLE hSharedRc)
{
    ExAcquireFastMutex(&pDevExt->ShRcTreeMutex);
    PAVLPVNODECORE pNode = RTAvlPVGet(&pDevExt->ShRcTree, (AVLPVKEY)hSharedRc);
    ExReleaseFastMutex(&pDevExt->ShRcTreeMutex);
    if (!pNode)
        return NULL;
    PVPOXWDDM_ALLOCATION pAlloc = PVPOXWDDM_ALLOCATION_FROM_SHRCTREENODE(pNode);
    return pAlloc;
}

BOOLEAN vpoxShRcTreeRemove(PVPOXMP_DEVEXT pDevExt, PVPOXWDDM_ALLOCATION pAlloc)
{
    HANDLE hSharedRc = pAlloc->hSharedHandle;
    if (!hSharedRc)
    {
        WARN(("invalid call with zero shared handle!"));
        return FALSE;
    }
    ExAcquireFastMutex(&pDevExt->ShRcTreeMutex);
    PAVLPVNODECORE pNode = RTAvlPVRemove(&pDevExt->ShRcTree, (AVLPVKEY)hSharedRc);
    ExReleaseFastMutex(&pDevExt->ShRcTreeMutex);
    if (!pNode)
        return NULL;
    PVPOXWDDM_ALLOCATION pRetAlloc = PVPOXWDDM_ALLOCATION_FROM_SHRCTREENODE(pNode);
    Assert(pRetAlloc == pAlloc);
    return !!pRetAlloc;
}
#endif

NTSTATUS vpoxWddmDrvCfgInit(PUNICODE_STRING pRegStr)
{
    HANDLE hKey;
    OBJECT_ATTRIBUTES ObjAttr;

    InitializeObjectAttributes(&ObjAttr, pRegStr, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    NTSTATUS Status = ZwOpenKey(&hKey, GENERIC_READ, &ObjAttr);
    if (!NT_SUCCESS(Status))
    {
        WARN(("ZwOpenKey for settings key failed, Status 0x%x", Status));
        return Status;
    }

    DWORD dwValue = 0;
    Status = vpoxWddmRegQueryValueDword(hKey, VPOXWDDM_CFG_STR_LOG_UM, &dwValue);
    if (NT_SUCCESS(Status))
        g_VPoxLogUm = dwValue;

    g_RefreshRate = 0;
    Status = vpoxWddmRegQueryValueDword(hKey, VPOXWDDM_CFG_STR_RATE, &dwValue);
    if (NT_SUCCESS(Status))
    {
        LOGREL(("WDDM: Guest refresh rate %u", dwValue));
        g_RefreshRate = dwValue;
    }

    if (g_RefreshRate == 0 || g_RefreshRate > 240)
        g_RefreshRate = VPOXWDDM_DEFAULT_REFRESH_RATE;

    ZwClose(hKey);

    return Status;
}

NTSTATUS vpoxWddmThreadCreate(PKTHREAD * ppThread, PKSTART_ROUTINE pStartRoutine, PVOID pStartContext)
{
    NTSTATUS fStatus;
    HANDLE hThread;
    OBJECT_ATTRIBUTES fObjectAttributes;

    Assert(KeGetCurrentIrql() == PASSIVE_LEVEL);

    InitializeObjectAttributes(&fObjectAttributes, NULL, OBJ_KERNEL_HANDLE,
                        NULL, NULL);

    fStatus = PsCreateSystemThread(&hThread, THREAD_ALL_ACCESS,
                        &fObjectAttributes, NULL, NULL,
                        (PKSTART_ROUTINE) pStartRoutine, pStartContext);
    if (!NT_SUCCESS(fStatus))
      return fStatus;

    ObReferenceObjectByHandle(hThread, THREAD_ALL_ACCESS, NULL,
                        KernelMode, (PVOID*) ppThread, NULL);
    ZwClose(hThread);
    return STATUS_SUCCESS;
}

static int vpoxWddmSlConfigure(PVPOXMP_DEVEXT pDevExt, uint32_t fFlags)
{
    PHGSMIGUESTCOMMANDCONTEXT pCtx = &VPoxCommonFromDeviceExt(pDevExt)->guestCtx;
    VBVASCANLINECFG *pCfg;
    int rc = VINF_SUCCESS;

    /* Allocate the IO buffer. */
    pCfg = (VBVASCANLINECFG *)VPoxHGSMIBufferAlloc(pCtx,
                                       sizeof (VBVASCANLINECFG), HGSMI_CH_VBVA,
                                       VBVA_SCANLINE_CFG);

    if (pCfg)
    {
        /* Prepare data to be sent to the host. */
        pCfg->rc    = VERR_NOT_IMPLEMENTED;
        pCfg->fFlags = fFlags;
        rc = VPoxHGSMIBufferSubmit(pCtx, pCfg);
        if (RT_SUCCESS(rc))
        {
            AssertRC(pCfg->rc);
            rc = pCfg->rc;
        }
        /* Free the IO buffer. */
        VPoxHGSMIBufferFree(pCtx, pCfg);
    }
    else
        rc = VERR_NO_MEMORY;
    return rc;
}

NTSTATUS VPoxWddmSlEnableVSyncNotification(PVPOXMP_DEVEXT pDevExt, BOOLEAN fEnable)
{
    if (!pDevExt->bVSyncTimerEnabled == !fEnable)
        return STATUS_SUCCESS;

    if (!fEnable)
    {
        KeCancelTimer(&pDevExt->VSyncTimer);
    }
    else
    {
        KeQuerySystemTime((PLARGE_INTEGER)&pDevExt->VSyncTime);

        LARGE_INTEGER DueTime;
        DueTime.QuadPart = -10000000LL / g_RefreshRate; /* 100ns units per second / Freq Hz */
        KeSetTimerEx(&pDevExt->VSyncTimer, DueTime, 1000 / g_RefreshRate, &pDevExt->VSyncDpc);
    }

    pDevExt->bVSyncTimerEnabled = !!fEnable;

    return STATUS_SUCCESS;
}

NTSTATUS VPoxWddmSlGetScanLine(PVPOXMP_DEVEXT pDevExt, DXGKARG_GETSCANLINE *pGetScanLine)
{
    Assert((UINT)VPoxCommonFromDeviceExt(pDevExt)->cDisplays > pGetScanLine->VidPnTargetId);
    VPOXWDDM_TARGET *pTarget = &pDevExt->aTargets[pGetScanLine->VidPnTargetId];
    Assert(pTarget->Size.cx);
    Assert(pTarget->Size.cy);
    if (pTarget->Size.cy)
    {
        uint32_t curScanLine = 0;
        BOOL bVBlank = FALSE;
        LARGE_INTEGER DevVSyncTime;
        DevVSyncTime.QuadPart =  ASMAtomicReadU64((volatile uint64_t*)&pDevExt->VSyncTime.QuadPart);
        LARGE_INTEGER VSyncTime;
        KeQuerySystemTime(&VSyncTime);

        if (VSyncTime.QuadPart < DevVSyncTime.QuadPart)
        {
            WARN(("vsync time is less than the one stored in device"));
            bVBlank = TRUE;
        }
        else
        {
            VSyncTime.QuadPart = VSyncTime.QuadPart - DevVSyncTime.QuadPart;
            /*
             * Check whether we are in VBlank state or actively drawing a scan line.
             * 10% of the VSync interval are dedicated to VBlank.
             *
             * Time intervals are in 100ns steps.
             */
            LARGE_INTEGER VSyncPeriod;
            VSyncPeriod.QuadPart = VSyncTime.QuadPart % (10000000LL / g_RefreshRate);
            LARGE_INTEGER VBlankStart;
            VBlankStart.QuadPart = ((10000000LL / g_RefreshRate) * 9) / 10;
            if (VSyncPeriod.QuadPart >= VBlankStart.QuadPart)
                bVBlank = TRUE;
            else
                curScanLine = (uint32_t)((pTarget->Size.cy * VSyncPeriod.QuadPart) / VBlankStart.QuadPart);
        }

        pGetScanLine->ScanLine = curScanLine;
        pGetScanLine->InVerticalBlank = bVBlank;
    }
    else
    {
        pGetScanLine->InVerticalBlank = TRUE;
        pGetScanLine->ScanLine = 0;
    }
    return STATUS_SUCCESS;
}

static BOOLEAN vpoxWddmSlVSyncIrqCb(PVOID pvContext)
{
    PVPOXMP_DEVEXT pDevExt = (PVPOXMP_DEVEXT)pvContext;
    DXGKARGCB_NOTIFY_INTERRUPT_DATA notify;
    BOOLEAN bNeedDpc = FALSE;
    for (UINT i = 0; i < (UINT)VPoxCommonFromDeviceExt(pDevExt)->cDisplays; ++i)
    {
        PVPOXWDDM_TARGET pTarget = &pDevExt->aTargets[i];
        if (pTarget->fConnected)
        {
            memset(&notify, 0, sizeof(DXGKARGCB_NOTIFY_INTERRUPT_DATA));
            notify.InterruptType = g_VPoxDisplayOnly?
                                       DXGK_INTERRUPT_DISPLAYONLY_VSYNC:
                                       DXGK_INTERRUPT_CRTC_VSYNC;
            notify.CrtcVsync.VidPnTargetId = i;
            pDevExt->u.primary.DxgkInterface.DxgkCbNotifyInterrupt(pDevExt->u.primary.DxgkInterface.DeviceHandle, &notify);
            bNeedDpc = TRUE;
        }
    }

    if (bNeedDpc)
    {
        pDevExt->u.primary.DxgkInterface.DxgkCbQueueDpc(pDevExt->u.primary.DxgkInterface.DeviceHandle);
    }

    return FALSE;
}

static VOID vpoxWddmSlVSyncDpc(
  __in      struct _KDPC *Dpc,
  __in_opt  PVOID DeferredContext,
  __in_opt  PVOID SystemArgument1,
  __in_opt  PVOID SystemArgument2
)
{
    RT_NOREF(Dpc, SystemArgument1, SystemArgument2);
    PVPOXMP_DEVEXT pDevExt = (PVPOXMP_DEVEXT)DeferredContext;
    Assert(!pDevExt->fVSyncInVBlank);
    ASMAtomicWriteU32(&pDevExt->fVSyncInVBlank, 1);

    BOOLEAN bDummy;
    NTSTATUS Status = pDevExt->u.primary.DxgkInterface.DxgkCbSynchronizeExecution(
            pDevExt->u.primary.DxgkInterface.DeviceHandle,
            vpoxWddmSlVSyncIrqCb,
            pDevExt,
            0, /* IN ULONG MessageNumber */
            &bDummy);
    if (!NT_SUCCESS(Status))
        WARN(("DxgkCbSynchronizeExecution failed Status %#x", Status));

    LARGE_INTEGER VSyncTime;
    KeQuerySystemTime(&VSyncTime);
    ASMAtomicWriteU64((volatile uint64_t*)&pDevExt->VSyncTime.QuadPart, VSyncTime.QuadPart);

    ASMAtomicWriteU32(&pDevExt->fVSyncInVBlank, 0);
}

NTSTATUS VPoxWddmSlInit(PVPOXMP_DEVEXT pDevExt)
{
    pDevExt->bVSyncTimerEnabled = FALSE;
    pDevExt->fVSyncInVBlank = 0;
    KeQuerySystemTime((PLARGE_INTEGER)&pDevExt->VSyncTime);
    KeInitializeTimer(&pDevExt->VSyncTimer);
    KeInitializeDpc(&pDevExt->VSyncDpc, vpoxWddmSlVSyncDpc, pDevExt);
    return STATUS_SUCCESS;
}

NTSTATUS VPoxWddmSlTerm(PVPOXMP_DEVEXT pDevExt)
{
    KeCancelTimer(&pDevExt->VSyncTimer);
    return STATUS_SUCCESS;
}

void vpoxWddmDiInitDefault(DXGK_DISPLAY_INFORMATION *pInfo, PHYSICAL_ADDRESS PhAddr, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId)
{
    pInfo->Width = 1024;
    pInfo->Height = 768;
    pInfo->Pitch = pInfo->Width * 4;
    pInfo->ColorFormat = D3DDDIFMT_A8R8G8B8;
    pInfo->PhysicAddress = PhAddr;
    pInfo->TargetId = VidPnSourceId;
    pInfo->AcpiId = 0;
}

void vpoxWddmDiToAllocData(PVPOXMP_DEVEXT pDevExt, const DXGK_DISPLAY_INFORMATION *pInfo, PVPOXWDDM_ALLOC_DATA pAllocData)
{
    pAllocData->SurfDesc.width = pInfo->Width;
    pAllocData->SurfDesc.height = pInfo->Height;
    pAllocData->SurfDesc.format = pInfo->ColorFormat;
    pAllocData->SurfDesc.bpp = vpoxWddmCalcBitsPerPixel(pInfo->ColorFormat);
    pAllocData->SurfDesc.pitch = pInfo->Pitch;
    pAllocData->SurfDesc.depth = 1;
    pAllocData->SurfDesc.slicePitch = pInfo->Pitch;
    pAllocData->SurfDesc.cbSize = pInfo->Pitch * pInfo->Height;
    pAllocData->SurfDesc.VidPnSourceId = pInfo->TargetId;
    pAllocData->SurfDesc.RefreshRate.Numerator = g_RefreshRate * 1000;
    pAllocData->SurfDesc.RefreshRate.Denominator = 1000;

    /* the address here is not a VRAM offset! so convert it to offset */
    vpoxWddmAddrSetVram(&pAllocData->Addr, 1,
            vpoxWddmVramAddrToOffset(pDevExt, pInfo->PhysicAddress));
}

void vpoxWddmDmSetupDefaultVramLocation(PVPOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID ModifiedVidPnSourceId,
                                        VPOXWDDM_SOURCE *paSources)
{
    PVPOXWDDM_SOURCE pSource = &paSources[ModifiedVidPnSourceId];
    AssertRelease(g_VPoxDisplayOnly);
    ULONG offVram = vpoxWddmVramCpuVisibleSegmentSize(pDevExt);
    offVram /= VPoxCommonFromDeviceExt(pDevExt)->cDisplays;
    offVram &= ~PAGE_OFFSET_MASK;
    offVram *= ModifiedVidPnSourceId;

    if (vpoxWddmAddrSetVram(&pSource->AllocData.Addr, 1, offVram))
        pSource->u8SyncState &= ~VPOXWDDM_HGSYNC_F_SYNCED_LOCATION;
}

char const *vpoxWddmAllocTypeString(PVPOXWDDM_ALLOCATION pAlloc)
{
    switch (pAlloc->enmType)
    {
        case VPOXWDDM_ALLOC_TYPE_UNEFINED:                 return "UNEFINED";
        case VPOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE: return "SHAREDPRIMARYSURFACE";
        case VPOXWDDM_ALLOC_TYPE_STD_SHADOWSURFACE:        return "SHADOWSURFACE";
        case VPOXWDDM_ALLOC_TYPE_STD_STAGINGSURFACE:       return "STAGINGSURFACE";
        case VPOXWDDM_ALLOC_TYPE_STD_GDISURFACE:           return "GDISURFACE";
        case VPOXWDDM_ALLOC_TYPE_UMD_RC_GENERIC:           return "UMD_RC_GENERIC";
        case VPOXWDDM_ALLOC_TYPE_UMD_HGSMI_BUFFER:         return "UMD_HGSMI_BUFFER";
        default: break;
    }
    AssertFailed();
    return "UNKNOWN";
}
